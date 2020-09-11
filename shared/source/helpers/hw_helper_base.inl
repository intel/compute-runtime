/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_interface.h"

#include "opencl/source/aub_mem_dump/aub_mem_dump.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/hardware_commands_helper.h"

#include "pipe_control_args.h"

namespace NEO {

template <typename Family>
const aub_stream::EngineType HwHelperHw<Family>::lowPriorityEngineType = aub_stream::EngineType::ENGINE_RCS;

template <typename Family>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Builtin;

template <typename Family>
bool HwHelperHw<Family>::obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const {
    return size > KB;
}

template <typename Family>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 16384;
    caps->image3DMaxWidth = 16384;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
}

template <typename Family>
bool HwHelperHw<Family>::isL3Configurable(const HardwareInfo &hwInfo) {
    return PreambleHelper<Family>::isL3Configurable(hwInfo);
}

template <typename Family>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsr;
}

template <typename Family>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) {
    return 4u;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getMaxNumSamplers() const {
    return 16;
}

template <typename Family>
const AubMemDump::LrcaHelper &HwHelperHw<Family>::getCsTraits(aub_stream::EngineType engineType) const {
    return *AUBFamilyMapper<Family>::csTraits[engineType];
}

template <typename Family>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool HwHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    return true;
}

template <typename Family>
void HwHelperHw<Family>::setRenderSurfaceStateForBuffer(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                        void *surfaceStateBuffer,
                                                        size_t bufferSize,
                                                        uint64_t gpuVa,
                                                        size_t offset,
                                                        uint32_t pitch,
                                                        GraphicsAllocation *gfxAlloc,
                                                        bool isReadOnly,
                                                        uint32_t surfaceType,
                                                        bool forceNonAuxMode,
                                                        bool useL1Cache) {
    using RENDER_SURFACE_STATE = typename Family::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuffer);
    RENDER_SURFACE_STATE state = Family::cmdInitRenderSurfaceState;
    auto surfaceSize = alignUp(bufferSize, 4);

    SURFACE_STATE_BUFFER_LENGTH Length = {0};
    Length.Length = static_cast<uint32_t>(surfaceSize - 1);

    state.setWidth(Length.SurfaceState.Width + 1);
    state.setHeight(Length.SurfaceState.Height + 1);
    state.setDepth(Length.SurfaceState.Depth + 1);
    if (pitch) {
        state.setSurfacePitch(pitch);
    }

    // The graphics allocation for Host Ptr surface will be created in makeResident call and GPU address is expected to be the same as CPU address
    auto bufferStateAddress = (gfxAlloc != nullptr) ? gfxAlloc->getGpuAddress() : gpuVa;
    bufferStateAddress += offset;

    auto bufferStateSize = (gfxAlloc != nullptr) ? gfxAlloc->getUnderlyingBufferSize() : bufferSize;

    state.setSurfaceType(static_cast<typename RENDER_SURFACE_STATE::SURFACE_TYPE>(surfaceType));

    state.setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_RAW);
    state.setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    state.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4);

    state.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    state.setVerticalLineStride(0);
    state.setVerticalLineStrideOffset(0);
    if ((isAligned<MemoryConstants::cacheLineSize>(bufferStateAddress) && isAligned<MemoryConstants::cacheLineSize>(bufferStateSize)) ||
        isReadOnly) {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

    state.setSurfaceBaseAddress(bufferStateAddress);

    Gmm *gmm = gfxAlloc ? gfxAlloc->getDefaultGmm() : nullptr;
    if (gmm && gmm->isRenderCompressed && !forceNonAuxMode) {
        // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
        state.setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
        state.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    } else {
        state.setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
        state.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
    *surfaceState = state;

    setL1CachePolicy(useL1Cache, surfaceState, rootDeviceEnvironment.getHardwareInfo());
}

template <typename GfxFamily>
void NEO::HwHelperHw<GfxFamily>::setL1CachePolicy(bool useL1Cache, typename GfxFamily::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) {}

template <typename Family>
bool HwHelperHw<Family>::getEnableLocalMemory(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.EnableLocalMemory.get() != -1) {
        return DebugManager.flags.EnableLocalMemory.get();
    } else if (DebugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
        return true;
    }

    return OSInterface::osEnableLocalMemory && isLocalMemoryEnabled(hwInfo);
}

