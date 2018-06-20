/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <windows.h>
#include <setjmp.h>
#include <signal.h>

#include "omrsignal.h"
#include "omrport.h"
#include "omrutil.h"
#include "omrutilbase.h"
#include "omrthread.h"
#include "ut_omrport.h"

/* UNWIND_CODE and UNWIND_INFO are not in the Windows SDK headers, but they are documented on MSDN */
typedef union UNWIND_CODE {
	struct {
		uint8_t CodeOffset;
		uint8_t UnwindOp : 4;
		uint8_t OpInfo   : 4;
	};
	uint16_t FrameOffset;
} UNWIND_CODE;

typedef struct UNWIND_INFO {
	uint8_t Version       : 3;
	uint8_t Flags         : 5;
	uint8_t SizeOfProlog;
	uint8_t CountOfCodes;
	uint8_t FrameRegister : 4;
	uint8_t FrameOffset   : 4;
	UNWIND_CODE UnwindCode[1];
	union {
		OPTIONAL uint32_t ExceptionHandler;
		OPTIONAL uint32_t FunctionEntry;
	};
	OPTIONAL uint32_t ExceptionData[];
} UNWIND_INFO;

typedef struct J9CurrentSignal {
	EXCEPTION_POINTERS *exceptionInfo;
	uint32_t portLibSignalType;
} J9CurrentSignal;

/* key to get the current synchronous signal */
static omrthread_tls_key_t tlsKeyCurrentSignal;
static RUNTIME_FUNCTION *j9SigProtectFunction = NULL;

struct J9SignalHandlerRecord {
	struct J9SignalHandlerRecord *previous;
	struct OMRPortLibrary *portLibrary;
	omrsig_handler_fn handler;
	void *handler_arg;
	jmp_buf returnBuf;
	uint32_t flags;
	BOOLEAN deferToTryExcept;
};

typedef struct J9WinAMD64AsyncHandlerRecord {
	OMRPortLibrary *portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct J9WinAMD64AsyncHandlerRecord *next;
} J9WinAMD64AsyncHandlerRecord;

static struct {
	uint32_t portLibSignalNo;
	int osSignalNo;
} signalMap[] = {
	{OMRPORT_SIG_FLAG_SIGSEGV, SIGSEGV},
	{OMRPORT_SIG_FLAG_SIGILL, SIGILL},
	{OMRPORT_SIG_FLAG_SIGFPE, SIGFPE},
	{OMRPORT_SIG_FLAG_SIGABRT, SIGABRT},
	{OMRPORT_SIG_FLAG_SIGTERM, SIGTERM},
	{OMRPORT_SIG_FLAG_SIGINT, SIGINT},
	{OMRPORT_SIG_FLAG_SIGBREAK, SIGBREAK},
};

#define ARRAY_SIZE_SIGNALS (NSIG + 1)

typedef void (*win_signal)(int);

/* Store the original signal handler. During shutdown, we need to restore
 * the signal handler to the original OS handler.
 */
static struct {
	win_signal originalHandler;
	uint32_t restore;
} handlerInfo[ARRAY_SIZE_SIGNALS];

/* Keep track of signal counts. */
static volatile uintptr_t signalCounts[ARRAY_SIZE_SIGNALS] = {0};

static omrthread_monitor_t masterExceptionMonitor;

/* access to this must be synchronized using masterExceptionMonitor */
static uint32_t vectoredExceptionHandlerInstalled;

/* Thread to invoke signal handlers for asynchronous signals. */
static omrthread_t asynchSignalReporterThread;

static J9WinAMD64AsyncHandlerRecord *asyncHandlerList;
static omrthread_monitor_t asyncMonitor;
static uint32_t asyncThreadCount;
static uint32_t attachedPortLibraries;
/* holds the options set by omrsig_set_options */
static uint32_t signalOptions;
static omrthread_tls_key_t tlsKey;

/* Calls to registerSignalHandlerWithOS are synchronized using registerHandlerMonitor */
static omrthread_monitor_t registerHandlerMonitor;

/* wakeUpASyncReporter semaphore coordinates between master async signal handler and
 * async signal reporter thread.
 */
static j9sem_t wakeUpASyncReporter;

/* Used to synchronize shutdown of asynchSignalReporterThread. */
static omrthread_monitor_t asyncReporterShutdownMonitor;

/* Used to indicate start and end of asynchSignalReporterThread termination. */
static uint32_t shutDownASynchReporter;

static uint32_t mapWin32ExceptionToPortlibType(uint32_t exceptionCode);
static uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForOther(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType);
static LONG WINAPI masterVectoredExceptionHandler(EXCEPTION_POINTERS *exceptionInfo);
static uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static void fillInWinAMD64SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct J9Win32SignalInfo *j9info);
static void sig_full_shutdown(struct OMRPortLibrary *portLibrary);
static void destroySignalTools(OMRPortLibrary *portLibrary);
static uint32_t addMasterVectoredExceptionHandler(struct OMRPortLibrary *portLibrary);
static int32_t initializeSignalTools(OMRPortLibrary *portLibrary);
static int structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo);
static int32_t runInTryExcept(struct OMRPortLibrary *portLibrary, omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result);

static uint32_t mapOSSignalToPortLib(uint32_t signalNo);
static int mapPortLibSignalToOSSignal(uint32_t portLibSignal);

static int32_t registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, win_signal handler, void **oldOSHandler);
static void updateSignalCount(int osSignalNo);
static void masterASynchSignalHandler(int osSignalNo);
static void removeAsyncHandlers(OMRPortLibrary *portLibrary);

#if defined(OMR_PORT_ASYNC_HANDLER)
static int J9THREAD_PROC asynchSignalReporter(void *userData);
static void runHandlers(uint32_t asyncSignalFlag);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

static int32_t registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler);

