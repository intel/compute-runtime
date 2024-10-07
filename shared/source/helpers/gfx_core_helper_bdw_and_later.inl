/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) {
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 36;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::hvAlign4Required() const {
    return true;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isTimestampWaitSupportedForQueues() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isUpdateTaskCountFromWaitSupported() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getAmountOfAllocationsToFill() const {
    if (debugManager.flags.SetAmountOfReusableAllocations.get() != -1) {
        return debugManager.flags.SetAmountOfReusableAllocations.get();
    }
    return 0u;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::makeResidentBeforeLockNeeded(bool precondition) const {
    return precondition;
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::calculateMaxWorkGroupSize(const KernelDescriptor &kernelDescriptor, uint32_t defaultMaxGroupSize) const {
    return std::min(defaultMaxGroupSize, CommonConstants::maxWorkgroupSize);
}

constexpr uint32_t planarYuvMaxHeight = 16352;

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
aub_stream::MMIOList GfxCoreHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    return {};
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(void *barrierCmd) {
    reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(barrierCmd)->setCommandStreamerStallEnable(true);
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::unTypedDataPortCacheFlushRequired() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMaxScratchSize(const NEO::ProductHelper &productHelper) const {
    return 2 * MemoryConstants::megaByte;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return false;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::preferInternalBcsEngine() const {
    return false;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalScratchSpaceSize() const {
    return 1024U;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getKernelPrivateMemSize(const KernelDescriptor &kernelDescriptor) const {
    return kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize;
}
} // namespace NEO
