/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/libult/mock_gfx_family.h"
#include "runtime/command_queue/gpgpu_walker.inl"
#include "runtime/command_queue/gpgpu_walker_base.inl"
#include "runtime/command_stream/preemption.inl"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw.inl"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/helpers/hw_helper_common.inl"
#include "runtime/helpers/kernel_commands.inl"
#include "runtime/helpers/kernel_commands_base.inl"
#include "runtime/helpers/preamble.inl"

namespace OCLRT {

static AubMemDump::LrcaHelperRcs rcs(0x000000);
static AubMemDump::LrcaHelperBcs bcs(0x020000);
static AubMemDump::LrcaHelperVcs vcs(0x010000);
static AubMemDump::LrcaHelperVecs vecs(0x018000);

const AubMemDump::LrcaHelper *AUBFamilyMapper<GENX>::csTraits[EngineType::NUM_ENGINES] = {
    &rcs,
    &bcs,
    &vcs,
    &vecs};

const MMIOList AUBFamilyMapper<GENX>::globalMMIO;

bool (*GENX::isSimulationFcn)(unsigned short) = nullptr;

GENX::GPGPU_WALKER GENX::cmdInitGpgpuWalker = GENX::GPGPU_WALKER::sInit();
GENX::INTERFACE_DESCRIPTOR_DATA GENX::cmdInitInterfaceDescriptorData = GENX::INTERFACE_DESCRIPTOR_DATA::sInit();
GENX::MEDIA_STATE_FLUSH GENX::cmdInitMediaStateFlush = GENX::MEDIA_STATE_FLUSH::sInit();
GENX::MEDIA_INTERFACE_DESCRIPTOR_LOAD GENX::cmdInitMediaInterfaceDescriptorLoad = GENX::MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();
GENX::MI_SEMAPHORE_WAIT GENX::cmdInitMiSemaphoreWait = GENX::MI_SEMAPHORE_WAIT::sInit();
GENX::RENDER_SURFACE_STATE GENX::cmdInitRenderSurfaceState = GENX::RENDER_SURFACE_STATE::sInit();
GENX::MI_LOAD_REGISTER_IMM GENX::cmdInitLoadRegisterImm = GENX::MI_LOAD_REGISTER_IMM::sInit();
GENX::MI_LOAD_REGISTER_REG GENX::cmdInitLoadRegisterReg = GENX::MI_LOAD_REGISTER_REG::sInit();
GENX::MI_LOAD_REGISTER_MEM GENX::cmdInitLoadRegisterMem = GENX::MI_LOAD_REGISTER_MEM::sInit();
GENX::MI_STORE_DATA_IMM GENX::cmdInitStoreDataImm = GENX::MI_STORE_DATA_IMM::sInit();
GENX::MI_STORE_REGISTER_MEM GENX::cmdInitStoreRegisterMem = GENX::MI_STORE_REGISTER_MEM::sInit();
GENX::MI_NOOP GENX::cmdInitNoop = GENX::MI_NOOP::sInit();
GENX::MI_BATCH_BUFFER_START GENX::cmdInitBatchBufferStart = GENX::MI_BATCH_BUFFER_START::sInit();
GENX::MI_BATCH_BUFFER_END GENX::cmdInitBatchBufferEnd = GENX::MI_BATCH_BUFFER_END::sInit();
GENX::MI_REPORT_PERF_COUNT GENX::cmdInitReportPerfCount = GENX::MI_REPORT_PERF_COUNT::sInit();
GENX::MI_ATOMIC GENX::cmdInitAtomic = GENX::MI_ATOMIC::sInit();
GENX::PIPELINE_SELECT GENX::cmdInitPipelineSelect = GENX::PIPELINE_SELECT::sInit();
GENX::MI_ARB_CHECK GENX::cmdInitArbCheck = GENX::MI_ARB_CHECK::sInit();
GENX::MEDIA_VFE_STATE GENX::cmdInitMediaVfeState = GENX::MEDIA_VFE_STATE::sInit();
GENX::STATE_BASE_ADDRESS GENX::cmdInitStateBaseAddress = GENX::STATE_BASE_ADDRESS::sInit();
GENX::MEDIA_SURFACE_STATE GENX::cmdInitMediaSurfaceState = GENX::MEDIA_SURFACE_STATE::sInit();
GENX::SAMPLER_STATE GENX::cmdInitSamplerState = GENX::SAMPLER_STATE::sInit();
GENX::GPGPU_CSR_BASE_ADDRESS GENX::cmdInitGpgpuCsrBaseAddress = GENX::GPGPU_CSR_BASE_ADDRESS::sInit();
GENX::STATE_SIP GENX::cmdInitStateSip = GENX::STATE_SIP::sInit();
GENX::BINDING_TABLE_STATE GENX::cmdInitBindingTableState = GENX::BINDING_TABLE_STATE::sInit();
GENX::MI_USER_INTERRUPT GENX::cmdInitUserInterrupt = GENX::MI_USER_INTERRUPT::sInit();
bool GENX::enabledYTiling = true;

template <>
size_t HwHelperHw<GENX>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <>
void HwHelperHw<GENX>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(pHwInfo->pPlatform);
    if (pPlatform->usDeviceID == 20) {
        coherencyFlag = false;
    } else {
        coherencyFlag = true;
    }
}