uint32_t
omrsig_info(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (category) {
	case OMRPORT_SIG_SIGNAL:
		return infoForSignal(portLibrary, info, index, name, value);
	case OMRPORT_SIG_GPR:
		return infoForGPR(portLibrary, info, index, name, value);
	case OMRPORT_SIG_CONTROL:
		return infoForControl(portLibrary, info, index, name, value);
	case OMRPORT_SIG_MODULE:
		return infoForModule(portLibrary, info, index, name, value);
	case OMRPORT_SIG_FPR:
		return infoForFPR(portLibrary, info, index, name, value);
	case OMRPORT_SIG_OTHER:
		return infoForOther(portLibrary, info, index, name, value);
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}
uint32_t
omrsig_info_count(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	return countInfoInCategory(portLibrary, info, category);
}

static int32_t
runInTryExcept(struct OMRPortLibrary *portLibrary,
			   omrsig_protected_fn fn, void *fn_arg,
			   omrsig_handler_fn handler, void *handler_arg,
			   uint32_t flags, uintptr_t *result)
{
	__try {
		*result = fn(portLibrary, fn_arg);
	} __except (structuredExceptionHandler(portLibrary, handler, handler_arg, flags, GetExceptionInformation())) {
		*result = 0;
		return OMRPORT_SIG_EXCEPTION_OCCURRED;
	}
	return OMRPORT_SIG_NO_EXCEPTION;
}

int32_t
omrsig_protect(struct OMRPortLibrary *portLibrary, omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result)
{
	struct J9SignalHandlerRecord thisRecord;
	uintptr_t rc = 0;
	omrthread_t thisThread = omrthread_self();

	thisRecord.previous = omrthread_tls_get(thisThread, tlsKey);
	thisRecord.portLibrary = portLibrary;
	thisRecord.handler = handler;
	thisRecord.handler_arg = handler_arg;
	thisRecord.flags = flags;
	thisRecord.deferToTryExcept = FALSE;

	/* omrsig_protect cannot protect with a handler that may do both OMRPORT_SIG_FLAG_MAY_RETURN and OMRPORT_SIG_FLAG_CONTINUE_EXECUTION */
	Assert_PRT_true((OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION) != (flags & (OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION)));

	if (OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS & signalOptions) {
		/* -Xrs was set, we can't protect against any signals, do not install the master handler */
		*result = fn(portLibrary, fn_arg);
		return 0;
	}

	/* Check to see if we have already registered the masterVectoredExceptionHandler.
	 * Because we are checking without acquiring the monitor first, we'll have to check again.
	 * Note that that we don't uninstall the masterHandlerMonitor, so we only have to account for it going from not installed to installed.
	*/
	if (0 == vectoredExceptionHandlerInstalled) {
		omrthread_monitor_enter(masterExceptionMonitor);
		if (0 == vectoredExceptionHandlerInstalled) {
			rc = addMasterVectoredExceptionHandler(portLibrary);
		}
		omrthread_monitor_exit(masterExceptionMonitor);

		if (rc) {
			return OMRPORT_SIG_ERROR;
		}
	}

	omrthread_tls_set(thisThread, tlsKey, &thisRecord);

	/* We can not use setjmp/longjmp to jump out of a VectoredExceptionHandler (CMVC 175576), so use _try/_except semantics instead */
	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_MAY_RETURN)) {

		J9CurrentSignal *currentSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);
		int32_t exceptionStatus = OMRPORT_SIG_NO_EXCEPTION;

		/* The VectoredExceptionHandler will get notified before the _except clause/block below,
		 * this tells the VectoredExceptionHandler to defer to this _except clause/block */
		thisRecord.deferToTryExcept = TRUE;

		/*
		 * Jazz 55704 - functions are statically marked with UNW_FLAG_EHANDLER if they contain a _try/__except block.
		 * This confuses tryExceptHandlerExistsOnStack() into seeing omrsig_protect as a _try/__except function,
		 * even when it (dynamically) isn't using it.
		 */
		exceptionStatus = runInTryExcept(portLibrary,  fn, fn_arg, handler, handler_arg, flags, result);
		if (OMRPORT_SIG_EXCEPTION_OCCURRED == exceptionStatus) {
			omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);
			omrthread_tls_set(thisThread, tlsKeyCurrentSignal, currentSignal);
			return OMRPORT_SIG_EXCEPTION_OCCURRED;
		}
		omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);

		return 0;
	}

	*result = fn(portLibrary, fn_arg);

	omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);

	return 0;
}

int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	int32_t rc = 0;
	J9WinAMD64AsyncHandlerRecord *cursor = NULL;
	J9WinAMD64AsyncHandlerRecord **previousLink = NULL;

	Trc_PRT_signal_omrsig_set_async_signal_handler_entered(handler, handler_arg, flags);

	if (OMR_ARE_ALL_BITS_SET(signalOptions, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/* Do not install any handlers if -Xrs is set. */
		Trc_PRT_signal_omrsig_set_async_signal_handler_will_not_set_handler_due_to_Xrs(handler, handler_arg, flags);
		return OMRPORT_SIG_ERROR;
	}

	omrthread_monitor_enter(registerHandlerMonitor);
	rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLASYNC, NULL);
	omrthread_monitor_exit(registerHandlerMonitor);

	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_async_signal_handler_exiting_did_nothing_possible_error(handler, handler_arg, flags);
		return rc;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* Wait until no signals are being reported. */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* Is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (NULL != cursor) {
		if ((cursor->portLib == portLibrary) && (cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
			if (0 == flags) {
				/* Remove this handler record. */
				*previousLink = cursor->next;
				portLibrary->mem_free_memory(portLibrary, cursor);
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_removed(handler, handler_arg, flags);

				/* If this is the last handler, unregister the handler function. */
				if (NULL == asyncHandlerList) {
					SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
				}
			} else {
				/* Update the listener with the new flags. */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_1(handler, handler_arg, flags);
				cursor->flags |= flags;
			}
			break;
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	if (NULL == cursor) {
		/* Cursor will only be NULL if we failed to find it in the list. */
		if (0 != flags) {
			J9WinAMD64AsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (NULL == record) {
				rc = OMRPORT_SIG_ERROR;
			} else {
				record->portLib = portLibrary;
				record->handler = handler;
				record->handler_arg = handler_arg;
				record->flags = flags;
				record->next = NULL;

				/* If this is the first handler, register the handler function. */
				if (NULL == asyncHandlerList) {
					SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
				}

				/* Add the new record to the end of the list. */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_2(handler, handler_arg, flags);
				*previousLink = record;
			}
		}
	}

	omrthread_monitor_exit(asyncMonitor);

	Trc_PRT_signal_omrsig_set_async_signal_handler_exiting(handler, handler_arg, flags);
	return rc;
}

int32_t
omrsig_set_single_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t portlibSignalFlag, void **oldOSHandler)
{
	return OMRPORT_SIG_ERROR;
}

uint32_t
omrsig_map_os_signal_to_portlib_signal(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue)
{
	return mapOSSignalToPortLib(osSignalValue);
}

int32_t
omrsig_map_portlib_signal_to_os_signal(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag)
{
	return (int32_t)mapPortLibSignalToOSSignal(portlibSignalFlag);
}

int32_t
omrsig_register_os_handler(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler)
{
	int32_t rc = 0;

	Trc_PRT_signal_omrsig_register_os_handler_entered(portlibSignalFlag, newOSHandler);

	if ((0 == portlibSignalFlag) || !OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag)) {
		/* If portlibSignalFlag is 0 or if portlibSignalFlag has multiple signal bits set, then fail. */
		Trc_PRT_signal_omrsig_register_os_handler_invalid_portlibSignalFlag(portlibSignalFlag);
		rc = OMRPORT_SIG_ERROR;
	} else {
		omrthread_monitor_enter(registerHandlerMonitor);
		rc = registerSignalHandlerWithOS(portLibrary, portlibSignalFlag, (win_signal)newOSHandler, oldOSHandler);
		omrthread_monitor_exit(registerHandlerMonitor);
	}

	if (NULL != oldOSHandler) {
		Trc_PRT_signal_omrsig_register_os_handler_exiting(rc, portlibSignalFlag, newOSHandler, *oldOSHandler);
	} else {
		Trc_PRT_signal_omrsig_register_os_handler_exiting(rc, portlibSignalFlag, newOSHandler, NULL);
	}

	return rc;
}

