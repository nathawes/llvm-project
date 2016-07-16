//===-- HexagonSubtarget.cpp - Hexagon Subtarget Information --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Hexagon specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

#include "HexagonSubtarget.h"
#include "Hexagon.h"
#include "HexagonRegisterInfo.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include <map>

using namespace llvm;

#define DEBUG_TYPE "hexagon-subtarget"

#define GET_SUBTARGETINFO_CTOR
#define GET_SUBTARGETINFO_TARGET_DESC
#include "HexagonGenSubtargetInfo.inc"

static cl::opt<bool> EnableMemOps("enable-hexagon-memops",
  cl::Hidden, cl::ZeroOrMore, cl::ValueDisallowed, cl::init(true),
  cl::desc("Generate V4 MEMOP in code generation for Hexagon target"));

static cl::opt<bool> DisableMemOps("disable-hexagon-memops",
  cl::Hidden, cl::ZeroOrMore, cl::ValueDisallowed, cl::init(false),
  cl::desc("Do not generate V4 MEMOP in code generation for Hexagon target"));

static cl::opt<bool> EnableIEEERndNear("enable-hexagon-ieee-rnd-near",
  cl::Hidden, cl::ZeroOrMore, cl::init(false),
  cl::desc("Generate non-chopped conversion from fp to int."));

static cl::opt<bool> EnableBSBSched("enable-bsb-sched",
  cl::Hidden, cl::ZeroOrMore, cl::init(true));

static cl::opt<bool> EnableHexagonHVXDouble("enable-hexagon-hvx-double",
  cl::Hidden, cl::ZeroOrMore, cl::init(false),
  cl::desc("Enable Hexagon Double Vector eXtensions"));

static cl::opt<bool> EnableHexagonHVX("enable-hexagon-hvx",
  cl::Hidden, cl::ZeroOrMore, cl::init(false),
  cl::desc("Enable Hexagon Vector eXtensions"));

static cl::opt<bool> EnableTCLatencySched("enable-tc-latency-sched",
  cl::Hidden, cl::ZeroOrMore, cl::init(false));

static cl::opt<bool> EnableDotCurSched("enable-cur-sched",
  cl::Hidden, cl::ZeroOrMore, cl::init(true),
  cl::desc("Enable the scheduler to generate .cur"));

static cl::opt<bool> EnableVecFrwdSched("enable-evec-frwd-sched",
  cl::Hidden, cl::ZeroOrMore, cl::init(true));

static cl::opt<bool> DisableHexagonMISched("disable-hexagon-misched",
  cl::Hidden, cl::ZeroOrMore, cl::init(false),
  cl::desc("Disable Hexagon MI Scheduling"));

static cl::opt<bool> EnableSubregLiveness("hexagon-subreg-liveness",
  cl::Hidden, cl::ZeroOrMore, cl::init(false),
  cl::desc("Enable subregister liveness tracking for Hexagon"));

void HexagonSubtarget::initializeEnvironment() {
  UseMemOps = false;
  ModeIEEERndNear = false;
  UseBSBScheduling = false;
}

HexagonSubtarget &
HexagonSubtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS) {
  CPUString = HEXAGON_MC::selectHexagonCPU(getTargetTriple(), CPU);

  static std::map<StringRef, HexagonArchEnum> CpuTable {
    { "hexagonv4", V4 },
    { "hexagonv5", V5 },
    { "hexagonv55", V55 },
    { "hexagonv60", V60 },
  };

  auto foundIt = CpuTable.find(CPUString);
  if (foundIt != CpuTable.end())
    HexagonArchVersion = foundIt->second;
  else
    llvm_unreachable("Unrecognized Hexagon processor version");

  UseHVXOps = false;
  UseHVXDblOps = false;
  ParseSubtargetFeatures(CPUString, FS);

  if (EnableHexagonHVX.getPosition())
    UseHVXOps = EnableHexagonHVX;
  if (EnableHexagonHVXDouble.getPosition())
    UseHVXDblOps = EnableHexagonHVXDouble;

  return *this;
}