template <typename Family>
AuxTranslationMode HwHelperHw<Family>::getAuxTranslationMode() {
    if (DebugManager.flags.ForceAuxTranslationMode.get() != -1) {
        return static_cast<AuxTranslationMode>(DebugManager.flags.ForceAuxTranslationMode.get());
    }

    return HwHelperHw<Family>::defaultAuxTranslationMode;
}

template <typename Family>
bool HwHelperHw<Family>::isBlitAuxTranslationRequired(const HardwareInfo &hwInfo, const MultiDispatchInfo &multiDispatchInfo) {
    return (HwHelperHw<Family>::getAuxTranslationMode() == AuxTranslationMode::Blit) &&
           hwInfo.capabilityTable.blitterOperationsSupported &&
           multiDispatchInfo.getMemObjsForAuxTranslation() &&
           (multiDispatchInfo.getMemObjsForAuxTranslation()->size() > 0);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
    LinearStream &commandStream,
    POST_SYNC_OPERATION operation,
    uint64_t gpuAddress,
    uint64_t immediateData,
    const HardwareInfo &hwInfo,
    PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    addPipeControlWA(commandStream, gpuAddress, hwInfo);

    setPostSyncExtraProperties(args, hwInfo);
    addPipeControlWithPostSync(commandStream, operation, gpuAddress, immediateData, args);

    MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, gpuAddress, hwInfo);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addPipeControlWithPostSync(
    LinearStream &commandStream,
    POST_SYNC_OPERATION operation,
    uint64_t gpuAddress,
    uint64_t immediateData,
    PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
    setPipeControl(cmd, args);
    cmd.setPostSyncOperation(operation);
    cmd.setAddress(static_cast<uint32_t>(gpuAddress & 0x0000FFFFFFFFULL));
    cmd.setAddressHigh(static_cast<uint32_t>(gpuAddress >> 32));
    if (operation == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
        cmd.setImmediateData(immediateData);
    }

    PIPE_CONTROL *pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = cmd;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setPipeControl(typename GfxFamily::PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setCommandStreamerStallEnable(true);
    pipeControl.setDcFlushEnable(args.dcFlushEnable);
    pipeControl.setConstantCacheInvalidationEnable(args.constantCacheInvalidationEnable);
    pipeControl.setInstructionCacheInvalidateEnable(args.instructionCacheInvalidateEnable);
    pipeControl.setPipeControlFlushEnable(args.pipeControlFlushEnable);
    pipeControl.setRenderTargetCacheFlushEnable(args.renderTargetCacheFlushEnable);
    pipeControl.setStateCacheInvalidationEnable(args.stateCacheInvalidationEnable);
    pipeControl.setTextureCacheInvalidationEnable(args.textureCacheInvalidationEnable);
    pipeControl.setVfCacheInvalidationEnable(args.vfCacheInvalidationEnable);
    pipeControl.setGenericMediaStateClear(args.genericMediaStateClear);

    setPipeControlExtraProperties(pipeControl, args);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pipeControl.setDcFlushEnable(true);
        pipeControl.setRenderTargetCacheFlushEnable(true);
        pipeControl.setInstructionCacheInvalidateEnable(true);
        pipeControl.setTextureCacheInvalidationEnable(true);
        pipeControl.setPipeControlFlushEnable(true);
        pipeControl.setVfCacheInvalidationEnable(true);
        pipeControl.setConstantCacheInvalidationEnable(true);
        pipeControl.setStateCacheInvalidationEnable(true);
    }
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addPipeControl(LinearStream &commandStream, PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
    MemorySynchronizationCommands<GfxFamily>::setPipeControl(cmd, args);
    auto pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = cmd;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addPipeControlWithCSStallOnly(LinearStream &commandStream, PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
    cmd.setCommandStreamerStallEnable(true);
    auto pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = cmd;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl() {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo) {
    const auto pipeControlCount = HardwareCommandsHelper<GfxFamily>::isPipeControlWArequired(hwInfo) ? 2u : 1u;
    return pipeControlCount * getSizeForSinglePipeControl() + getSizeForAdditonalSynchronization(hwInfo);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleSynchronization(const HardwareInfo &hwInfo) {
    return 0u;
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo) {
    return 0u;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::requiresAuxResolves() const {
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) {
    if (DebugManager.flags.ForceLinearImages.get() || forceLinearStorage || isSharedContext) {
        return false;
    }
    return !isImage1d;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::alignSlmSize(uint32_t slmSize) {
    if (slmSize == 0u) {
        return 0u;
    }
    slmSize = std::max(slmSize, 1024u);
    slmSize = Math::nextPowerOfTwo(slmSize);
    UNRECOVERABLE_IF(slmSize > 64u * KB);
    return slmSize;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::computeSlmValues(uint32_t slmSize) {
    auto value = std::max(slmSize, 1024u);
    value = Math::nextPowerOfTwo(value);
    value = Math::getMinLsbSet(value);
    value = value - 9;
    DEBUG_BREAK_IF(value > 7);
    return value * !!slmSize;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getBarriersCountFromHasBarriers(uint32_t hasBarriers) {
    return hasBarriers;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getSteppingFromHwRevId(uint32_t hwRevId, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const {
    auto lowestHwRevIdWithBug = getHwRevIdFromStepping(lowestSteppingWithBug, hwInfo);
    auto hwRevIdWithFix = getHwRevIdFromStepping(steppingWithFix, hwInfo);
    if ((lowestHwRevIdWithBug == CommonConstants::invalidStepping) || (hwRevIdWithFix == CommonConstants::invalidStepping)) {
        return false;
    }
    return (lowestHwRevIdWithBug <= hwInfo.platform.usRevId && hwInfo.platform.usRevId < hwRevIdWithFix);
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
inline uint32_t HwHelperHw<GfxFamily>::getMinimalSIMDSize() {
    return 8u;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    return HwHelper::getMaxThreadsForWorkgroup(hwInfo, maxNumEUsPerSubSlice);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isSpecialWorkgroupSizeRequired(const HardwareInfo &hwInfo, bool isSimulation) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::allowRenderCompression(const HardwareInfo &hwInfo) const {
    return true;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isBlitCopyRequiredForLocalMemory(const HardwareInfo &hwInfo) const {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return (hwHelper.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::forceBlitterUseForGlobalBuffers(const HardwareInfo &hwInfo, GraphicsAllocation *allocation) const {
    return false;
}

template <typename GfxFamily>
LocalMemoryAccessMode HwHelperHw<GfxFamily>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    switch (static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get())) {
    case LocalMemoryAccessMode::Default:
    case LocalMemoryAccessMode::CpuAccessAllowed:
    case LocalMemoryAccessMode::CpuAccessDisallowed:
        return static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get());
    }
    return getDefaultLocalMemoryAccessMode(hwInfo);
}

template <typename GfxFamily>
inline LocalMemoryAccessMode HwHelperHw<GfxFamily>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush() {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addFullCacheFlush(LinearStream &commandStream) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL *pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;

    PipeControlArgs args(true);
    args.renderTargetCacheFlushEnable = true;
    args.instructionCacheInvalidateEnable = true;
    args.textureCacheInvalidationEnable = true;
    args.pipeControlFlushEnable = true;
    args.constantCacheInvalidationEnable = true;
    args.stateCacheInvalidationEnable = true;
    MemorySynchronizationCommands<GfxFamily>::setPipeControl(cmd, args);
    MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(cmd);
    *pipeControl = cmd;
}

template <typename GfxFamily>
const StackVec<size_t, 3> HwHelperHw<GfxFamily>::getDeviceSubGroupSizes() const {
    return {8, 16, 32};
}

template <typename GfxFamily>
const StackVec<uint32_t, 6> HwHelperHw<GfxFamily>::getThreadsPerEUConfigs() const {
    return {};
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const HardwareInfo &hwInfo) const {}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isBankOverrideRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getDefaultThreadArbitrationPolicy() const {
    return 0;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::useOnlyGlobalTimestamps() const {
    return false;
}
} // namespace NEO