BOOLEAN
omrsig_is_master_signal_handler(struct OMRPortLibrary *portLibrary, void *osHandler)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_signal_omrsig_is_master_signal_handler_entered(osHandler);

	if (osHandler == (void *)masterASynchSignalHandler) {
		rc = TRUE;
	}

	Trc_PRT_signal_omrsig_is_master_signal_handler_exiting(rc);
	return rc;
}

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;

	if (0 == (signalOptions & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	if (0 == (signalOptions & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLASYNC;
	}

	if ((flags & supportedFlags) == flags) {
		return 1;
	} else {
		return 0;
	}
}


/*
 * The full shutdown routine "sig_full_shutdown" overwrites this once we've completed startup
 */
void
omrsig_shutdown(struct OMRPortLibrary *portLibrary)
{

}

/**
 * Start up the signal handling component of the port library
 */
int32_t
omrsig_startup(struct OMRPortLibrary *portLibrary)
{

	int32_t result = 0;
	ULONG64 imageBase = 0;
	uint32_t index = 1;
	UNWIND_HISTORY_TABLE unwindHistoryTable;

	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	if (omrthread_tls_alloc(&tlsKey)) {
		return OMRPORT_ERROR_STARTUP_SIGNAL;
	}

	if (omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		return OMRPORT_ERROR_STARTUP_SIGNAL;
	}

	omrthread_monitor_enter(globalMonitor);
	if (attachedPortLibraries++ == 0) {
		/* initialize handlerInfo */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			handlerInfo[index].restore = 0;
		}
		result = initializeSignalTools(portLibrary);
	}

	memset(&unwindHistoryTable, 0, sizeof(UNWIND_HISTORY_TABLE));
	j9SigProtectFunction = RtlLookupFunctionEntry((ULONG64)omrsig_protect, &imageBase, &unwindHistoryTable);

	omrthread_monitor_exit(globalMonitor);

	if (result == 0) {
		/* we have successfully started up the signal portion, install the full shutdown routine */
		portLibrary->sig_shutdown = sig_full_shutdown;
	}

	return result;
}

int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options)
{
	uintptr_t handlersInstalled = 0;

	if ((OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS) & options)  {
		/* check that we haven't already set any master handlers */

		omrthread_monitor_enter(masterExceptionMonitor);
		if (0 != vectoredExceptionHandlerInstalled) {
			handlersInstalled = 1;
		}
		omrthread_monitor_exit(masterExceptionMonitor);
	}

	if (handlersInstalled) {
		return -1;
	}

	signalOptions |= options;
	return 0;
}

/* these options should always be 0 */
uint32_t
omrsig_get_options(struct OMRPortLibrary *portLibrary)
{
	return signalOptions;
}

/**
 * sets the priority of the the async reporting thread
 *
 * In windows, a new thread is created to run the handlers each time a signal is received,
 * so this function does nothing.
*/
int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, uintptr_t priority)
{
	return 0;
}

intptr_t
omrsig_get_current_signal(struct OMRPortLibrary *portLibrary)
{
	omrthread_t thisThread = omrthread_self();
	struct J9CurrentSignal *currentSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

	if (currentSignal != NULL) {
		return currentSignal->portLibSignalType;
	}

	return 0;
}

static uint32_t
mapWin32ExceptionToPortlibType(uint32_t exceptionCode)
{
	switch (exceptionCode) {
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO;

	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO;

	case EXCEPTION_INT_OVERFLOW:
		return OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW;

	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_STACK_CHECK:
		return OMRPORT_SIG_FLAG_SIGFPE;

	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return OMRPORT_SIG_FLAG_SIGILL;

	case EXCEPTION_ACCESS_VIOLATION:
	default:
		return OMRPORT_SIG_FLAG_SIGSEGV;
	}
}

/**
 * Look for a try/except signal handler on the stack.  Do not search past omrsig_protect.
 *  @param portLibrary port library
 *  @param context Windows _CONTEXT structure containing register information
 *  @return TRUE if the there is a try/except handler registered on the stack
 */

static BOOLEAN
tryExceptHandlerExistsOnStack(OMRPortLibrary *portLibrary, CONTEXT *originalContext)
{

	KNONVOLATILE_CONTEXT_POINTERS nvContext;
	UNWIND_HISTORY_TABLE unwindHistoryTable;
	RUNTIME_FUNCTION *runtimeFunction;
	PVOID handlerData;
	ULONG64 establisherFrame;
	ULONG64 imageBase;
	UNWIND_INFO *unwindInfo;
	BOOLEAN firstFrame = TRUE;
	CONTEXT unwindContext;
	CONTEXT *context = &unwindContext;

#if !defined(UNW_FLAG_NHANDLER)
#define UNW_FLAG_NHANDLER  0x0
#endif

#if !defined(UNW_FLAG_EHANDLER)
#define UNW_FLAG_EHANDLER  0x01
#endif

#if !defined(UNW_FLAG_UHANDLER)
#define UNW_FLAG_UHANDLER  0x02
#endif

#if !defined(UNW_FLAG_CHAININFO)
#define UNW_FLAG_CHAININFO 0x04
#endif

	memset(&unwindHistoryTable, 0, sizeof(UNWIND_HISTORY_TABLE));
	memcpy(context, originalContext, sizeof(CONTEXT));

	do {

		runtimeFunction = RtlLookupFunctionEntry(context->Rip, &imageBase, &unwindHistoryTable);

		memset(&nvContext, 0 , sizeof(KNONVOLATILE_CONTEXT_POINTERS));

		if (!runtimeFunction) {
			if (!firstFrame) {
				break;
			}
			context->Rip = (ULONG64)(*(PULONG64)context->Rsp);
			context->Rsp += 8;
		} else {

			uint8_t flags;

			unwindInfo = (UNWIND_INFO *)(runtimeFunction->UnwindData + imageBase);
			flags = unwindInfo->Flags;

			if (runtimeFunction == j9SigProtectFunction) {
				return FALSE;
			} else if (UNW_FLAG_EHANDLER == flags) {
				/* we found a try/except handler */
				return TRUE;
			}

			RtlVirtualUnwind(
				UNW_FLAG_NHANDLER, /* suppresses the invocation of any unwind or exception handlers registered by the function).*/
				imageBase,
				context->Rip,
				runtimeFunction,
				context,
				&handlerData,
				&establisherFrame,
				&nvContext);
		}

		firstFrame = FALSE;

	} while (0 != context->Rip);

	/* no try/except handler was found */
	return FALSE;
}