HexagonSubtarget::HexagonSubtarget(const Triple &TT, StringRef CPU,
                                   StringRef FS, const TargetMachine &TM)
    : HexagonGenSubtargetInfo(TT, CPU, FS), CPUString(CPU),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)), TLInfo(TM, *this),
      FrameLowering() {

  initializeEnvironment();

  // Initialize scheduling itinerary for the specified CPU.
  InstrItins = getInstrItineraryForCPU(CPUString);

  // UseMemOps on by default unless disabled explicitly
  if (DisableMemOps)
    UseMemOps = false;
  else if (EnableMemOps)
    UseMemOps = true;
  else
    UseMemOps = false;

  if (EnableIEEERndNear)
    ModeIEEERndNear = true;
  else
    ModeIEEERndNear = false;

  UseBSBScheduling = hasV60TOps() && EnableBSBSched;
}


void HexagonSubtarget::HexagonDAGMutation::apply(ScheduleDAGInstrs *DAG) {
  for (auto &SU : DAG->SUnits) {
    if (!SU.isInstr())
      continue;
    SmallVector<SDep, 4> Erase;
    for (auto &D : SU.Preds)
      if (D.getKind() == SDep::Output && D.getReg() == Hexagon::USR_OVF)
        Erase.push_back(D);
    for (auto &E : Erase)
      SU.removePred(E);
  }

  for (auto &SU : DAG->SUnits) {
    // Update the latency of chain edges between v60 vector load or store
    // instructions to be 1. These instructions cannot be scheduled in the
    // same packet.
    MachineInstr *MI1 = SU.getInstr();
    auto *QII = static_cast<const HexagonInstrInfo*>(DAG->TII);
    bool IsStoreMI1 = MI1->mayStore();
    bool IsLoadMI1 = MI1->mayLoad();
    if (!QII->isV60VectorInstruction(MI1) || !(IsStoreMI1 || IsLoadMI1))
      continue;
    for (auto &SI : SU.Succs) {
      if (SI.getKind() != SDep::Order || SI.getLatency() != 0)
        continue;
      MachineInstr *MI2 = SI.getSUnit()->getInstr();
      if (!QII->isV60VectorInstruction(MI2))
        continue;
      if ((IsStoreMI1 && MI2->mayStore()) || (IsLoadMI1 && MI2->mayLoad())) {
        SI.setLatency(1);
        SU.setHeightDirty();
        // Change the dependence in the opposite direction too.
        for (auto &PI : SI.getSUnit()->Preds) {
          if (PI.getSUnit() != &SU || PI.getKind() != SDep::Order)
            continue;
          PI.setLatency(1);
          SI.getSUnit()->setDepthDirty();
        }
      }
    }
  }
}


void HexagonSubtarget::getPostRAMutations(
      std::vector<std::unique_ptr<ScheduleDAGMutation>> &Mutations) const {
  Mutations.push_back(make_unique<HexagonSubtarget::HexagonDAGMutation>());
}


// Pin the vtable to this file.
void HexagonSubtarget::anchor() {}

bool HexagonSubtarget::enableMachineScheduler() const {
  if (DisableHexagonMISched.getNumOccurrences())
    return !DisableHexagonMISched;
  return true;
}

bool HexagonSubtarget::enableSubRegLiveness() const {
  return EnableSubregLiveness;
}

// This helper function is responsible for increasing the latency only.
void HexagonSubtarget::updateLatency(MachineInstr *SrcInst,
      MachineInstr *DstInst, SDep &Dep) const {
  if (!hasV60TOps())
    return;

  auto &QII = static_cast<const HexagonInstrInfo&>(*getInstrInfo());

  if (EnableVecFrwdSched && QII.addLatencyToSchedule(SrcInst, DstInst)) {
    // Vec frwd scheduling.
    Dep.setLatency(Dep.getLatency() + 1);
  } else if (useBSBScheduling() &&
             QII.isLateInstrFeedsEarlyInstr(SrcInst, DstInst)) {
    // BSB scheduling.
    Dep.setLatency(Dep.getLatency() + 1);
  } else if (EnableTCLatencySched) {
    // TClass latency scheduling.
    // Check if SrcInst produces in 2C an operand of DstInst taken in stage 2B.
    if (QII.isTC1(SrcInst) || QII.isTC2(SrcInst))
      if (!QII.isTC1(DstInst) && !QII.isTC2(DstInst))
        Dep.setLatency(Dep.getLatency() + 1);
  }
}

