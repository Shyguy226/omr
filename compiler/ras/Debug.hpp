/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

//  __   ___  __   __   ___  __       ___  ___  __
// |  \ |__  |__) |__) |__  /  `  /\   |  |__  |  \
// |__/ |___ |    |  \ |___ \__, /~~\  |  |___ |__/
//
// This file is now deprecated and its contents are slowly
// being moved back to codegen and other directories. Please do not
// add more interfaces here.
//

#ifndef DEBUG_INCL
#define DEBUG_INCL

#include <stdarg.h>                         // for va_list
#include <stddef.h>                         // for NULL, size_t
#include <stdint.h>                         // for int32_t, uint32_t, etc
#include <stdio.h>                          // for FILE
#include <string.h>                         // for strcpy
#include "codegen/Machine.hpp"              // for MachineBaseConnector
#include "codegen/RegisterConstants.hpp"    // for TR_RegisterKinds, etc
#include "compile/Method.hpp"               // for TR_Method, etc
#include "compile/VirtualGuard.hpp"         // for TR_VirtualGuardKind, etc.
#include "cs2/hashtab.h"                    // for HashTable
#include "env/RawAllocator.hpp"
#include "env/TRMemory.hpp"                 // for Allocator, etc
#include "env/jittypes.h"
#include "il/DataTypes.hpp"                 // for DataTypes, etc
#include "il/ILOpCodes.hpp"                 // for ILOpCodes, etc
#include "il/ILOps.hpp"                     // for TR::ILOpCode
#include "infra/Assert.hpp"                 // for TR_ASSERT
#include "infra/BitVector.hpp"              // for TR_BitVector
#include "infra/TRlist.hpp"
#include "optimizer/Optimizations.hpp"      // for Optimizations
#include "runtime/Runtime.hpp"              // for TR_CCPreLoadedCode

#include "codegen/RegisterRematerializationInfo.hpp" // REMOVE_THIS_LATER

class TR_Debug;
class TR_BlockStructure;
class TR_CHTable;
class TR_CompilationFilters;
class TR_FilterBST;
class TR_FrontEnd;
class TR_GCStackMap;
class TR_InductionVariable;
class TR_PrettyPrinterString;
class TR_PseudoRandomNumbersListElement;
class TR_RegionAnalysis;
class TR_RegionStructure;
class TR_RematerializationInfo;
class TR_ResolvedMethod;
class TR_Structure;
class TR_StructureSubGraphNode;
class TR_VPConstraint;
namespace TR { class GCStackAtlas; }
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class DebugCounterGroup; }
namespace TR { class GCRegisterMap; }
namespace TR { class InstOpCode; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Machine; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class OptionSet; }
namespace TR { class Options; }
namespace TR { class RealRegister; }
namespace TR { class Register; }
namespace TR { class RegisterDependency; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SimpleRegex; }
namespace TR { class Snippet; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }
namespace TR { struct OptionTable; }
template <class T> class List;
template <class T> class ListIterator;

struct J9JITExceptionTable;
struct J9AnnotationInfo;
struct J9AnnotationInfoEntry;
struct J9PortLibrary;

class TR_X86OpCode;
namespace TR { class X86LabelInstruction;                  }
namespace TR { class X86PaddingInstruction;                }
namespace TR { class X86AlignmentInstruction;              }
namespace TR { class X86BoundaryAvoidanceInstruction;      }
namespace TR { class X86PatchableCodeAlignmentInstruction; }
namespace TR { class X86RestoreVMThreadInstruction;        }
namespace TR { class X86FenceInstruction;                  }
namespace TR { class X86VirtualGuardNOPInstruction;        }
namespace TR { class X86ImmInstruction;                    }
namespace TR { class X86ImmSnippetInstruction;             }
namespace TR { class X86ImmSymInstruction;                 }
namespace TR { class X86RegInstruction;                    }
namespace TR { class X86RegRegInstruction;                 }
namespace TR { class X86RegImmInstruction;                 }
namespace TR { class X86RegRegImmInstruction;              }
namespace TR { class X86RegRegRegInstruction;              }
namespace TR { class X86MemInstruction;                    }
namespace TR { class X86MemImmInstruction;                 }
namespace TR { class X86MemRegInstruction;                 }
namespace TR { class X86MemRegImmInstruction;              }
namespace TR { class X86MemRegRegInstruction;              }
namespace TR { class X86RegMemInstruction;                 }
namespace TR { class X86RegMemImmInstruction;              }
namespace TR { class X86FPRegInstruction;                  }
namespace TR { class X86FPRegRegInstruction;               }
namespace TR { class X86FPMemRegInstruction;               }
namespace TR { class X86FPRegMemInstruction;               }
class TR_X86RegisterDependencyGroup;
class TR_X86RestartSnippet;
class TR_S390LookupSwitchSnippet;
class TR_X86PicDataSnippet;
class TR_IA32ConstantDataSnippet;
class TR_IA32DataSnippet;
class TR_X86DivideCheckSnippet;
class TR_X86FPConvertToIntSnippet;
class TR_X86FPConvertToLongSnippet;
class TR_X86fbits2iSnippet;
class TR_X86GuardedDevirtualSnippet;
class TR_X86HelperCallSnippet;
class TR_X86ScratchArgHelperCallSnippet;
namespace TR { class UnresolvedDataSnippet; }
class TR_X86UnresolvedVirtualCallSnippet;
namespace TR { class AMD64Imm64Instruction;    }
namespace TR { class AMD64Imm64SymInstruction; }
namespace TR { class AMD64RegImm64Instruction; }
class TR_AMD64FPConversionSnippet;

struct TR_VFPState;
namespace TR { class X86VFPSaveInstruction;        }
namespace TR { class X86VFPRestoreInstruction;     }
namespace TR { class X86VFPDedicateInstruction;    }
namespace TR { class X86VFPReleaseInstruction;     }
namespace TR { class X86VFPCallCleanupInstruction; }

#ifdef J9_PROJECT_SPECIFIC
class TR_X86CallSnippet;
class TR_IA32WriteBarrierSnippet;
class TR_AMD64WriteBarrierSnippet;
class TR_X86JNIPauseSnippet;
class TR_X86PassJNINullSnippet;
class TR_X86HeapAllocationSnippet;
class TR_X86CheckFailureSnippet;
class TR_X86CheckFailureSnippetWithResolve;
class TR_X86BoundCheckWithSpineCheckSnippet;
class TR_X86SpineCheckSnippet;
class TR_X86ForceRecompilationSnippet;
class TR_X86RecompilationSnippet;
#endif

namespace TR { class PPCDepInstruction;                  }
namespace TR { class PPCLabelInstruction;                }
namespace TR { class PPCDepLabelInstruction;             }
namespace TR { class PPCConditionalBranchInstruction;    }
namespace TR { class PPCDepConditionalBranchInstruction; }
namespace TR { class PPCAdminInstruction;                }
namespace TR { class PPCImmInstruction;                  }
namespace TR { class PPCSrc1Instruction;                 }
namespace TR { class PPCDepImmInstruction;               }
namespace TR { class PPCDepImmSymInstruction;            }
namespace TR { class PPCTrg1Instruction;                 }
namespace TR { class PPCTrg1Src1Instruction;             }
namespace TR { class PPCTrg1ImmInstruction;              }
namespace TR { class PPCTrg1Src1ImmInstruction;          }
namespace TR { class PPCTrg1Src1Imm2Instruction;         }
namespace TR { class PPCSrc2Instruction;                 }
namespace TR { class PPCTrg1Src2Instruction;             }
namespace TR { class PPCTrg1Src2ImmInstruction;          }
namespace TR { class PPCTrg1Src3Instruction;             }
namespace TR { class PPCMemSrc1Instruction;              }
namespace TR { class PPCMemInstruction;                  }
namespace TR { class PPCTrg1MemInstruction;              }
namespace TR { class PPCControlFlowInstruction;          }
namespace TR { class PPCVirtualGuardNOPInstruction;      }
class TR_PPCUnresolvedCallSnippet;
class TR_PPCVirtualSnippet;
class TR_PPCVirtualUnresolvedSnippet;
class TR_PPCInterfaceCallSnippet;
class TR_PPCHelperCallSnippet;
class TR_PPCMonitorEnterSnippet;
class TR_PPCMonitorExitSnippet;
class TR_PPCReadMonitorSnippet;
class TR_PPCHeapAllocSnippet;