static uint32_t
infoForOther(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {

	/* hide this from users that iterate through all of the omrsig_info() by only exposing it through OMRPORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER
	 * which is a negative value
	 */
	case OMRPORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER:
		*name = "Defer_to_Try_Except_Handler";
		info->deferToTryExcept = !info->tryExceptHandlerIgnore
								 && tryExceptHandlerExistsOnStack(portLibrary, info->ContextRecord);

		*value = &info->deferToTryExcept;
		return OMRPORT_SIG_VALUE_32;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}


static uint32_t
infoForSignal(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {
	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
	case 0:
		*name = "Windows_ExceptionCode";
		*value = &info->systemType;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_TYPE:
	case 1:
		*name = "J9Generic_Signal";
		*value = &info->portLibType;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_ADDRESS:
	case 2:
		*name = "ExceptionAddress";
		*value = &info->ExceptionRecord->ExceptionAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 3:
		*name = "ContextFlags";
		*value = &info->ContextRecord->ContextFlags;
		return OMRPORT_SIG_VALUE_32;
	case OMRPORT_SIG_SIGNAL_HANDLER:
	case 4:
		*name = "Handler1";
		*value = &info->handlerAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 5:
		*name = "Handler2";
		*value = &info->handlerAddress2;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS:
	case 6:
		/* For EXCEPTION_ACCESS_VIOLATION (very common) we can extract the address
		 * that was being read or written to and whether it was an attempt to read, write
		 * or execute the data at that address.
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363082%28v=vs.85%29.aspx */
		if (EXCEPTION_ACCESS_VIOLATION == info->ExceptionRecord->ExceptionCode) {
			if (EXCEPTION_READ_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "InaccessibleReadAddress";
			} else if (EXCEPTION_WRITE_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "InaccessibleWriteAddress";
			} else if (EXCEPTION_EXECUTE_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "DEPAddress";
			}
			*value = &info->ExceptionRecord->ExceptionInformation[1];
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}

	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

static uint32_t
infoForGPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	if (info->ContextRecord->ContextFlags & CONTEXT_INTEGER) {
		switch (index) {

		case OMRPORT_SIG_GPR_AMD64_RDI:
		case 0:
			*name = "RDI";
			*value = &info->ContextRecord->Rdi;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_RSI:
		case 1:
			*name = "RSI";
			*value = &info->ContextRecord->Rsi;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_RAX:
		case 2:
			*name = "RAX";
			*value = &info->ContextRecord->Rax;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_RBX:
		case 3:
			*name = "RBX";
			*value = &info->ContextRecord->Rbx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_RCX:
		case 4:
			*name = "RCX";
			*value = &info->ContextRecord->Rcx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_RDX:
		case 5:
			*name = "RDX";
			*value = &info->ContextRecord->Rdx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R8:
		case 6:
			*name = "R8";
			*value = &info->ContextRecord->R8;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R9:
		case 7:
			*name = "R9";
			*value = &info->ContextRecord->R9;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R10:
		case 8:
			*name = "R10";
			*value = &info->ContextRecord->R10;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R11:
		case 9:
			*name = "R11";
			*value = &info->ContextRecord->R11;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R12:
		case 10:
			*name = "R12";
			*value = &info->ContextRecord->R12;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R13:
		case 11:
			*name = "R13";
			*value = &info->ContextRecord->R13;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R14:
		case 12:
			*name = "R14";
			*value = &info->ContextRecord->R14;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_AMD64_R15:
		case 13:
			*name = "R15";
			*value = &info->ContextRecord->R15;
			return OMRPORT_SIG_VALUE_ADDRESS;

		default:
			return OMRPORT_SIG_VALUE_UNDEFINED;

		}
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

static uint32_t
infoForControl(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	if (info->ContextRecord->ContextFlags & CONTEXT_CONTROL) {
		switch (index) {

		case OMRPORT_SIG_CONTROL_PC:
		case 0:
			*name = "RIP";
			*value = &info->ContextRecord->Rip;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_CONTROL_SP:
		case 1:
			*name = "RSP";
			*value = &info->ContextRecord->Rsp;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_CONTROL_BP:
		case 2:
			*name = "RBP";
			*value = &info->ContextRecord->Rbp;
			return OMRPORT_SIG_VALUE_ADDRESS;
		}
	}

	if (info->ContextRecord->ContextFlags & CONTEXT_SEGMENTS) {
		switch (index) {
		case 3:
			*name = "GS";
			*value = &info->ContextRecord->SegGs;
			return OMRPORT_SIG_VALUE_16;
		case 4:
			*name = "FS";
			*value = &info->ContextRecord->SegFs;
			return OMRPORT_SIG_VALUE_16;
		case 5:
			*name = "ES";
			*value = &info->ContextRecord->SegEs;
			return OMRPORT_SIG_VALUE_16;
		case 6:
			*name = "DS";
			*value = &info->ContextRecord->SegDs;
			return OMRPORT_SIG_VALUE_16;
		}
	}
	return OMRPORT_SIG_VALUE_UNDEFINED;
}

static uint32_t
infoForModule(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	if (info->moduleBaseAddress == NULL) {
		MEMORY_BASIC_INFORMATION mbi;

		if (VirtualQuery((LPBYTE) info->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
			if (MEM_FREE == mbi.State)	{
				/* misc hack from Advanced Windows (Richter) */
				mbi.AllocationBase = mbi.BaseAddress;
			}

			GetModuleFileName((HINSTANCE) mbi.AllocationBase, info->moduleName, sizeof(info->moduleName));
			info->moduleBaseAddress = mbi.AllocationBase;
			info->offsetInDLL = (uintptr_t) info->ExceptionRecord->ExceptionAddress - (uintptr_t) mbi.AllocationBase;
		}
	}

	*name = "";

	switch (index) {
	case OMRPORT_SIG_MODULE_NAME:
	case 0:
		*name = "Module";
		*value = &info->moduleName;
		return OMRPORT_SIG_VALUE_STRING;
	case 1:
		*name = "Module_base_address";
		*value = &info->moduleBaseAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 2:
		*name = "Offset_in_DLL";
		*value = &info->offsetInDLL;
		return OMRPORT_SIG_VALUE_64;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

static uint32_t
countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	void *value;
	const char *name;
	uint32_t count = 0;

	while (portLibrary->sig_info(portLibrary, info, category, count, &name, &value) != OMRPORT_SIG_VALUE_UNDEFINED) {
		count++;
	}

	return count;
}

static uint32_t
infoForFPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	if (info->ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT) {
		switch (index) {
		case 0:
			*name = "XMM0";
			*value = &info->ContextRecord->Xmm0.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 1:
			*name = "XMM1";
			*value = &info->ContextRecord->Xmm1.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 2:
			*name = "XMM2";
			*value = &info->ContextRecord->Xmm2.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 3:
			*name = "XMM3";
			*value = &info->ContextRecord->Xmm3.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 4:
			*name = "XMM4";
			*value = &info->ContextRecord->Xmm4.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 5:
			*name = "XMM5";
			*value = &info->ContextRecord->Xmm5.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 6:
			*name = "XMM6";
			*value = &info->ContextRecord->Xmm6.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 7:
			*name = "XMM7";
			*value = &info->ContextRecord->Xmm7.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 8:
			*name = "XMM8";
			*value = &info->ContextRecord->Xmm8.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 9:
			*name = "XMM9";
			*value = &info->ContextRecord->Xmm9.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 10:
			*name = "XMM10";
			*value = &info->ContextRecord->Xmm10.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 11:
			*name = "XMM11";
			*value = &info->ContextRecord->Xmm11.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 12:
			*name = "XMM12";
			*value = &info->ContextRecord->Xmm12.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 13:
			*name = "XMM13";
			*value = &info->ContextRecord->Xmm13.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 14:
			*name = "XMM14";
			*value = &info->ContextRecord->Xmm14.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		case 15:
			*name = "XMM15";
			*value = &info->ContextRecord->Xmm15.Low;
			return OMRPORT_SIG_VALUE_FLOAT_64;
		default:
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

static LONG WINAPI
masterVectoredExceptionHandler(EXCEPTION_POINTERS *exceptionInfo)
{
	uint32_t portLibType;
	struct J9SignalHandlerRecord *thisRecord;
	struct J9CurrentSignal currentSignal;
	struct J9CurrentSignal *previousSignal;
	omrthread_t thisThread = NULL;

	if ((exceptionInfo->ExceptionRecord->ExceptionCode & (ERROR_SEVERITY_ERROR | APPLICATION_ERROR_MASK)) != ERROR_SEVERITY_ERROR) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	/* masterVectoredExceptionHandler is a process-wide handler so it
	 * may be invoked by threads that aren't omrthreads or that had
	 * not been protected by omrsig_protect() */

	/* is this a omrthread? */
	thisThread = omrthread_self();
	if (NULL == thisThread) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	/* had omrsig_protect() been called on this thread? */
	thisRecord = omrthread_tls_get(thisThread, tlsKey);
	if (NULL == thisRecord) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (thisRecord->deferToTryExcept) {
		/* Defer to the _try/_except in omrsig_protect */
		return EXCEPTION_CONTINUE_SEARCH;
	}

	/* record this signal in tls so that omrsig_handler can be called if any of the handlers decide we should be shutting down */
	currentSignal.exceptionInfo = exceptionInfo;

	portLibType = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	currentSignal.portLibSignalType = portLibType;

	previousSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);
	omrthread_tls_set(thisThread, tlsKeyCurrentSignal, &currentSignal);

	/* walk the stack of registered handlers from top to bottom searching for one which handles this type of exception */
	while (thisRecord) {
		if (thisRecord->flags & portLibType) {
			struct J9Win32SignalInfo j9info;
			uintptr_t result;

			/* found a suitable handler */
			fillInWinAMD64SignalInfo(thisRecord->portLibrary, thisRecord->handler, exceptionInfo, &j9info);

			/* remove the handler we are about to invoke, now, in case the handler crashes */
			omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);

			result = thisRecord->handler(thisRecord->portLibrary, portLibType, &j9info, thisRecord->handler_arg);

			/* The only case in which we don't want the previous handler back on top is if it just returned OMRPORT_SIG_EXCEPTION_RETURN
			 * 		In this case we will remove it from the top */
			omrthread_tls_set(thisThread, tlsKey, thisRecord);

			if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
				/* continue looping */
			} else if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
				omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
				return EXCEPTION_CONTINUE_EXECUTION;
			} else /* if (result == OMRPORT_SIG_EXCEPTION_RETURN) */ {
				if (thisRecord->deferToTryExcept) {
					return EXCEPTION_CONTINUE_SEARCH;
				} else {
					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
					longjmp(thisRecord->returnBuf, 0);
					/* unreachable */
				}
			}
		}

		thisRecord = thisRecord->previous;
	}
	omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);

	return EXCEPTION_CONTINUE_SEARCH;

}

/*
 * This was copied from port/win32/omrsignal.c
 * and modified to remove support for OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION.
 * Handling OMRPORT_SIG_FLAG_MAY_RETURN is different on Win64.
 * @param portLibrary port library
 * @param handler signal handler function
 * @param handler_arg user-supplied parameter for the handler
 * @param flags specify which signals are handled
 * @param exceptionInfo Operatingsystem supplied information about the signal and context.
 * @return code indicating action following the signal handling
 */
static int
structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler,
						   void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo)
{
	uintptr_t result;
	uint32_t type;
	struct J9Win32SignalInfo j9info;
	omrthread_t thisThread;
	struct J9SignalHandlerRecord *thisRecord;
	struct J9CurrentSignal currentSignal;
	struct J9CurrentSignal *previousSignal;

	if ((exceptionInfo->ExceptionRecord->ExceptionCode & (ERROR_SEVERITY_ERROR | APPLICATION_ERROR_MASK)) != ERROR_SEVERITY_ERROR) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	type = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	if (0 == (type & flags)) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	thisThread = omrthread_self();
	thisRecord = omrthread_tls_get(thisThread, tlsKey);

	fillInWinAMD64SignalInfo(portLibrary, handler, exceptionInfo, &j9info);

	previousSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

	currentSignal.exceptionInfo = exceptionInfo;
	currentSignal.portLibSignalType = type;

	omrthread_tls_set(thisThread, tlsKeyCurrentSignal, &currentSignal);

	__try {
		result = handler(portLibrary, j9info.portLibType, &j9info, handler_arg);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		/* if a recursive exception occurs, ignore it and pass control to the next handler */
		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
		omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH == result) {
		/*
		 * vmSignalHandler will defer to try/except without running the crash handler
		 * if it finds a handler on the stack.  Setting tryExceptHandlerIgnore=TRUE
		 * forces it to ignore try/except handlers.
		 */
		j9info.tryExceptHandlerIgnore = TRUE;
		thisRecord = thisRecord->previous;
		while (thisRecord) {
			if (thisRecord->flags & type) {

				/* remove the handler we are about to invoke, now, in case the handler crashes */
				omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);

				result = thisRecord->handler(thisRecord->portLibrary, type, &j9info, thisRecord->handler_arg);

				/* The only case in which we don't want the previous handler back on top is if it just returned OMRPORT_SIG_EXCEPTION_RETURN
				 * 		In this case we will remove it from the top */
				omrthread_tls_set(thisThread, tlsKey, thisRecord);

				if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {

					/* continue looping */
				} else if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
					/* This usage deprecated: it is not exhaustively tested or guaranteed to work */
					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
					return EXCEPTION_CONTINUE_EXECUTION;
				} else if (result == OMRPORT_SIG_EXCEPTION_RETURN) {
					break;
				}
			}
			thisRecord = thisRecord->previous;
		}
	}

	omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
	if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
		return EXCEPTION_CONTINUE_SEARCH;
	} else /* if (result == OMRPORT_SIG_EXCEPTION_RETURN) || (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) */ {
		/* Note that OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION isn't supported in this use of SEH, but there's no way to report the error */
		return EXCEPTION_EXECUTE_HANDLER;
	}

}

