/*
 * Copyright (C) 2019-2020 Intel Corporation
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
void HwHelperHw<GfxFamily>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::heapInLocalMem(const HardwareInfo &hwInfo) const {
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

template <typename Family>
bool HwHelperHw<Family>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
const HwHelper::EngineInstancesContainer HwHelperHw<GfxFamily>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    return {
        {aub_stream::ENGINE_RCS, EngineUsage::Regular},
        {aub_stream::ENGINE_RCS, EngineUsage::LowPriority},
        {aub_stream::ENGINE_RCS, EngineUsage::Internal},
    };
}

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::addEngineToEngineGroup(std::vector<std::vector<EngineControl>> &engineGroups,
                                                   EngineControl &engine, const HardwareInfo &hwInfo) const {
    engineGroups[static_cast<uint32_t>(EngineGroupType::RenderCompute)].push_back(engine);
}

template <typename GfxFamily>
std::string HwHelperHw<GfxFamily>::getExtensions() const {
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
uint32_t HwHelperHw<GfxFamily>::calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                              uint32_t threadsPerEu) {
    return threadsPerEu * euCount;
}

template <typename GfxFamily>
uint64_t HwHelperHw<GfxFamily>::getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const {
    return static_cast<uint64_t>(timeStamp * frequency);
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(PipeControlArgs &args) {
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPipeControlExtraProperties(typename GfxFamily::PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) { return false; }

} // namespace NEO