class TR_PPCAllocPrefetchSnippet;

class TR_PPCLockReservationEnterSnippet;
class TR_PPCLockReservationExitSnippet;
class TR_PPCArrayCopyCallSnippet;

#ifdef J9_PROJECT_SPECIFIC
class TR_PPCInterfaceCastSnippet;
class TR_PPCStackCheckFailureSnippet;
class TR_PPCForceRecompilationSnippet;
class TR_PPCRecompilationSnippet;
class TR_PPCCallSnippet;
#endif


class TR_ARMOpCode;
class TR_ARMLabelInstruction;
class TR_ARMConditionalBranchInstruction;
class TR_ARMVirtualGuardNOPInstruction;
class TR_ARMAdminInstruction;
class TR_ARMImmInstruction;
class TR_ARMImmSymInstruction;
class TR_ARMTrg1Src2Instruction;
class TR_ARMTrg2Src1Instruction;
class TR_ARMMulInstruction;
class TR_ARMMemSrc1Instruction;
class TR_ARMTrg1Instruction;
class TR_ARMMemInstruction;
class TR_ARMTrg1MemInstruction;
class TR_ARMTrg1MemSrc1Instruction;
class TR_ARMControlFlowInstruction;
class TR_ARMMultipleMoveInstruction;
class TR_ARMMemoryReference;
class TR_ARMOperand2;
class TR_ARMRealRegister;
class TR_ARMCallSnippet;
class TR_ARMUnresolvedCallSnippet;
class TR_ARMVirtualSnippet;
class TR_ARMVirtualUnresolvedSnippet;
class TR_ARMInterfaceCallSnippet;
class TR_ARMHelperCallSnippet;
class TR_ARMMonitorEnterSnippet;
class TR_ARMMonitorExitSnippet;
class TR_ARMStackCheckFailureSnippet;
class TR_ARMRecompilationSnippet;
class TR_ARMRegisterDependencyGroup;

class TR_S390LabelInstruction;
class TR_S390BranchInstruction;
class TR_S390BranchOnCountInstruction;
class TR_S390VirtualGuardNOPInstruction;
class TR_S390BranchOnIndexInstruction;
class TR_S390AnnotationInstruction;
class TR_S390PseudoInstruction;
class TR_S390ImmInstruction;
class TR_S390ImmSnippetInstruction;
class TR_S390ImmSymInstruction;
class TR_S390Imm2Instruction;
class TR_S390RegInstruction;
class TR_S390RRInstruction;
class TR_S390TranslateInstruction;
class TR_S390RRFInstruction;
class TR_S390RRRInstruction;
class TR_S390RXFInstruction;
class TR_S390RIInstruction;
class TR_S390RILInstruction;
class TR_S390RSInstruction;
class TR_S390RSLInstruction;
class TR_S390RSLbInstruction;
class TR_S390RXEInstruction;
class TR_S390RXInstruction;
class TR_S390MemInstruction;
class TR_S390SS1Instruction;
class TR_S390MIIInstruction;
class TR_S390SMIInstruction;
class TR_S390SS2Instruction;
class TR_S390SS4Instruction;
class TR_S390SSEInstruction;
class TR_S390SSFInstruction;
class TR_S390VRIInstruction;
class TR_S390VRIaInstruction;
class TR_S390VRIbInstruction;
class TR_S390VRIcInstruction;
class TR_S390VRIdInstruction;
class TR_S390VRIeInstruction;
class TR_S390VRRInstruction;
class TR_S390VRRaInstruction;
class TR_S390VRRbInstruction;
class TR_S390VRRcInstruction;
class TR_S390VRRdInstruction;
class TR_S390VRReInstruction;
class TR_S390VRRfInstruction;
class TR_S390VRSaInstruction;
class TR_S390VRSbInstruction;
class TR_S390VRScInstruction;
class TR_S390VRVInstruction;
class TR_S390VRXInstruction;
class TR_S390VStorageInstruction;
class TR_S390OpCodeOnlyInstruction;
class TR_S390IInstruction;
class TR_S390SInstruction;
class TR_S390SIInstruction;
class TR_S390SILInstruction;
class TR_S390NOPInstruction;
class TR_S390WarmToColdTrampolineSnippet;
class TR_S390RestoreGPR7Snippet;
class TR_S390CallSnippet;
class TR_S390ConstantDataSnippet;
class TR_S390WritableDataSnippet;
class TR_S390TargetAddressSnippet;
class TR_S390HelperCallSnippet;
class TR_S390InterfaceCallDataSnippet;
class TR_S390JNICallDataSnippet;

class TR_S390StackCheckFailureSnippet;
class TR_S390HeapAllocSnippet;
class TR_S390RegisterDependencyGroup;
class TR_S390RRSInstruction;
class TR_S390RIEInstruction;
class TR_S390RISInstruction;
class TR_S390IEInstruction;

#ifdef J9_PROJECT_SPECIFIC
class TR_S390ForceRecompilationSnippet;
class TR_S390ForceRecompilationDataSnippet;
class TR_S390UnresolvedCallSnippet;
class TR_S390VirtualSnippet;
class TR_S390VirtualUnresolvedSnippet;
class TR_S390InterfaceCallSnippet;
#endif

TR_Debug *createDebugObject(TR::Compilation *);


