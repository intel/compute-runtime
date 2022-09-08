/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice *
           pHwInfo->gtSystemInfo.ThreadCount / pHwInfo->gtSystemInfo.EUCount;
}

template <typename GfxFamily>
inline uint32_t HwHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 36;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::hvAlign4Required() const {
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isTimestampWaitSupportedForQueues() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isTimestampWaitSupportedForEvents(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isUpdateTaskCountFromWaitSupported() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isAssignEngineRoundRobinSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
const EngineInstancesContainer HwHelperHw<GfxFamily>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    return {
        {aub_stream::ENGINE_RCS, EngineUsage::Regular},
        {aub_stream::ENGINE_RCS, EngineUsage::LowPriority},
        {aub_stream::ENGINE_RCS, EngineUsage::Internal},
    };
}

template <typename GfxFamily>
EngineGroupType HwHelperHw<GfxFamily>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    switch (engineType) {
    case aub_stream::ENGINE_RCS:
        return EngineGroupType::RenderCompute;
    default:
        UNRECOVERABLE_IF(true);
    }
}

template <typename GfxFamily>
std::string HwHelperHw<GfxFamily>::getExtensions(const HardwareInfo &hwInfo) const {
    return "";
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    }
    return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) {
    return hwInfo.gtSystemInfo.ThreadCount;
}

template <typename GfxFamily>
uint64_t HwHelperHw<GfxFamily>::getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const {
    return static_cast<uint64_t>(timeStamp * frequency);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const {
    return false;
}

constexpr uint32_t planarYuvMaxHeight = 16352;

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
aub_stream::MMIOList HwHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    return {};
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(PipeControlArgs &args) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(const HardwareInfo &hwInfo) { return false; }

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(void *barrierCmd) {
    reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(barrierCmd)->setCommandStreamerStallEnable(true);
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::unTypedDataPortCacheFlushRequired() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::preferInternalBcsEngine() const {
    return false;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMinimalScratchSpaceSize() const {
    return 1024U;
}

} // namespace NEO