/* Adds the master exception handler
 *
 * Calls to this function must be synchronized around the masterExceptionMonitor
 *
 * @param[in] portLibrary The port library
 *
 * @return non-zero if the vectored exception handler was installed properly.
 *
 */
static uint32_t
addMasterVectoredExceptionHandler(struct OMRPortLibrary *portLibrary)
{
	/* We can't use Windows structured exception handling (SEH) on this platform.
	 * On AMD64, SEH relies on finding exception records associated with the current
	 * point of execution. This only works for code in a module (e.g. a DLL). Since JIT
	 * code is actually in allocated memory, SEH is bypassed for exceptions which
	 * occur in JIT code
	 */
	if (NULL == AddVectoredExceptionHandler(1, masterVectoredExceptionHandler)) {
		omrthread_tls_free(tlsKey);
		omrthread_tls_free(tlsKeyCurrentSignal);
		return -1;
	}

	/* vectoredExceptionHandlerInstalled is checked in omrsig_protect without checking the masterExceptionMonitor */
	/* issueWriteBarrier here in response to CMVC 96193, although it's likely not needed on this platform */
	issueWriteBarrier();
	vectoredExceptionHandlerInstalled = 1;

	return 0;
}

/* Destroys the signal tools
 *
 * @param[in] portLibrary The port library
 *
 * @return non-zero if the vectored exception handler was installed properly.
 *
 */