class TR_Debug
   {
public:

   void * operator new (size_t s, TR_HeapMemory m);
   void * operator new(size_t s, TR::PersistentAllocator &allocator);

   TR_Debug(TR::Compilation * c);

   TR::FILE *getFile() {return _file;}
   virtual void   setFile(TR::FILE *f) {_file = f;}

   virtual void resetDebugData();
   virtual void newNode(TR::Node *);
   virtual void newLabelSymbol(TR::LabelSymbol *);
   virtual void newRegister(TR::Register *);
   virtual void newVariableSizeSymbol(TR::AutomaticSymbol *sym);
   virtual void newInstruction(TR::Instruction *);
   virtual void addInstructionComment(TR::Instruction *, char*, ...);

   virtual TR_CompilationFilters * getInlineFilters() { return _inlineFilters; }

   virtual TR_FrontEnd *fe() { return _fe; }
   virtual TR::Compilation *comp() { return _comp; }

   virtual char *formattedString(char *buf, uint32_t bufLen, const char *format, va_list args, TR_AllocationKind=heapAlloc);

   virtual TR::Node     *getCurrentParent() { return _currentParent; }
   virtual void        setCurrentParentAndChildIndex(TR::Node *n, int32_t i) { _currentParent = n; _currentChildIndex=i; }
   virtual void        setCurrentParent(TR::Node *n) { _currentParent = n;}

   virtual int32_t     getCurrentChildIndex() { return _currentChildIndex; }
   virtual void        setCurrentChildIndex(int32_t i) { _currentChildIndex = i;}

   // Print current stack back trace to standard error
   static void printStackBacktrace();

   // Print current stack back trace to trace log
   // Argument: comp is the current compilation object, which cannot be NULL
   static void printStackBacktraceToTraceLog(TR::Compilation* comp);

   void breakOn();

   void debugOnCreate();

   // Options processing
   //
   virtual TR::FILE *         findLogFile(TR::Options *aotCmdLineOptions, TR::Options *jitCmdLineOptions, TR::OptionSet *optSet, char *logFileName);
   virtual int32_t         findLogFile(const char *logFileName, TR::Options *aotCmdLineOptions, TR::Options *jitCmdLineOptions, TR::Options **optionsArray, int32_t arraySize);
   virtual void            dumpOptionHelp(TR::OptionTable *jitOptions, TR::OptionTable *feOptions, TR::SimpleRegex *nameFilter);

   static void dumpOptions(char *optionsType, char *options, char *envOptions, TR::Options *cmdLineOptions, TR::OptionTable *jitOptions, TR::OptionTable *feOptions, void *, TR_FrontEnd *);
   virtual char *          limitfileOption(char *, void *, TR::OptionTable *, TR::Options *, bool loadLimit, TR_PseudoRandomNumbersListElement **pseudoRandomListHeadPtr = 0);
   virtual char *          inlinefileOption(char *, void *, TR::OptionTable *, TR::Options *);
   virtual char *          limitOption(char *, void *, TR::OptionTable *, TR::Options *, bool loadLimit);
   virtual int32_t *       loadCustomStrategy(char *optFileName);
   virtual bool            methodCanBeCompiled(TR_Memory *mem, TR_ResolvedMethod *, TR_FilterBST * &);
   virtual bool            methodCanBeRelocated(TR_Memory *mem, TR_ResolvedMethod *, TR_FilterBST * &);
   virtual bool            methodSigCanBeCompiled(const char *, TR_FilterBST * & , TR_Method::Type methodType);
   virtual bool            methodSigCanBeRelocated(const char *, TR_FilterBST * & );
   virtual bool            methodSigCanBeCompiledOrRelocated(const char *, TR_FilterBST * &, bool isRelocation, TR_Method::Type methodType);
   virtual bool            methodCanBeFound(TR_Memory *, TR_ResolvedMethod *, TR_CompilationFilters *, TR_FilterBST * &);
   virtual bool            methodSigCanBeFound(const char *, TR_CompilationFilters *, TR_FilterBST * &, TR_Method::Type methodType);
   virtual TR_CompilationFilters * getCompilationFilters() { return _compilationFilters; }
   virtual TR_CompilationFilters * getRelocationFilters() { return _relocationFilters; }
   virtual void            clearFilters(TR_CompilationFilters *);
   void                    clearFilters(bool loadLimit);
   virtual bool            scanInlineFilters(FILE *, int32_t &, TR_CompilationFilters *);
   virtual TR_FilterBST *  addFilter(char * &, int32_t, int32_t, int32_t, TR_CompilationFilters *);
   virtual TR_FilterBST *  addFilter(char * &, int32_t, int32_t, int32_t, bool loadLimit);
   virtual TR_FilterBST *  addExcludedMethodFilter(bool loadLimit);
   virtual bool            addSamplingPoint(char *, TR_FilterBST * &, bool loadLimit);
   virtual int32_t         scanFilterName(char *, TR_FilterBST *);
   virtual void            printFilters(TR_CompilationFilters *);
   virtual void            printFilters();
   virtual void            print(TR_FilterBST * filter);
   virtual void            printSamplingPoints();
   virtual void         printHeader();
   virtual void         printMethodHotness();
   virtual void         printInstrDumpHeader(const char * title);
#if defined(TR_TARGET_ARM)
   virtual void         printAsmDumpHeader();
#endif

   virtual void         printByteCodeAnnotations();
   virtual void         printAnnotationInfoEntry(J9AnnotationInfo *,J9AnnotationInfoEntry *,int32_t);

   virtual void         printOptimizationHeader(const char *, const char *, int32_t, bool mustBeDone);

   virtual const char * getName(TR::ILOpCode);
   virtual const char * getName(TR::ILOpCodes);
   virtual const char * getName(TR::DataTypes);
   virtual const char * getName(TR_RawBCDSignCode);
   virtual const char * getName(TR::LabelSymbol *);
   virtual const char * getName(TR::SymbolReference *);
   virtual const char * getName(TR::Register *, TR_RegisterSizes = TR_WordReg);
   virtual const char * getRealRegisterName(uint32_t regNum);
   virtual const char * getGlobalRegisterName(TR_GlobalRegisterNumber regNum, TR_RegisterSizes size = TR_WordReg);
   virtual const char * getName(TR::Snippet *);
   virtual const char * getName(TR::Node *);
   virtual const char * getName(TR::Symbol *);
   virtual const char * getName(TR::Instruction *);
   virtual const char * getName(TR_Structure *);
   virtual const char * getName(TR::CFGNode *);
   virtual const char * getName(TR_ResolvedMethod *m) { return getName((void *) m, "(TR_ResolvedMethod*)", 0, false); }
   virtual const char * getName(TR_OpaqueClassBlock *c) { return getName((void *) c, "(TR_OpaqueClassBlock*)", 0, false); }
   virtual const char * getName(void *, const char *, uint32_t, bool);
   virtual const char * getName(const char *s) { return s; }
   virtual const char * getName(const char *s, int32_t len) { return s; }
   virtual const char * getVSSName(TR::AutomaticSymbol *sym);
   virtual const char * getWriteBarrierKindName(int32_t);
   virtual const char * getSpillKindName(uint8_t);
   virtual const char * getLinkageConventionName(uint8_t);
   virtual const char * getVirtualGuardKindName(TR_VirtualGuardKind kind);
   virtual const char * getVirtualGuardTestTypeName(TR_VirtualGuardTestType testType);
   virtual const char * getRuntimeHelperName(int32_t);

   virtual void         roundAddressEnumerationCounters(uint32_t boundary=16);

   virtual void         print(TR::FILE *, uint8_t*, uint8_t*);
   virtual void         printIRTrees(TR::FILE *, const char *, TR::ResolvedMethodSymbol *);
   virtual void         printBlockOrders(TR::FILE *, const char *, TR::ResolvedMethodSymbol *);
   virtual void         print(TR::FILE *, TR::CFG *);
   virtual void         print(TR::FILE *, TR_Structure * structure, uint32_t indentation);
   virtual void         print(TR::FILE *, TR_RegionAnalysis * structure, uint32_t indentation);
   virtual int32_t      print(TR::FILE *, TR::TreeTop *);
   virtual int32_t      print(TR::FILE *, TR::Node *, uint32_t indentation=0, bool printSubtree=true);
   virtual void         print(TR::FILE *, TR::SymbolReference *);
   virtual void         print(TR::SymbolReference *, TR_PrettyPrinterString&, bool hideHelperMethodInfo=false, bool verbose=false);
   virtual void         print(TR::FILE *, TR::LabelSymbol *);
   virtual void         print(TR::LabelSymbol *, TR_PrettyPrinterString&);
   virtual void         print(TR::FILE *, TR_BitVector *);
   virtual void         print(TR::FILE *pOutFile, TR::BitVector * bv);
   virtual void         print(TR::FILE *pOutFile, TR::SparseBitVector * sparse);
   virtual void         print(TR::FILE *, TR::SymbolReferenceTable *);
   virtual void         printAliasInfo(TR::FILE *, TR::SymbolReferenceTable *);
   virtual void         printAliasInfo(TR::FILE *, TR::SymbolReference *);

   virtual int32_t      printWithFixedPrefix(TR::FILE *, TR::Node *, uint32_t indentation, bool printChildren, bool printRefCounts, const char *prefix);
   virtual void         printVCG(TR::FILE *, TR::CFG *, const char *);
   virtual void         printVCG(TR::FILE *, TR::Node *, uint32_t indentation);

   virtual void         print(J9JITExceptionTable * data, TR_ResolvedMethod * feMethod, bool fourByteOffsets);

   virtual void         clearNodeChecklist();
   virtual void         saveNodeChecklist(TR_BitVector &saveArea);
   virtual void         restoreNodeChecklist(TR_BitVector &saveArea);
   virtual int32_t      dumpLiveRegisters();
   virtual int32_t      dumpLiveRegisters(TR::FILE *pOutFile, TR_RegisterKinds rk);
   virtual void         dumpLiveRealRegisters(TR::FILE *pOutFile, TR_RegisterKinds rk);
   virtual void         setupToDumpTreesAndInstructions(const char *);
   virtual void         dumpSingleTreeWithInstrs(TR::TreeTop *, TR::Instruction *, bool, bool, bool, bool);
   virtual void         dumpMethodInstrs(TR::FILE *, const char *, bool, bool header = false);
   virtual void         printRegisterKilled(TR::Register *reg);
   virtual void         printNodeEvaluation(TR::Node *node, const char *relationship = "", TR::Register *reg = NULL, bool printOpCode = true);
   virtual void         dumpMixedModeDisassembly();
   virtual void         dumpInstructionComments(TR::FILE *, TR::Instruction *, bool needsStartComment = true );
   virtual void         printCommonDataMiningAnnotations(TR::FILE *pOutFile, TR::Instruction * inst, bool needsStartComment = true);
   virtual void         print(TR::FILE *, TR::Instruction *);
   virtual void         print(TR::FILE *, TR::Instruction *, const char *);
   virtual void         print(TR::FILE *, List<TR::Snippet> &, bool isWarm = false);
   virtual void         print(TR::FILE *, TR::list<TR::Snippet*> &, bool isWarm = false);
   virtual void         print(TR::FILE *, TR::Snippet *);

   virtual void         print(TR::FILE *, TR::RegisterMappedSymbol *, bool);
   virtual void         print(TR::FILE *, TR::GCStackAtlas *);
   virtual void         print(TR::FILE *, TR_GCStackMap *, TR::GCStackAtlas *atlas = NULL);

   virtual void         printRegisterMask(TR::FILE *pOutFile, TR_RegisterMask mask, TR_RegisterKinds rk);
   virtual void         print(TR::FILE *, TR::Register *, TR_RegisterSizes size = TR_WordReg);
   virtual void         printFullRegInfo(TR::FILE *, TR::Register *);
   virtual const char * getRegisterKindName(TR_RegisterKinds);
   virtual const char * toString(TR_RematerializationInfo *);

   virtual void         print(TR::FILE *, TR_VPConstraint *);

#ifdef J9_PROJECT_SPECIFIC
   virtual void         dump(TR::FILE *, TR_CHTable *);
#endif

   virtual void         trace(const char *, ...);
   virtual void         vtrace(const char *, va_list args);
   virtual void         traceLnFromLogTracer(const char *);
   virtual bool         performTransformationImpl(bool, const char *, ...);

   virtual void         printInstruction(TR::Instruction*);

   virtual const char * getDiagnosticFormat(const char *, char *, int32_t);

   virtual void         dumpGlobalRegisterTable();
   virtual void         dumpSimulatedNode(TR::Node *node, char tagChar);

   virtual uint32_t     getLabelNumber() { return _nextLabelNumber; }

   // Verification Passes
   //
   virtual void         verifyTrees (TR::ResolvedMethodSymbol *s);
   virtual void         verifyBlocks(TR::ResolvedMethodSymbol *s);
   virtual void         verifyCFG   (TR::ResolvedMethodSymbol *s);

   virtual TR::Node     *verifyFinalNodeReferenceCounts(TR::ResolvedMethodSymbol *s);

   virtual void startTracingRegisterAssignment() { startTracingRegisterAssignment("backward"); }
   virtual void startTracingRegisterAssignment(const char *direction, TR_RegisterKinds kindsToAssign = TR_RegisterKinds(TR_GPR_Mask|TR_FPR_Mask));
   virtual void         stopTracingRegisterAssignment();
   virtual void         pauseTracingRegisterAssignment();
   virtual void         resumeTracingRegisterAssignment();
   virtual void         traceRegisterAssignment(const char *format, va_list args);
   virtual void         traceRegisterAssignment(TR::Instruction *instr, bool insertedByRA = true, bool postRA = false);
   virtual void         traceRegisterAssigned(TR_RegisterAssignmentFlags flags, TR::Register *virtReg, TR::Register *realReg);
   virtual void         traceRegisterFreed(TR::Register *virtReg, TR::Register *realReg);
   virtual void         traceRegisterInterference(TR::Register *virtReg, TR::Register *interferingVirtual, int32_t distance);
   virtual void         traceRegisterWeight(TR::Register *realReg, uint32_t weight);

   virtual void         printGPRegisterStatus(TR::FILE *pOutFile, OMR::MachineConnector *machine);
   virtual void         printFPRegisterStatus(TR::FILE *pOutFile, OMR::MachineConnector *machine);

   virtual const char * getPerCodeCacheHelperName(TR_CCPreLoadedCode helper);

#if defined(TR_TARGET_X86)
   virtual const char * getOpCodeName(TR_X86OpCode *);
   virtual const char * getMnemonicName(TR_X86OpCode *);
   virtual void         printReferencedRegisterInfo(TR::FILE *, TR::Instruction *);
   virtual void         dumpInstructionWithVFPState(TR::Instruction *instr, const TR_VFPState *prevState);

   void print(TR::FILE *, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   const char * getNamex(TR::Snippet *);
   void printRegMemInstruction(TR::FILE *, const char *, TR::RealRegister *reg, TR::RealRegister *base = 0, int32_t = 0);
   void printRegRegInstruction(TR::FILE *, const char *, TR::RealRegister *reg1, TR::RealRegister *reg2 = 0);
   void printRegImmInstruction(TR::FILE *, const char *, TR::RealRegister *reg, int32_t imm);
   void printMemRegInstruction(TR::FILE *, const char *, TR::RealRegister *base, int32_t offset, TR::RealRegister * = 0);
   void printMemImmInstruction(TR::FILE *, const char *, TR::RealRegister *base, int32_t offset, int32_t imm);
#endif
#if defined(TR_TARGET_POWER)
   virtual const char * getOpCodeName(TR::InstOpCode *);
   const char * getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   void print(TR::FILE *, TR_PPCHelperCallSnippet *);
#endif
#if defined(TR_TARGET_ARM)
   virtual void printARMDelayedOffsetInstructions(TR::FILE *pOutFile, TR_ARMMemInstruction *instr);
   virtual void printARMHelperBranch(TR::SymbolReference *symRef, uint8_t *cursor, TR::FILE *outFile, const char * opcodeName = "bl");
   virtual const char * getOpCodeName(TR_ARMOpCode *);
   const char * getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   const char * getName(uint32_t realRegisterIndex, TR_RegisterSizes = (TR_RegisterSizes)-1);
   void print(TR::FILE *, TR_ARMHelperCallSnippet *);
#endif
#if defined(TR_TARGET_S390)
   virtual const char * getOpCodeName(TR::InstOpCode *);
   virtual void printRegisterDependencies(TR::FILE *pOutFile, TR_S390RegisterDependencyGroup *rgd, int numberOfRegisters);
   const char * getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
#endif

#if defined(AIXPPC)
   virtual void setupDebugger(void *);
#elif defined(LINUX) || defined(J9ZOS390) || (defined(WIN32))
   virtual void setupDebugger(void *, void *, bool);
#endif

   virtual void setSingleAllocMetaData(bool usesSingleAllocMetaData);

   const char* internalNamePrefix() { return "_"; }

   TR_OpaqueClassBlock * containingClass(TR::SymbolReference *);
   const char * signature(TR::ResolvedMethodSymbol *s);
   void nodePrintAllFlags(TR::Node *, TR_PrettyPrinterString &);

   // used by DebugExt and may be overridden
   virtual void printDestination(TR::FILE *, TR::TreeTop *);
   virtual void printDestination(TR::TreeTop *, TR_PrettyPrinterString&);
   virtual void printNodeInfo(TR::FILE *, TR::Node *);
   virtual void printNodeInfo(TR::Node *, TR_PrettyPrinterString& output, bool);
   virtual void print(TR::FILE *, TR::CFGNode *, uint32_t indentation);
   virtual void printNodesInEdgeListIterator(TR::FILE *, TR::list<TR::CFGEdge*> &li, bool fromNode);
   virtual void print(TR::FILE *, TR::Block * block, uint32_t indentation);
   virtual void print(TR::FILE *, TR_RegionStructure * regionStructure, uint32_t indentation);
   virtual void printSubGraph(TR::FILE *, TR_RegionStructure * regionStructure, uint32_t indentation);
   virtual void print(TR::FILE *, TR_InductionVariable * inductionVariable, uint32_t indentation);
   virtual bool inDebugExtension() { return false; }
   virtual void* dxMallocAndRead(uintptrj_t size, void *remotePtr, bool dontAddToMap = false){return remotePtr;}
   virtual void* dxMallocAndReadString(void *remotePtr, bool dontAddToMap = false){return remotePtr;}
   virtual void  dxFree(void * localPtr){return;}
   void printTopLegend(TR::FILE *);
   void printBottomLegend(TR::FILE *);
   void printSymRefTable(TR::FILE *, bool printFullTable = false);
   void printBasicPreNodeInfoAndIndent(TR::FILE *, TR::Node *, uint32_t indentation);
   void printBasicPostNodeInfo(TR::FILE *, TR::Node *, uint32_t indentation);

   bool isAlignStackMaps()
      {
#if defined(TR_HOST_ARM)
      return true;
#else
      return false;
#endif
      }

   void printNodeFlags(TR::FILE *, TR::Node *);
#ifdef J9_PROJECT_SPECIFIC
   void printBCDNodeInfo(TR::FILE *pOutFile, TR::Node * node);
   void printDFPNodeInfo(TR::FILE *pOutFile, TR::Node * node);
   void printBCDNodeInfo(TR::Node * node, TR_PrettyPrinterString& output);
   void printDFPNodeInfo(TR::Node * node, TR_PrettyPrinterString& output);
#endif

   int32_t * printStackAtlas(uintptrj_t startPC, uint8_t * mapBits, int32_t numberOfSlotsMapped, bool fourByteOffsets, int32_t * sizeOfStackAtlas, int32_t frameSize);
   uint16_t printStackAtlasDetails(uintptrj_t startPC, uint8_t * mapBits, int numberOfSlotsMapped, bool fourByteOffsets, int32_t * sizeOfStackAtlas, int32_t frameSize, int32_t *offsetInfo);
   uint8_t *printMapInfo(uintptrj_t startPC, uint8_t * mapBits, int32_t numberOfSlotsMapped, bool fourByteOffsets, int32_t * sizeOfStackAtlas, TR_ByteCodeInfo *byteCodeInfo, uint16_t indexOfFirstInternalPtr, int32_t offsetInfo[], bool nummaps=false);
   void printStackMapInfo(uint8_t * & mapBits, int32_t numberOfSlotsMapped, int32_t * sizeOfStackAtlas, int32_t * offsetInfo, bool nummaps=false);
   void printJ9JITExceptionTableDetails(J9JITExceptionTable *data, J9JITExceptionTable *dbgextRemotePtr = NULL);
   void printSnippetLabel(TR::FILE *, TR::LabelSymbol *label, uint8_t *cursor, const char *comment1, const char *comment2 = 0);
   uint8_t * printPrefix(TR::FILE *, TR::Instruction *, uint8_t *cursor, uint8_t size);



   void printLabelInstruction(TR::FILE *, const char *, TR::LabelSymbol *label);
   int32_t printRestartJump(TR::FILE *, TR_X86RestartSnippet *, uint8_t *);
   int32_t printRestartJump(TR::FILE *, TR_X86RestartSnippet *, uint8_t *, int32_t, const char *);

#ifdef J9_PROJECT_SPECIFIC
   int32_t printRestartJump(TR::FILE *, TR_AMD64WriteBarrierSnippet *, uint8_t *);
#endif

   char * printSymbolName(TR::FILE *, TR::Symbol *, TR::SymbolReference *, TR::MemoryReference *mr=NULL)  ;
   bool isBranchToTrampoline(TR::SymbolReference *, uint8_t *, int32_t &);


   virtual void printDebugCounters(TR::DebugCounterGroup *counterGroup, const char *name);



// ------------------------------

   void printFirst(int32_t i);
   void printCPIndex(int32_t i);
   void printConstant(int32_t i);
   void printConstant(double d);
   void printFirstAndConstant(int32_t i, int32_t j);

   void printLoadConst(TR::FILE *, TR::Node *);
   void printLoadConst(TR::Node *, TR_PrettyPrinterString&);

   TR_CompilationFilters * findOrCreateFilters(TR_CompilationFilters *);
   TR_CompilationFilters * findOrCreateFilters(bool loadLimit);

   void printFilterTree(TR_FilterBST *root);

   TR::ResolvedMethodSymbol * getOwningMethodSymbol(TR::SymbolReference *);
   TR_ResolvedMethod     * getOwningMethod(TR::SymbolReference *);

   const char * getAutoName(TR::SymbolReference *);
   const char * getParmName(TR::SymbolReference *);
   const char * getStaticName(TR::SymbolReference *);
   const char * getStaticName_ForListing(TR::SymbolReference *);

   virtual const char * getMethodName(TR::SymbolReference *);

   const char * getShadowName(TR::SymbolReference *);
   const char * getMetaDataName(TR::SymbolReference *);

   TR::FILE *findLogFile(TR::Options *, TR::OptionSet *, char *);
   void   findLogFile(const char *logFileName, TR::Options *cmdOptions, TR::Options **optionArray, int32_t arraySize, int32_t &index);

   void printPreds(TR::FILE *, TR::CFGNode *);
   void printBaseInfo(TR::FILE *, TR_Structure * structure, uint32_t indentation);
   void print(TR::FILE *, TR_BlockStructure * blockStructure, uint32_t indentation);
   void print(TR::FILE *, TR_StructureSubGraphNode * node, uint32_t indentation);

   void printBlockInfo(TR::FILE *, TR::Node * node);

   void printVCG(TR::FILE *, TR_Structure * structure);
   void printVCG(TR::FILE *, TR_RegionStructure * regionStructure);
   void printVCG(TR::FILE *, TR_StructureSubGraphNode * node, bool isEntry);
   void printVCGEdges(TR::FILE *, TR_StructureSubGraphNode * node);
   void printVCG(TR::FILE *, TR::Block * block, int32_t vorder = -1, int32_t horder = -1);

   void printByteCodeStack(int32_t parentStackIndex, uint16_t byteCodeIndex, char * indent);
   void print(TR::FILE *, TR::GCRegisterMap *);

   void verifyTreesPass1(TR::Node *node);
   void verifyTreesPass2(TR::Node *node, bool isTreeTop);
   void verifyBlocksPass1(TR::Node *node);
   void verifyBlocksPass2(TR::Node *node);
   void verifyGlobalIndices(TR::Node * node, TR::Node **nodesByGlobalIndex);

   TR::Node *verifyFinalNodeReferenceCounts(TR::Node *node);
   uint32_t getIntLength( uint32_t num ) const; // Number of digits in an integer
   // Number of spaces that must be inserted after index when index's length < maxIndexLength so the information following it will be aligned
   uint32_t getNumSpacesAfterIndex( uint32_t index, uint32_t maxIndexLength ) const;

#if defined(TR_TARGET_X86)
   void printPrefix(TR::FILE *, TR::Instruction *instr);
   int32_t printPrefixAndMnemonicWithoutBarrier(TR::FILE *, TR::Instruction *instr, int32_t barrier);
   void printPrefixAndMemoryBarrier(TR::FILE *, TR::Instruction *instr, int32_t barrier, int32_t barrierOffset);
   void dumpDependencyGroup(TR::FILE *pOutFile, TR_X86RegisterDependencyGroup *group, int32_t numConditions, char *prefix, bool omitNullDependencies);
   void dumpDependencies(TR::FILE *, TR::Instruction *);
   void printRegisterInfoHeader(TR::FILE *, TR::Instruction *);
   void printBoundaryAvoidanceInfo(TR::FILE *, TR::X86BoundaryAvoidanceInstruction *);

   void printX86OOLSequences(TR::FILE *pOutFile);

   void printx(TR::FILE *, TR::Instruction *);
   void print(TR::FILE *, TR::X86LabelInstruction *);
   void print(TR::FILE *, TR::X86PaddingInstruction *);
   void print(TR::FILE *, TR::X86AlignmentInstruction *);
   void print(TR::FILE *, TR::X86BoundaryAvoidanceInstruction *);
   void print(TR::FILE *, TR::X86PatchableCodeAlignmentInstruction *);
   void print(TR::FILE *, TR::X86RestoreVMThreadInstruction *);
   void print(TR::FILE *, TR::X86FenceInstruction *);
#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR::X86VirtualGuardNOPInstruction *);
#endif
   void print(TR::FILE *, TR::X86ImmInstruction *);
   void print(TR::FILE *, TR::X86ImmSnippetInstruction *);
   void print(TR::FILE *, TR::X86ImmSymInstruction *);
   void print(TR::FILE *, TR::X86RegInstruction *);
   void print(TR::FILE *, TR::X86RegRegInstruction *);
   void print(TR::FILE *, TR::X86RegImmInstruction *);
   void print(TR::FILE *, TR::X86RegRegImmInstruction *);
   void print(TR::FILE *, TR::X86RegRegRegInstruction *);
   void print(TR::FILE *, TR::X86MemInstruction *);
   void print(TR::FILE *, TR::X86MemImmInstruction *);
   void print(TR::FILE *, TR::X86MemRegInstruction *);
   void print(TR::FILE *, TR::X86MemRegImmInstruction *);
   void print(TR::FILE *, TR::X86MemRegRegInstruction *);
   void print(TR::FILE *, TR::X86RegMemInstruction *);
   void print(TR::FILE *, TR::X86RegMemImmInstruction *);
   void print(TR::FILE *, TR::X86FPRegInstruction *);
   void print(TR::FILE *, TR::X86FPRegRegInstruction *);
   void print(TR::FILE *, TR::X86FPMemRegInstruction *);
   void print(TR::FILE *, TR::X86FPRegMemInstruction *);
   void print(TR::FILE *, TR::AMD64Imm64Instruction *);
   void print(TR::FILE *, TR::AMD64Imm64SymInstruction *);
   void print(TR::FILE *, TR::AMD64RegImm64Instruction *);
   void print(TR::FILE *, TR::X86VFPSaveInstruction *);
   void print(TR::FILE *, TR::X86VFPRestoreInstruction *);
   void print(TR::FILE *, TR::X86VFPDedicateInstruction *);
   void print(TR::FILE *, TR::X86VFPReleaseInstruction *);
   void print(TR::FILE *, TR::X86VFPCallCleanupInstruction *);

   void printReferencedRegisterInfo(TR::FILE *, TR::X86RegRegRegInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86RegInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86RegRegInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86MemInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86MemRegInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86MemRegRegInstruction *);
   void printReferencedRegisterInfo(TR::FILE *, TR::X86RegMemInstruction *);

   void printFullRegisterDependencyInfo(TR::FILE *, TR::RegisterDependencyConditions * conditions);
   void printDependencyConditions(TR_X86RegisterDependencyGroup *, uint8_t, char *, TR::FILE *);

   void print(TR::FILE *, TR::MemoryReference *, TR_RegisterSizes);
   void printReferencedRegisterInfo(TR::FILE *, TR::MemoryReference *);

   int32_t printIntConstant(TR::FILE *pOutFile, int64_t value, int8_t radix, TR_RegisterSizes size = TR_WordReg, bool padWithZeros = false);
   int32_t printDecimalConstant(TR::FILE *pOutFile, int64_t value, int8_t width, bool padWithZeros);
   int32_t printHexConstant(TR::FILE *pOutFile, int64_t value, int8_t width, bool padWithZeros);
   int32_t printFPConstant(TR::FILE *pOutFile, void *value, int8_t numBits, bool printAsBytes = false);
   void printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr);
   void printFPRegisterComment(TR::FILE *pOutFile, TR::Register *target, TR::Register *source);
   void printMemoryReferenceComment(TR::FILE *pOutFile, TR::MemoryReference *mr);
   TR_RegisterSizes getTargetSizeFromInstruction(TR::Instruction *instr);
   TR_RegisterSizes getSourceSizeFromInstruction(TR::Instruction *instr);
   TR_RegisterSizes getImmediateSizeFromInstruction(TR::Instruction *instr);

   void printFullRegInfo(TR::FILE *, TR::RealRegister *);
   void printX86GCRegisterMap(TR::FILE *, TR::GCRegisterMap *);

   const char * getName(TR::RealRegister *, TR_RegisterSizes = TR_WordReg);
   const char * getName(uint32_t realRegisterIndex, TR_RegisterSizes = (TR_RegisterSizes)-1);

   void printx(TR::FILE *, TR::Snippet *);

#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR_X86CallSnippet *);
   void print(TR::FILE *, TR_X86PicDataSnippet *);
   void print(TR::FILE *, TR_X86UnresolvedVirtualCallSnippet *);
   void print(TR::FILE *, TR_IA32WriteBarrierSnippet *);
#ifdef TR_TARGET_64BIT
   uint8_t *printArgs(TR::FILE *, TR_AMD64WriteBarrierSnippet *, bool, uint8_t *);
   void print(TR::FILE *, TR_AMD64WriteBarrierSnippet *);
#endif
   void print(TR::FILE *, TR_X86JNIPauseSnippet *);
   void print(TR::FILE *, TR_X86PassJNINullSnippet *);
   void print(TR::FILE *, TR_X86HeapAllocationSnippet *);
   void print(TR::FILE *, TR_X86CheckFailureSnippet *);
   void print(TR::FILE *, TR_X86CheckFailureSnippetWithResolve *);
   void print(TR::FILE *, TR_X86BoundCheckWithSpineCheckSnippet *);
   void print(TR::FILE *, TR_X86SpineCheckSnippet *);
   void print(TR::FILE *, TR_X86ForceRecompilationSnippet *);
   void print(TR::FILE *, TR_X86RecompilationSnippet *);
#endif

   void print(TR::FILE *, TR_IA32ConstantDataSnippet *);
   void print(TR::FILE *, TR_IA32DataSnippet *);
   void print(TR::FILE *, TR_X86DivideCheckSnippet *);
   void print(TR::FILE *, TR_X86FPConvertToIntSnippet *);
   void print(TR::FILE *, TR_X86FPConvertToLongSnippet *);
   void print(TR::FILE *, TR_X86fbits2iSnippet *);
   void print(TR::FILE *, TR_X86GuardedDevirtualSnippet *);
   void print(TR::FILE *, TR_X86HelperCallSnippet *);
   void printBody(TR::FILE *, TR_X86HelperCallSnippet *, uint8_t *bufferPos);
   void print(TR::FILE *, TR_X86ScratchArgHelperCallSnippet *);


   void print(TR::FILE *, TR::UnresolvedDataSnippet *);

#ifdef TR_TARGET_64BIT
   uint8_t *printArgumentFlush(TR::FILE *, TR::Node *, bool, uint8_t *);
   void print(TR::FILE *, TR_AMD64FPConversionSnippet *);
#endif
#endif
#ifdef TR_TARGET_POWER
   void printPrefix(TR::FILE *, TR::Instruction *);

   void print(TR::FILE *, TR::PPCDepInstruction *);
   void print(TR::FILE *, TR::PPCLabelInstruction *);
   void print(TR::FILE *, TR::PPCDepLabelInstruction *);
   void print(TR::FILE *, TR::PPCConditionalBranchInstruction *);
   void print(TR::FILE *, TR::PPCDepConditionalBranchInstruction *);
   void print(TR::FILE *, TR::PPCAdminInstruction *);
   void print(TR::FILE *, TR::PPCImmInstruction *);
   void print(TR::FILE *, TR::PPCSrc1Instruction *);
   void print(TR::FILE *, TR::PPCDepImmInstruction *);
   void print(TR::FILE *, TR::PPCDepImmSymInstruction *);
   void print(TR::FILE *, TR::PPCTrg1Instruction *);
   void print(TR::FILE *, TR::PPCTrg1Src1Instruction *);
   void print(TR::FILE *, TR::PPCTrg1ImmInstruction *);
   void print(TR::FILE *, TR::PPCTrg1Src1ImmInstruction *);
   void print(TR::FILE *, TR::PPCTrg1Src1Imm2Instruction *);
   void print(TR::FILE *, TR::PPCSrc2Instruction *);
   void print(TR::FILE *, TR::PPCTrg1Src2Instruction *);
   void print(TR::FILE *, TR::PPCTrg1Src2ImmInstruction *);
   void print(TR::FILE *, TR::PPCTrg1Src3Instruction *);
   void print(TR::FILE *, TR::PPCMemSrc1Instruction *);
   void print(TR::FILE *, TR::PPCMemInstruction *);
   void print(TR::FILE *, TR::PPCTrg1MemInstruction *);
   void print(TR::FILE *, TR::PPCControlFlowInstruction *);
#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR::PPCVirtualGuardNOPInstruction *);
#endif

   TR::Instruction* getOutlinedTargetIfAny(TR::Instruction *instr);
   void printPPCOOLSequences(TR::FILE *pOutFile);

   const char * getPPCRegisterName(uint32_t regNum, bool useVSR = false);

   void print(TR::FILE *, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   void print(TR::FILE *, TR::RegisterDependency *);
   void print(TR::FILE *, TR::RegisterDependencyConditions *);
   void printPPCGCRegisterMap(TR::FILE *, TR::GCRegisterMap *);

   void print(TR::FILE *, TR::MemoryReference *, bool d_form = true);

   uint8_t * printEmitLoadPPCHelperAddrToCtr(TR::FILE *, uint8_t*, int32_t, TR::RealRegister *);
   uint8_t * printEmitLoadIndirectPPCHelperAddrToCtr(TR::FILE *, uint8_t*, TR::RealRegister *, TR::RealRegister *, int32_t);
   uint8_t * printPPCArgumentsFlush(TR::FILE *, TR::Node *, uint8_t *, int32_t);
   void printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr);

   void printp(TR::FILE *, TR::Snippet *);
   void print(TR::FILE *, TR_PPCMonitorEnterSnippet *);
   void print(TR::FILE *, TR_PPCMonitorExitSnippet *);
   void print(TR::FILE *, TR_PPCReadMonitorSnippet *);
   void print(TR::FILE *, TR_PPCHeapAllocSnippet *);
   void print(TR::FILE *, TR_PPCAllocPrefetchSnippet *);


   void print(TR::FILE *, TR::UnresolvedDataSnippet *);

#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR_PPCStackCheckFailureSnippet *);
   void print(TR::FILE *, TR_PPCInterfaceCastSnippet *);
   void print(TR::FILE *, TR_PPCUnresolvedCallSnippet *);
   void print(TR::FILE *, TR_PPCVirtualSnippet *);
   void print(TR::FILE *, TR_PPCVirtualUnresolvedSnippet *);
   void print(TR::FILE *, TR_PPCInterfaceCallSnippet *);
   void print(TR::FILE *, TR_PPCForceRecompilationSnippet *);
   void print(TR::FILE *, TR_PPCRecompilationSnippet *);
   void print(TR::FILE *, TR_PPCCallSnippet *);
#endif



   void printMemoryReferenceComment(TR::FILE *pOutFile, TR::MemoryReference *mr);
   void print(TR::FILE *, TR_PPCLockReservationEnterSnippet *);
   void print(TR::FILE *, TR_PPCLockReservationExitSnippet *);
   uint8_t* print(TR::FILE *pOutFile, TR_PPCArrayCopyCallSnippet *snippet, uint8_t *cursor);

#endif
#ifdef TR_TARGET_ARM
   char * fullOpCodeName(TR::Instruction *instr);
   void printPrefix(TR::FILE *, TR::Instruction *);
   void printBinaryPrefix(char *prefixBuffer, TR::Instruction *);
   void dumpDependencyGroup(TR::FILE *pOutFile, TR_ARMRegisterDependencyGroup *group, int32_t numConditions, char *prefix, bool omitNullDependencies);
   void dumpDependencies(TR::FILE *, TR::Instruction *);
   void print(TR::FILE *, TR_ARMLabelInstruction *);
#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR_ARMVirtualGuardNOPInstruction *);
#endif
   void print(TR::FILE *, TR_ARMAdminInstruction *);
   void print(TR::FILE *, TR_ARMImmInstruction *);
   void print(TR::FILE *, TR_ARMImmSymInstruction *);
   void print(TR::FILE *, TR_ARMTrg1Src2Instruction *);
   void print(TR::FILE *, TR_ARMTrg2Src1Instruction *);
   void print(TR::FILE *, TR_ARMMulInstruction *);
   void print(TR::FILE *, TR_ARMMemSrc1Instruction *);
   void print(TR::FILE *, TR_ARMTrg1Instruction *);
   void print(TR::FILE *, TR_ARMMemInstruction *);
   void print(TR::FILE *, TR_ARMTrg1MemInstruction *);
   void print(TR::FILE *, TR_ARMTrg1MemSrc1Instruction *);
   void print(TR::FILE *, TR_ARMControlFlowInstruction *);
   void print(TR::FILE *, TR_ARMMultipleMoveInstruction *);

   void print(TR::FILE *, TR::MemoryReference *);

   void print(TR::FILE *, TR_ARMOperand2 * op, TR_RegisterSizes size = TR_WordReg);

   const char * getNamea(TR::Snippet *);

   void print(TR::FILE *, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   void printARMGCRegisterMap(TR::FILE *, TR::GCRegisterMap *);
   void printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr);

   void printa(TR::FILE *, TR::Snippet *);
   void print(TR::FILE *, TR_ARMCallSnippet *);
   void print(TR::FILE *, TR_ARMUnresolvedCallSnippet *);
   void print(TR::FILE *, TR_ARMVirtualSnippet *);
   void print(TR::FILE *, TR_ARMVirtualUnresolvedSnippet *);
   void print(TR::FILE *, TR_ARMInterfaceCallSnippet *);
   void print(TR::FILE *, TR_ARMMonitorEnterSnippet *);
   void print(TR::FILE *, TR_ARMMonitorExitSnippet *);
   void print(TR::FILE *, TR_ARMStackCheckFailureSnippet *);
   void print(TR::FILE *, TR::UnresolvedDataSnippet *);
   void print(TR::FILE *, TR_ARMRecompilationSnippet *);
   void printARM(TR::FILE *, uint8_t *, uint8_t *);
#endif
#ifdef TR_TARGET_S390
   void printPrefix(TR::FILE *, TR::Instruction *);

   void printS390RegisterDependency(TR::FILE *pOutFile, TR::Register * virtReg, int realReg, bool uses, bool defs);

   TR::Instruction* getOutlinedTargetIfAny(TR::Instruction *instr);
   void printS390OOLSequences(TR::FILE *pOutFile);

   void dumpDependencies(TR::FILE *, TR::Instruction *, bool, bool);
   void printAssocRegDirective(TR::FILE *pOutFile, TR::Instruction * instr);
   void printz(TR::FILE *, TR::Instruction *);
   void printz(TR::FILE *, TR::Instruction *, const char *);
   void print(TR::FILE *, TR_S390LabelInstruction *);
   void print(TR::FILE *, TR_S390BranchInstruction *);
   void print(TR::FILE *, TR_S390BranchOnCountInstruction *);
#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR_S390VirtualGuardNOPInstruction *);
#endif
   void print(TR::FILE *, TR_S390BranchOnIndexInstruction *);
   void print(TR::FILE *, TR_S390ImmInstruction *);
   void print(TR::FILE *, TR_S390ImmSnippetInstruction *);
   void print(TR::FILE *, TR_S390ImmSymInstruction *);
   void print(TR::FILE *, TR_S390Imm2Instruction *);
   void print(TR::FILE *, TR_S390RegInstruction *);
   void print(TR::FILE *, TR_S390TranslateInstruction *);
   void print(TR::FILE *, TR_S390RRInstruction *);
   void print(TR::FILE *, TR_S390RRFInstruction *);
   void print(TR::FILE *, TR_S390RRRInstruction *);
   void print(TR::FILE *, TR_S390RIInstruction *);
   void print(TR::FILE *, TR_S390RILInstruction *);
   void print(TR::FILE *, TR_S390RSInstruction *);
   void print(TR::FILE *, TR_S390RSLInstruction *);
   void print(TR::FILE *, TR_S390RSLbInstruction *);
   void print(TR::FILE *, TR_S390MemInstruction *);
   void print(TR::FILE *, TR_S390SS1Instruction *);
   void print(TR::FILE *, TR_S390SS2Instruction *);
   void print(TR::FILE *, TR_S390SMIInstruction *);
   void print(TR::FILE *, TR_S390MIIInstruction *);
   void print(TR::FILE *, TR_S390SS4Instruction *);
   void print(TR::FILE *, TR_S390SSFInstruction *);
   void print(TR::FILE *, TR_S390SSEInstruction *);
   void print(TR::FILE *, TR_S390SIInstruction *);
   void print(TR::FILE *, TR_S390SILInstruction *);
   void print(TR::FILE *, TR_S390SInstruction *);
   void print(TR::FILE *, TR_S390RXInstruction *);
   void print(TR::FILE *, TR_S390RXEInstruction *);
   void print(TR::FILE *, TR_S390RXFInstruction *);
   void print(TR::FILE *, TR_S390AnnotationInstruction *);
   void print(TR::FILE *, TR_S390PseudoInstruction *);
   void print(TR::FILE *, TR_S390NOPInstruction *);
   void print(TR::FILE *, TR::MemoryReference *, TR::Instruction *);
   void print(TR::FILE *, TR_S390RRSInstruction *);
   void print(TR::FILE *, TR_S390RIEInstruction *);
   void print(TR::FILE *, TR_S390RISInstruction *);
   void print(TR::FILE *, TR_S390OpCodeOnlyInstruction *);
   void print(TR::FILE *, TR_S390IInstruction *);
   void print(TR::FILE *, TR_S390IEInstruction *);
   void print(TR::FILE *, TR_S390VRIInstruction *);
   void print(TR::FILE *, TR_S390VRRInstruction *);
   void print(TR::FILE *, TR_S390VStorageInstruction *);


   const char * getS390RegisterName(uint32_t regNum, bool isVRF = false);
   void print(TR::FILE *, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
   void printFullRegInfo(TR::FILE *, TR::RealRegister *);
   void printS390GCRegisterMap(TR::FILE *, TR::GCRegisterMap *);

   uint8_t * printS390ArgumentsFlush(TR::FILE *, TR::Node *, uint8_t *, int32_t);

   void printz(TR::FILE *, TR::Snippet *);
   void print(TR::FILE *, TR_S390CallSnippet *);

   void print(TR::FILE *, TR_S390ConstantDataSnippet *);
   void print(TR::FILE *, TR_S390TargetAddressSnippet *);

   void print(TR::FILE *, TR_S390LookupSwitchSnippet *);
   void print(TR::FILE *, TR_S390HelperCallSnippet *);
#ifdef J9_PROJECT_SPECIFIC
   void print(TR::FILE *, TR_S390ForceRecompilationSnippet *);
   void print(TR::FILE *, TR_S390ForceRecompilationDataSnippet *);
   void print(TR::FILE *, TR_S390UnresolvedCallSnippet *);
   void print(TR::FILE *, TR_S390VirtualSnippet *);
   void print(TR::FILE *, TR_S390VirtualUnresolvedSnippet *);
   void print(TR::FILE *, TR_S390InterfaceCallSnippet *);
#endif
   void print(TR::FILE *, TR_S390StackCheckFailureSnippet *);
   void print(TR::FILE *, TR::UnresolvedDataSnippet *);
   void print(TR::FILE *, TR_S390HeapAllocSnippet *);
   void print(TR::FILE *, TR_S390InterfaceCallDataSnippet *);
   void print(TR::FILE *, TR_S390JNICallDataSnippet *);
   void print(TR::FILE *, TR_S390WarmToColdTrampolineSnippet *);
   void print(TR::FILE *, TR_S390RestoreGPR7Snippet *);

   // Assembly related snippet display
   void printGPRegisterStatus(TR::FILE *pOutFile, TR::Machine *machine);
   void printFPRegisterStatus(TR::FILE *pOutFile, TR::Machine *machine);
   uint32_t getBitRegNum (TR::RealRegister *reg);

   void printInstructionComment(TR::FILE *pOutFile, int32_t tabStops, TR::Instruction *instr, bool needsStartComment = false);
   uint8_t *printLoadVMThreadInstruction(TR::FILE *pOutFile, uint8_t* cursor);
   uint8_t *printRuntimeInstrumentationOnOffInstruction(TR::FILE *pOutFile, uint8_t* cursor, bool isRION, bool isPrivateLinkage = false);
   const char *updateBranchName(const char * opCodeName, const char * brCondName);
#endif

   friend class TR_CFGChecker;

protected:
   TR::FILE                  * _file;
   TR::Compilation           * _comp;
   TR_FrontEnd               * _fe;

   uint32_t                   _nextLabelNumber;
   uint32_t                   _nextRegisterNumber;
   uint32_t                   _nextNodeNumber;
   uint32_t                   _nextSymbolNumber;
   uint32_t                   _nextInstructionNumber;
   uint32_t                   _nextStructureNumber;
   uint32_t                   _nextVariableSizeSymbolNumber;
   TR_CompilationFilters    * _compilationFilters;
   TR_CompilationFilters    * _relocationFilters;
   TR_CompilationFilters    * _inlineFilters;
   bool                       _usesSingleAllocMetaData;
   TR_BitVector               _nodeChecklist;
   TR_BitVector               _structureChecklist;

   TR::CodeGenerator *_cg;

   int32_t                    _lastFrequency;
   bool                       _isCold;
   bool                       _mainEntrySeen;
   TR::Node                  * _currentParent;
   int32_t                    _currentChildIndex;

#define TRACERA_IN_PROGRESS             (0x0001)
#define TRACERA_INSTRUCTION_INSERTED    (0x0002)
   uint16_t                   _registerAssignmentTraceFlags;
   uint16_t                   _pausedRegisterAssignmentTraceFlags;
   int16_t                    _registerAssignmentTraceCursor;
   TR_RegisterKinds           _registerKindsToAssign;

   const static uint32_t      DEFAULT_NODE_LENGTH = 82; //Tree information printed to the right of node OPCode will be aligned if node OPCode's length <= DEFAULT_NODE_LENGTH, or will follow the node OPCode immediately (non-aligned) otherwise.
   const static uint32_t      MAX_GLOBAL_INDEX_LENGTH = 5;
   };


class TR_PrettyPrinterString
   {
   public:
      TR_PrettyPrinterString(TR_Debug* debug);
      void append(const char* format, ...);
      const char* getStr() {return buffer;}
      int getLength() {return len;}
      void reset() {buffer[0] = '\0'; len = 0;}
      bool isEmpty() { return len <= 0; }
   private:
      char buffer[2000];
      int len;
      TR::Compilation *_comp;
      TR_Debug *_debug;
   };

typedef TR_Debug * (* TR_CreateDebug_t)(TR::Compilation *);
#endif