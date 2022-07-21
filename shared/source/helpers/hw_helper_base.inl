/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"

namespace NEO {

template <typename Family>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Builtin;

template <typename Family>
bool HwHelperHw<Family>::isBufferSizeSuitableForCompression(const size_t size, const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.OverrideBufferSuitableForRenderCompression.get() != -1) {
        return !!DebugManager.flags.OverrideBufferSuitableForRenderCompression.get();
    }
    return size > KB;
}

template <typename Family>
size_t HwHelperHw<Family>::getMax3dImageWidthOrHeight() const {
    return 16384;
}

template <typename Family>
uint64_t HwHelperHw<Family>::getMaxMemAllocSize() const {
    // With stateful messages we have an allocation cap of 4GB
    // Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching
    return (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
}

template <typename Family>
bool HwHelperHw<Family>::isStatelessToStatefulWithOffsetSupported() const {
    return true;
}

template <typename Family>
bool HwHelperHw<Family>::isL3Configurable(const HardwareInfo &hwInfo) {
    return PreambleHelper<Family>::isL3Configurable(hwInfo);
}

template <typename Family>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) const {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return DebugManager.flags.UseBindlessDebugSip.get() ? SipKernelType::DbgBindless : SipKernelType::DbgCsr;
}

template <typename Family>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
size_t HwHelperHw<Family>::getPaddingForISAAllocation() const {
    if (DebugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 512 + (MemoryConstants::pageSize * DebugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 512;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) const {
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
bool HwHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    return true;
}

template <typename Family>
void HwHelperHw<Family>::setRenderSurfaceStateForScratchResource(const RootDeviceEnvironment &rootDeviceEnvironment,
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

    SURFACE_STATE_BUFFER_LENGTH length = {0};
    length.Length = static_cast<uint32_t>(surfaceSize - 1);

    state.setWidth(length.SurfaceState.Width + 1);
    state.setHeight(length.SurfaceState.Height + 1);
    state.setDepth(length.SurfaceState.Depth + 1);
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
    state.setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT);

    state.setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    state.setVerticalLineStride(0);
    state.setVerticalLineStrideOffset(0);
    if ((isAligned<MemoryConstants::cacheLineSize>(bufferStateAddress) && isAligned<MemoryConstants::cacheLineSize>(bufferStateSize)) ||
        isReadOnly) {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        state.setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }
    if (DebugManager.flags.OverrideMocsIndexForScratchSpace.get() != -1) {
        auto mocsIndex = static_cast<uint32_t>(DebugManager.flags.OverrideMocsIndexForScratchSpace.get()) << 1;
        state.setMemoryObjectControlState(mocsIndex);
    }

    state.setSurfaceBaseAddress(bufferStateAddress);

    bool isCompressionEnabled = gfxAlloc ? gfxAlloc->isCompressionEnabled() : false;
    if (isCompressionEnabled && !forceNonAuxMode) {
        // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
        EncodeSurfaceState<Family>::setCoherencyType(&state, RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
        EncodeSurfaceState<Family>::setBufferAuxParamsForCCS(&state);
    } else {
        EncodeSurfaceState<Family>::setCoherencyType(&state, RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
        state.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
    setL1CachePolicy(useL1Cache, &state, rootDeviceEnvironment.getHardwareInfo());

    *surfaceState = state;
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
bool HwHelperHw<Family>::is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const {
    return false;
}

template <typename Family>
AuxTranslationMode HwHelperHw<Family>::getAuxTranslationMode(const HardwareInfo &hwInfo) {
    auto mode = HwHelperHw<Family>::defaultAuxTranslationMode;
    if (DebugManager.flags.ForceAuxTranslationMode.get() != -1) {
        mode = static_cast<AuxTranslationMode>(DebugManager.flags.ForceAuxTranslationMode.get());
    }

    if (mode == AuxTranslationMode::Blit && !hwInfo.capabilityTable.blitterOperationsSupported) {
        DEBUG_BREAK_IF(true);
        mode = AuxTranslationMode::Builtin;
    }

    return mode;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData,
                                                                               const HardwareInfo &hwInfo, PipeControlArgs &args) {

    void *commandBuffer = commandStream.getSpace(MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo));

    MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(commandBuffer, postSyncMode, gpuAddress, immediateData, hwInfo, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
    void *&commandsBuffer,
    PostSyncMode postSyncMode,
    uint64_t gpuAddress,
    uint64_t immediateData,
    const HardwareInfo &hwInfo,
    PipeControlArgs &args) {

    MemorySynchronizationCommands<GfxFamily>::setBarrierWa(commandsBuffer, gpuAddress, hwInfo);

    setPostSyncExtraProperties(args, hwInfo);
    MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(commandsBuffer, postSyncMode, gpuAddress, immediateData, args);
    commandsBuffer = ptrOffset(commandsBuffer, getSizeForSingleBarrier());

    MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(commandsBuffer, gpuAddress, false, hwInfo);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(LinearStream &commandStream, PipeControlArgs &args) {
    addSingleBarrier(commandStream, PostSyncMode::NoWrite, 0, 0, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(void *commandsBuffer, PipeControlArgs &args) {
    setSingleBarrier(commandsBuffer, PostSyncMode::NoWrite, 0, 0, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(LinearStream &commandStream, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args) {
    auto barrier = commandStream.getSpace(MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier());

    setSingleBarrier(barrier, postSyncMode, gpuAddress, immediateData, args);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(void *commandsBuffer, PostSyncMode postSyncMode, uint64_t gpuAddress, uint64_t immediateData, PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL pipeControl = GfxFamily::cmdInitPipeControl;

    pipeControl.setCommandStreamerStallEnable(true);

    if (args.csStallOnly) {
        *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = pipeControl;
        return;
    }

    pipeControl.setConstantCacheInvalidationEnable(args.constantCacheInvalidationEnable);
    pipeControl.setInstructionCacheInvalidateEnable(args.instructionCacheInvalidateEnable);
    pipeControl.setPipeControlFlushEnable(args.pipeControlFlushEnable);
    pipeControl.setRenderTargetCacheFlushEnable(args.renderTargetCacheFlushEnable);
    pipeControl.setStateCacheInvalidationEnable(args.stateCacheInvalidationEnable);
    pipeControl.setTextureCacheInvalidationEnable(args.textureCacheInvalidationEnable);
    pipeControl.setVfCacheInvalidationEnable(args.vfCacheInvalidationEnable);
    pipeControl.setTlbInvalidate(args.tlbInvalidation);
    pipeControl.setNotifyEnable(args.notifyEnable);
    pipeControl.setDcFlushEnable(args.dcFlushEnable);
    pipeControl.setDepthCacheFlushEnable(args.depthCacheFlushEnable);
    pipeControl.setDepthStallEnable(args.depthStallEnable);
    pipeControl.setProtectedMemoryDisable(args.protectedMemoryDisable);

    if constexpr (GfxFamily::isUsingGenericMediaStateClear) {
        pipeControl.setGenericMediaStateClear(args.genericMediaStateClear);
    }
    setBarrierExtraProperties(&pipeControl, args);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pipeControl.setDcFlushEnable(true);
        pipeControl.setRenderTargetCacheFlushEnable(true);
        pipeControl.setInstructionCacheInvalidateEnable(true);
        pipeControl.setTextureCacheInvalidationEnable(true);
        pipeControl.setPipeControlFlushEnable(true);
        pipeControl.setVfCacheInvalidationEnable(true);
        pipeControl.setConstantCacheInvalidationEnable(true);
        pipeControl.setStateCacheInvalidationEnable(true);
        pipeControl.setTlbInvalidate(true);
    }
    if (DebugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setDcFlushEnable(false);
        pipeControl.setRenderTargetCacheFlushEnable(false);
        pipeControl.setInstructionCacheInvalidateEnable(false);
        pipeControl.setTextureCacheInvalidationEnable(false);
        pipeControl.setPipeControlFlushEnable(false);
        pipeControl.setVfCacheInvalidationEnable(false);
        pipeControl.setConstantCacheInvalidationEnable(false);
        pipeControl.setStateCacheInvalidationEnable(false);
    }

    if (postSyncMode != PostSyncMode::NoWrite) {
        pipeControl.setAddress(static_cast<uint32_t>(gpuAddress & 0x0000FFFFFFFFULL));
        pipeControl.setAddressHigh(static_cast<uint32_t>(gpuAddress >> 32));
    }

    if (postSyncMode == PostSyncMode::Timestamp) {
        pipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    } else if (postSyncMode == PostSyncMode::ImmediateData) {
        pipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
        pipeControl.setImmediateData(immediateData);
    }

    *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = pipeControl;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addBarrierWa(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    size_t requiredSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWa(hwInfo);
    void *commandBuffer = commandStream.getSpace(requiredSize);
    setBarrierWa(commandBuffer, gpuAddress, hwInfo);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setBarrierWa(void *&commandsBuffer, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    if (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(hwInfo)) {
        PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
        MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(&cmd);
        *reinterpret_cast<PIPE_CONTROL *>(commandsBuffer) = cmd;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(PIPE_CONTROL));

        MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(commandsBuffer, gpuAddress, false, hwInfo);
    }
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo) {
    size_t requiredSize = MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(hwInfo);
    void *commandBuffer = commandStream.getSpace(requiredSize);
    setAdditionalSynchronization(commandBuffer, gpuAddress, acquire, hwInfo);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronizationForDirectSubmission(LinearStream &commandStream, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo) {
    MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, gpuAddress, acquire, hwInfo);
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(bool isFlushPreferred, const HardwareInfo &hwInfo) {
    if (isFlushPreferred) {
        const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
        return hwInfoConfig.isDcFlushAllowed();
    }
    return false;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(const HardwareInfo &hwInfo) {
    size_t size = getSizeForSingleBarrier() +
                  getSizeForBarrierWa(hwInfo) +
                  getSizeForSingleAdditionalSynchronization(hwInfo);
    return size;
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWa(const HardwareInfo &hwInfo) {
    size_t size = 0;
    if (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(hwInfo)) {
        size = getSizeForSingleBarrier() +
               getSizeForSingleAdditionalSynchronization(hwInfo);
    }
    return size;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(const HardwareInfo &hwInfo) {
    return 0u;
}

template <typename GfxFamily>
inline size_t MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(const HardwareInfo &hwInfo) {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(hwInfo);
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
bool HwHelperHw<GfxFamily>::isLinearStoragePreferred(bool isSharedContext, bool isImage1d, bool forceLinearStorage) {
    if (DebugManager.flags.ForceLinearImages.get() || forceLinearStorage || isSharedContext || isImage1d) {
        return true;
    }
    return false;
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
uint32_t HwHelperHw<GfxFamily>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) {
    auto value = std::max(slmSize, 1024u);
    value = Math::nextPowerOfTwo(value);
    value = Math::getMinLsbSet(value);
    value = value - 9;
    DEBUG_BREAK_IF(value > 7);
    return value * !!slmSize;
}

template <typename GfxFamily>
uint8_t HwHelperHw<GfxFamily>::getBarriersCountFromHasBarriers(uint8_t hasBarriers) const {
    return hasBarriers;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) const {
    const auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto lowestHwRevIdWithBug = hwInfoConfig->getHwRevIdFromStepping(lowestSteppingWithBug, hwInfo);
    auto hwRevIdWithFix = hwInfoConfig->getHwRevIdFromStepping(steppingWithFix, hwInfo);
    if ((lowestHwRevIdWithBug == CommonConstants::invalidStepping) || (hwRevIdWithFix == CommonConstants::invalidStepping)) {
        return false;
    }
    return (lowestHwRevIdWithBug <= hwInfo.platform.usRevId && hwInfo.platform.usRevId < hwRevIdWithFix);
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isWaDisableRccRhwoOptimizationRequired() const {
    return false;
}

template <typename GfxFamily>
inline uint32_t HwHelperHw<GfxFamily>::getMinimalSIMDSize() {
    return 8u;
}

template <typename GfxFamily>
std::unique_ptr<TagAllocatorBase> HwHelperHw<GfxFamily>::createTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices, MemoryManager *memoryManager,
                                                                                        size_t initialTagCount, CommandStreamReceiverType csrType, DeviceBitfield deviceBitfield) const {
    bool doNotReleaseNodes = (csrType > CommandStreamReceiverType::CSR_HW) ||
                             DebugManager.flags.DisableTimestampPacketOptimizations.get();

    auto tagAlignment = getTimestampPacketAllocatorAlignment();

    if (DebugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (DebugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            using TimestampPackets32T = TimestampPackets<uint32_t>;
            return std::make_unique<TagAllocator<TimestampPackets32T>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPackets32T), doNotReleaseNodes, deviceBitfield);
        } else if (DebugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            using TimestampPackets64T = TimestampPackets<uint64_t>;
            return std::make_unique<TagAllocator<TimestampPackets64T>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPackets64T), doNotReleaseNodes, deviceBitfield);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    using TimestampPacketType = typename GfxFamily::TimestampPacketType;
    using TimestampPacketsT = TimestampPackets<TimestampPacketType>;

    return std::make_unique<TagAllocator<TimestampPacketsT>>(rootDeviceIndices, memoryManager, initialTagCount, tagAlignment, sizeof(TimestampPacketsT), doNotReleaseNodes, deviceBitfield);
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getTimestampPacketAllocatorAlignment() const {
    return MemoryConstants::cacheLineSize * 4;
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getSingleTimestampPacketSize() const {
    return HwHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw();
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw() {
    if (DebugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (DebugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return TimestampPackets<uint32_t>::getSinglePacketSize();
        } else if (DebugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return TimestampPackets<uint64_t>::getSinglePacketSize();
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return TimestampPackets<typename GfxFamily::TimestampPacketType>::getSinglePacketSize();
}

template <typename GfxFamily>
size_t MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush() {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::addFullCacheFlush(LinearStream &commandStream, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL *pipeControl = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;

    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    args.renderTargetCacheFlushEnable = true;
    args.instructionCacheInvalidateEnable = true;
    args.textureCacheInvalidationEnable = true;
    args.pipeControlFlushEnable = true;
    args.constantCacheInvalidationEnable = true;
    args.stateCacheInvalidationEnable = true;
    args.tlbInvalidation = true;
    MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(args);
    MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(&cmd, args);
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
int32_t HwHelperHw<GfxFamily>::getDefaultThreadArbitrationPolicy() const {
    return 0;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::useOnlyGlobalTimestamps() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::useSystemMemoryPlacementForISA(const HardwareInfo &hwInfo) const {
    return !getEnableLocalMemory(hwInfo);
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isBarrierlPriorToPipelineSelectWaRequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isRcsAvailable(const HardwareInfo &hwInfo) const {
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const {
    return true;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                        const HardwareInfo &hwInfo, bool isEngineInstanced) const {
    return maxWorkGroupCount;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isKmdMigrationSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isSipWANeeded(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isAdditionalFeatureFlagRequired(const FeatureTable *featureTable) const {
    return false;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getDefaultRevisionId(const HardwareInfo &hwInfo) const {
    return 0u;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getNumCacheRegions() const {
    return 0;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isSubDeviceEngineSupported(const HardwareInfo &hwInfo, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const {
    return true;
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getPreemptionAllocationAlignment() const {
    return 256 * MemoryConstants::kiloByte;
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const {}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const {
    gmm.resourceParams.Flags.Info.RenderCompressed = isCompressed;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isEngineTypeRemappingToHwSpecificRequired() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isSipKernelAsHexadecimalArrayPreferred() const {
    return false;
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::setSipKernelData(uint32_t *&sipKernelBinary, size_t &kernelBinarySize) const {
}

template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const {
    return 0x1800000;
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::adjustPreemptionSurfaceSize(size_t &csrSize) const {
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) {
    EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::disableL3CacheForDebug(const HardwareInfo &) const {
    return false;
}
template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isRevisionSpecificBinaryBuiltinRequired() const {
    return false;
}
template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::forceNonGpuCoherencyWA(bool requiresCoherency) const {
    return requiresCoherency;
}
template <typename GfxFamily>
size_t HwHelperHw<GfxFamily>::getBatchBufferEndSize() const {
    return sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);
}
template <typename GfxFamily>
const void *HwHelperHw<GfxFamily>::getBatchBufferEndReference() const {
    return reinterpret_cast<const void *>(&GfxFamily::cmdInitBatchBufferEnd);
}
template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isPlatformFlushTaskEnabled(const HardwareInfo &hwInfo) const {
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    return hwInfoConfig.isFlushTaskAllowed();
}

template <typename GfxFamily>
uint64_t HwHelperHw<GfxFamily>::getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const {
    UNRECOVERABLE_IF(true);

    return -1;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isPatIndexFallbackWaRequired() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isDevicePreemptionModeTrackedInScm() const {
    return false;
}
} // namespace NEO