static void
destroySignalTools(OMRPortLibrary *portLibrary)
{
	omrthread_monitor_destroy(asyncMonitor);
	omrthread_monitor_destroy(masterExceptionMonitor);
	omrthread_monitor_destroy(registerHandlerMonitor);
	j9sem_destroy(wakeUpASyncReporter);
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
}

static int32_t
initializeSignalTools(OMRPortLibrary *portLibrary)
{
	if (omrthread_monitor_init_with_name(&asyncMonitor, 0, "portLibrary_omrsig_async_monitor")) {
		goto error;
	}

	if (omrthread_monitor_init_with_name(&masterExceptionMonitor, 0, "portLibrary_omrsig_master_exception_monitor")) {
		goto cleanup1;
	}

	if (omrthread_monitor_init_with_name(&registerHandlerMonitor, 0, "portLibrary_omrsig_register_handler_monitor")) {
		goto cleanup2;
	}

	if (0 != j9sem_init(&wakeUpASyncReporter, 0)) {
		goto cleanup3;
	}

	if (omrthread_monitor_init_with_name(&asyncReporterShutdownMonitor, 0, "portLibrary_omrsig_asynch_reporter_shutdown_monitor")) {
			goto cleanup4;
	}

#if defined(OMR_PORT_ASYNC_HANDLER)
	if (J9THREAD_SUCCESS != createThreadWithCategory(
			&asynchSignalReporterThread,
			256 * 1024,
			J9THREAD_PRIORITY_MAX,
			0,
			&asynchSignalReporter,
			NULL,
			J9THREAD_CATEGORY_SYSTEM_THREAD)
	) {
		goto cleanup5;
	}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

	return 0;

#if defined(OMR_PORT_ASYNC_HANDLER)
cleanup5:
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */
cleanup4:
	j9sem_destroy(wakeUpASyncReporter);
cleanup3:
	omrthread_monitor_destroy(registerHandlerMonitor);
cleanup2:
	omrthread_monitor_destroy(masterExceptionMonitor);
cleanup1:
	omrthread_monitor_destroy(asyncMonitor);
error:
	return OMRPORT_SIG_ERROR;
}

static void
sig_full_shutdown(struct OMRPortLibrary *portLibrary)
{
	uint32_t index = 1;
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (--attachedPortLibraries == 0) {
#if defined(OMR_PORT_ASYNC_HANDLER)
		/* Terminate asynchSignalReporterThread. */
		omrthread_monitor_enter(asyncReporterShutdownMonitor);
		shutDownASynchReporter = 1;
		j9sem_post(wakeUpASyncReporter);
		while (0 != shutDownASynchReporter) {
			omrthread_monitor_wait(asyncReporterShutdownMonitor);
		}
		omrthread_monitor_exit(asyncReporterShutdownMonitor);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

		/* Register the original signal handlers, which were overwritten. */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			if (handlerInfo[index].restore) {
				signal(index, handlerInfo[index].originalHandler);
				/* record that we no longer have a handler installed with the OS for this signal */
				Trc_PRT_signal_sig_full_shutdown_deregistered_handler_with_OS(portLibrary, index);
				handlerInfo[index].restore = 0;
			}
		}

		/* Remove elements in asyncHandlerList, and free the associated memory. */
		removeAsyncHandlers(portLibrary);

		omrthread_tls_free(tlsKey);
		RemoveVectoredExceptionHandler(masterVectoredExceptionHandler);
		vectoredExceptionHandlerInstalled = 0;

		/* destroy all of the remaining monitors */
		destroySignalTools(portLibrary);

		omrthread_tls_free(tlsKeyCurrentSignal);
	}
	omrthread_monitor_exit(globalMonitor);
}