template <>
bool HwHelperHw<GENX>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    pHwInfo->capabilityTable.whitelistedRegisters.csChicken1_0x2580 = enable;
    return enable;
}
template <>
const AubMemDump::LrcaHelper &HwHelperHw<GENX>::getCsTraits(EngineInstanceT engineInstance) const {
    return *AUBFamilyMapper<GENX>::csTraits[engineInstance.type];
}

template <>
bool HwHelperHw<GENX>::supportsYTiling() const {
    return GENX::enabledYTiling;
}

template class HwHelperHw<GENX>;

struct hw_helper_static_init {
    hw_helper_static_init() {
        hwHelperFactory[IGFX_UNKNOWN_CORE] = &HwHelperHw<GENX>::get();
    }
};

hw_helper_static_init si;

template class GpgpuWalkerHelper<GENX>;

template <>
bool KernelCommandsHelper<GENX>::isPipeControlWArequired() {
    return false;
}

template struct KernelCommandsHelper<GENX>;

template <>
size_t PreemptionHelper::getRequiredCmdStreamSize<GENX>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    return 0;
}

template <>
void PreemptionHelper::programCmdStream<GENX>(LinearStream &cmdStream, PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                              GraphicsAllocation *preemptionCsr, Device &device) {
}

template <>
size_t PreemptionHelper::getRequiredPreambleSize<GENX>(const Device &device) {
    return 0;
}

template <>
size_t PreemptionHelper::getRequiredStateSipCmdSize<GENX>(const Device &device) {
    return 0;
}

template <>
void PreemptionHelper::programStateSip<GENX>(LinearStream &preambleCmdStream, Device &device) {
}

template <>
size_t PreemptionHelper::getPreemptionWaCsSize<GENX>(const Device &device) {
    return 0;
}

template void PreemptionHelper::programInterfaceDescriptorDataPreemption<GENX>(INTERFACE_DESCRIPTOR_DATA<GENX> *idd, PreemptionMode preemptionMode);

template <>
size_t DeviceQueueHw<GENX>::getWaCommandsSize() {
    return (size_t)0;
}

template <>
void DeviceQueueHw<GENX>::addArbCheckCmdWa() {
}

template <>
void DeviceQueueHw<GENX>::addMiAtomicCmdWa(uint64_t atomicOpPlaceholder) {
}

template <>
void DeviceQueueHw<GENX>::addLriCmdWa(bool setArbCheck) {
}

template <>
void DeviceQueueHw<GENX>::addPipeControlCmdWa(bool isNoopCmd) {
}

template <>
void DeviceQueueHw<GENX>::addProfilingEndCmds(uint64_t timestampAddress) {
}

template class DeviceQueueHw<GENX>;

template <>
void PreambleHelper<GENX>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
}

template <>
uint32_t PreambleHelper<GENX>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;
    return l3Config;
}

template <>
void PreambleHelper<GENX>::programPipelineSelect(LinearStream *pCommandStream, const DispatchFlags &dispatchFlags) {
}

template <>
struct L3CNTLRegisterOffset<GENX> {
    static const uint32_t registerOffset = 0x7034;
};

template struct PreambleHelper<GENX>;

} // namespace OCLRT