// Return true if these are the best two instructions to schedule
// together with a zero latency. Only one dependence should have a zero
// latency. If there are multiple choices, choose the best, and change
// ther others, if needed.
bool HexagonSubtarget::isBestZeroLatency(SUnit *Src, SUnit *Dst,
      const HexagonInstrInfo *TII) const {
  MachineInstr *SrcInst = Src->getInstr();
  MachineInstr *DstInst = Dst->getInstr();
  // Check if the instructions can be scheduled together.
  assert((TII->isToBeScheduledASAP(SrcInst, DstInst) ||
          TII->canExecuteInBundle(SrcInst, DstInst)) &&
         "Unable to schedule instructions together.");

  if (SrcInst->isPHI() || DstInst->isPHI())
    return false;

  // Look for the best candidate to schedule together. If there are
  // multiple choices, then the best candidate is the one with the
  // greatest height, i.e., longest critical path.
  SUnit *Best = Dst;
  SUnit *PrevBest = nullptr;
  for (const SDep &SI : Src->Succs) {
    if (!SI.isAssignedRegDep())
      continue;
    if (SI.getLatency() == 0)
      PrevBest = SI.getSUnit();
    MachineInstr *Inst = SI.getSUnit()->getInstr();
    if (!TII->isToBeScheduledASAP(SrcInst, Inst) ||
        !TII->canExecuteInBundle(SrcInst, Inst))
      continue;
    if (SI.getSUnit()->getHeight() > Best->getHeight())
      Best = SI.getSUnit();
  }

  // Reassign the latency for the previous best, which requires setting
  // the dependence edge in both directions.
  if (Best != PrevBest) {
    for (SDep &SI : Src->Succs) {
      if (SI.getSUnit() != PrevBest)
        continue;
      SI.setLatency(1);
      updateLatency(SrcInst, DstInst, SI);
      // Update the latency of the predecessor edge too.
      for (SDep &PI : PrevBest->Preds) {
        if (PI.getSUnit() != Src || !PI.isAssignedRegDep())
          continue;
        PI.setLatency(1);
        updateLatency(SrcInst, DstInst, PI);
      }
    }
  }

  return Best == Dst;
}

// Update the latency of a Phi when the Phi bridges two instructions that
// require a multi-cycle latency.
void HexagonSubtarget::changePhiLatency(MachineInstr *SrcInst, SUnit *Dst,
      SDep &Dep) const {
  if (!SrcInst->isPHI() || Dst->NumPreds == 0 || Dep.getLatency() != 0)
    return;

  for (const SDep &PI : Dst->Preds) {
    if (PI.getLatency() != 0)
      continue;
    Dep.setLatency(2);
    break;
  }
}

/// \brief Perform target specific adjustments to the latency of a schedule
/// dependency.
void HexagonSubtarget::adjustSchedDependency(SUnit *Src, SUnit *Dst,
                                             SDep &Dep) const {
  MachineInstr *SrcInst = Src->getInstr();
  MachineInstr *DstInst = Dst->getInstr();
  if (!Src->isInstr() || !Dst->isInstr())
    return;

  const HexagonInstrInfo *QII = static_cast<const HexagonInstrInfo *>(getInstrInfo());

  // Instructions with .new operands have zero latency.
  if (QII->canExecuteInBundle(SrcInst, DstInst) &&
      isBestZeroLatency(Src, Dst, QII)) {
    Dep.setLatency(0);
    return;
  }

  if (!hasV60TOps())
    return;

  // Don't adjust the latency of post-increment part of the instruction.
  if (QII->isPostIncrement(SrcInst) && Dep.isAssignedRegDep()) {
    if (SrcInst->mayStore())
      return;
    if (Dep.getReg() != SrcInst->getOperand(0).getReg())
      return;
  } else if (QII->isPostIncrement(DstInst) && Dep.getKind() == SDep::Anti) {
    if (DstInst->mayStore())
      return;
    if (Dep.getReg() != DstInst->getOperand(0).getReg())
      return;
  } else if (QII->isPostIncrement(DstInst) && DstInst->mayStore() &&
             Dep.isAssignedRegDep()) {
    MachineOperand &Op = DstInst->getOperand(DstInst->getNumOperands() - 1);
    if (Op.isReg() && Dep.getReg() != Op.getReg())
      return;
  }

  // Check if we need to change any the latency values when Phis are added.
  if (useBSBScheduling() && SrcInst->isPHI()) {
    changePhiLatency(SrcInst, Dst, Dep);
    return;
  }

  // Try to schedule uses near definitions to generate .cur.
  if (EnableDotCurSched && QII->isToBeScheduledASAP(SrcInst, DstInst) &&
      isBestZeroLatency(Src, Dst, QII)) {
    Dep.setLatency(0);
    return;
  }

  updateLatency(SrcInst, DstInst, Dep);
}