static void
fillInWinAMD64SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct J9Win32SignalInfo *j9info)
{
	memset(j9info, 0, sizeof(*j9info));

	j9info->systemType = exceptionInfo->ExceptionRecord->ExceptionCode;
	j9info->portLibType = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	j9info->handlerAddress = (void *)handler;
	j9info->handlerAddress2 = (void *)masterVectoredExceptionHandler;
	j9info->ExceptionRecord = exceptionInfo->ExceptionRecord;
	j9info->ContextRecord = exceptionInfo->ContextRecord;
	j9info->deferToTryExcept = FALSE;
	j9info->tryExceptHandlerIgnore = FALSE;

	/* module info is filled on demand */
}

static BOOL WINAPI
consoleCtrlHandler(DWORD dwCtrlType)
{
	uint32_t flags;
	BOOL result = FALSE;

	switch (dwCtrlType) {
	case CTRL_BREAK_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGQUIT;
		break;
	case CTRL_C_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGINT;
		break;
	case CTRL_CLOSE_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGTERM;
		break;
	default:
		return result;
	}

	if (0 == omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT)) {
		J9WinAMD64AsyncHandlerRecord *cursor;

		/* incrementing the asyncThreadCount will prevent the list from being modified while we use it */
		omrthread_monitor_enter(asyncMonitor);
		asyncThreadCount++;
		omrthread_monitor_exit(asyncMonitor);

		cursor = asyncHandlerList;
		while (cursor) {

			if (cursor->flags & flags) {

				cursor->handler(cursor->portLib, flags, NULL, cursor->handler_arg);

				/* Returning TRUE stops control from being passed to the next handler routine. The OS default handler may be the next handler routine.
				 * The default action of the OS default handler routine is to shut down the process
				 */
				if (flags & OMRPORT_SIG_FLAG_SIGQUIT) {
					/* Continue executing */
					result = TRUE;
				}
			}

			cursor = cursor->next;
		}

		omrthread_monitor_enter(asyncMonitor);
		if (--asyncThreadCount == 0) {
			omrthread_monitor_notify_all(asyncMonitor);
		}
		omrthread_monitor_exit(asyncMonitor);

		/* TODO: possible timing hole. The thread library could be unloaded by the time we reach this line. We can't use omrthread_exit(), as that kills the async reporting thread */
		omrthread_detach(NULL);
	}

	return result;
}

/**
 * The OS signal number is converted to the corresponding port library
 * signal flag.
 *
 * @param[in] signalNo OS signal number
 *
 * @return The corresponding port library signal flag on success.
 *         Otherwise, return 0 in case of error.
 */
static uint32_t
mapOSSignalToPortLib(uint32_t signalNo)
{
	uint32_t index = 0;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].osSignalNo == signalNo) {
			return signalMap[index].portLibSignalNo;
		}
	}

	Trc_PRT_signal_mapOSSignalToPortLib_ERROR_unknown_signal(signalNo);
	return 0;
}

/**
 * The port library signal flag is converted to the corresponding OS signal number.
 *
 * @param portLibSignal the port library signal flag
 *
 * @return The corresponding OS signal number or OMRPORT_SIG_ERROR (-1) if the portLibSignal
 *         can't be mapped.
 */
static int
mapPortLibSignalToOSSignal(uint32_t portLibSignal)
{
	uint32_t index = 0;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].portLibSignalNo == portLibSignal) {
			return signalMap[index].osSignalNo;
		}
	}

	Trc_PRT_signal_mapPortLibSignalToOSSignal_ERROR_unknown_signal(portLibSignal);
	return OMRPORT_SIG_ERROR;
}

/**
 * Register the signal handler with the OS. Calls to this function must be synchronized using
 * "registerHandlerMonitor". *oldOSHandler points to the old signal handler function. It is only
 * set if oldOSHandler is non-null. During the first registration, the old signal handler is
 * stored in handlerInfo[osSignalNo].originalHandler. The original OS handler must be restored
 * during port library shut down.
 *
 * @param[in] portLibrary the OMR port library
 * @param[in] portLibrarySignalNo the port library signal flag
 * @param[in] handler the OS handler to register
 * @param[out] oldOSHandler points to the old OS handler, if oldOSHandler is non-null
 *
 * @return 0 upon success, non-zero otherwise.
 */
static int32_t
registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, win_signal handler, void **oldOSHandler)
{
    int osSignalNo = mapPortLibSignalToOSSignal(portLibrarySignalNo);
    win_signal localOldOSHandler = NULL;

    /* Don't register a handler for unrecognized OS signals.
     * Unrecognized OS signals are the ones which aren't included in signalMap.
     */
    if (OMRPORT_SIG_ERROR == osSignalNo) {
        return OMRPORT_SIG_ERROR;
    }

    localOldOSHandler = signal(osSignalNo, handler);
    if (SIG_ERR == localOldOSHandler) {
        Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, osSignalNo, handler);
        return OMRPORT_SIG_ERROR;
    }

    Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler1(portLibrarySignalNo, osSignalNo, handler, localOldOSHandler);
    if (0 == handlerInfo[osSignalNo].restore) {
        handlerInfo[osSignalNo].originalHandler = localOldOSHandler;
        handlerInfo[osSignalNo].restore = 1;
    }

    if (NULL != oldOSHandler) {
        *oldOSHandler = (void *)localOldOSHandler;
    }

    return 0;
}

/**
 * Atomically increment signalCounts[osSignalNo] and notify the async signal reporter thread
 * that a signal is received.
 *
 * @param[in] osSignalNo the integer value of the signal that is raised
 *
 * @return void
 */
static void
updateSignalCount(int osSignalNo)
{
	addAtomic(&signalCounts[osSignalNo], 1);
	j9sem_post(wakeUpASyncReporter);
}

/**
 * Invoke updateSignalCount(osSignalNo), and re-register masterASynchSignalHandler since the
 * masterASynchSignalHandler is unregistered after invocation.
 *
 * @param[in] osSignalNo the integer value of the signal that is raised
 *
 * @return void
 */
static void
masterASynchSignalHandler(int osSignalNo)
{
	updateSignalCount(osSignalNo);

	/* Signal handler is reset after invocation. So, the signal handler needs to
	 * be registered again after it is invoked.
	 */
	signal(osSignalNo, masterASynchSignalHandler);
}

/**
 * Remove elements in asyncHandlerList, and free the associated memory.
 *
 * @param[in] portLibrary the OMR port library
 *
 * @return void
 */
static void
removeAsyncHandlers(OMRPortLibrary *portLibrary)
{
	/* clean up the list of async handlers */
	J9WinAMD64AsyncHandlerRecord *cursor = NULL;
	J9WinAMD64AsyncHandlerRecord **previousLink = NULL;

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (cursor) {
		if (cursor->portLib == portLibrary) {
			*previousLink = cursor->next;
			portLibrary->mem_free_memory(portLibrary, cursor);
			cursor = *previousLink;
		} else {
			previousLink = &cursor->next;
 			cursor = cursor->next;
 		}
	}

	omrthread_monitor_exit(asyncMonitor);
}

#if defined(OMR_PORT_ASYNC_HANDLER)
/**
 * Given a port library signal flag, execute the associated handlers stored
 * within asyncHandlerList (list of J9WinAMD64AsyncHandlerRecord).
 *
 * @param asyncSignalFlag port library signal flag
 *
 * @return void
 */
static void
runHandlers(uint32_t asyncSignalFlag)
{
	J9WinAMD64AsyncHandlerRecord *cursor = NULL;

	/* incrementing the asyncThreadCount will prevent the list from being modified while we use it */
	omrthread_monitor_enter(asyncMonitor);
	asyncThreadCount++;
	omrthread_monitor_exit(asyncMonitor);

	cursor = asyncHandlerList;
	while (NULL != cursor) {
		if (OMR_ARE_ANY_BITS_SET(cursor->flags, asyncSignalFlag)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_calling_handler(cursor->portLib, asyncSignalFlag, cursor->handler_arg);
			cursor->handler(cursor->portLib, asyncSignalFlag, NULL, cursor->handler_arg);
		}
		cursor = cursor->next;
	}

	omrthread_monitor_enter(asyncMonitor);
	if (--asyncThreadCount == 0) {
		omrthread_monitor_notify_all(asyncMonitor);
	}
	omrthread_monitor_exit(asyncMonitor);
}

/**
 * This is the main body of the asynchSignalReporterThread. It executes handlers
 * (J9WinAMD64AsyncHandlerRecord->handler) associated to a signal once a signal
 * is raised.
 *
 * @param[in] userData the user data provided during thread creation
 *
 * @return the return statement won't be executed since the thread exits before
 *         executing the return statement
 */
static int J9THREAD_PROC
asynchSignalReporter(void *userData)
{
	omrthread_set_name(omrthread_self(), "Signal Reporter");

	while (0 == shutDownASynchReporter) {
		int osSignal = 1;
		uint32_t asyncSignalFlag = 0;

		/* determine which signal we've been woken up for */
		for (osSignal = 1; osSignal < ARRAY_SIZE_SIGNALS; osSignal++) {
			uintptr_t signalCount = signalCounts[osSignal];

			if (signalCount > 0) {
				asyncSignalFlag = mapOSSignalToPortLib(osSignal);
				runHandlers(asyncSignalFlag);
				subtractAtomic(&signalCounts[osSignal], 1);
				/* j9sem_wait will fall-through for each j9sem_post. We can handle
				 * one signal at a time. Ultimately, all signals will be handled
				 * So, break out of the for loop.
				 */
				break;
			}
		}

		j9sem_wait(wakeUpASyncReporter);

		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();
	}

	omrthread_monitor_enter(asyncReporterShutdownMonitor);
	shutDownASynchReporter = 0;
	omrthread_monitor_notify(asyncReporterShutdownMonitor);

	omrthread_exit(asyncReporterShutdownMonitor);

	/* Unreachable. */
	return 0;
}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

/**
 * Register the master handler for the signals in flags that don't have one.
 * Only masterASynchSignalHandler/OMRPORT_SIG_FLAG_SIGALLASYNC is supported
 * on win64amd. OMRPORT_SIG_FLAG_SIGALLSYNC is not supported on win64amd.
 *
 * Calls to this function must be synchronized using registerHandlerMonitor.
 *
 * @param[in] flags the flags that we want signals for
 * @param[in] allowedSubsetOfFlags must be OMRPORT_SIG_FLAG_SIGALLASYNC for
 *            asynchronous signals. OMRPORT_SIG_FLAG_SIGALLSYNC is not
 *            supported on win64amd.
 * @param[out] *oldOSHandler points to the old signal handler function, if
 *             oldOSHandler is non-null
 *
 * @return	0 upon success; OMRPORT_SIG_ERROR otherwise.
 *			Possible failure scenarios include attempting to register a handler for
 *			a signal that is not included in the allowedSubsetOfFlags
 */
static int32_t
registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler)
{
	uint32_t flagsSignalsOnly = 0;
	win_signal handler = NULL;

	if (OMRPORT_SIG_FLAG_SIGALLASYNC == allowedSubsetOfFlags) {
		handler = masterASynchSignalHandler;
	} else {
		return OMRPORT_SIG_ERROR;
	}

	flagsSignalsOnly = flags & allowedSubsetOfFlags;

	if (0 != flagsSignalsOnly) {
		/* registering some handlers */
		uint32_t portSignalType = 0;

		/* OMRPORT_SIG_SMALLEST_SIGNAL_FLAG represents the smallest signal
		 * flag. portSignalType is initialized to the smallest signal flag
		 * in order to avoid non-signal flags. Any non-signal flags greater
		 * than the smallest signal flag are ignored via a bitwise-and
		 * operation with allowedSubsetOfFlags. allowedSubsetOfFlags either
		 * represents all synchronous signal flags (OMRPORT_SIG_FLAG_SIGALLSYNC)
		 * or all asynchronous signal flags (OMRPORT_SIG_FLAG_SIGALLASYNC).
		 */
		for (portSignalType = OMRPORT_SIG_SMALLEST_SIGNAL_FLAG; portSignalType < allowedSubsetOfFlags; portSignalType = portSignalType << 1) {
			/* Iterate through all the  signals and register the master handler for the signals
			 * specified in flagsSignalsOnly.
			 */
			if (OMR_ARE_ALL_BITS_SET(flagsSignalsOnly, portSignalType)) {
				if (0 != registerSignalHandlerWithOS(portLibrary, portSignalType, handler, oldOSHandler)) {
					return OMRPORT_SIG_ERROR;
				}
			}
		}
	}

	return 0;
}
