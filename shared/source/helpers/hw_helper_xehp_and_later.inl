/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "engine_node.h"

namespace NEO {

template <typename GfxFamily>
void HwHelperHw<GfxFamily>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    if (DebugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }
    auto maxSubSlice = HwInfoConfig::get(pHwInfo->platform.eProductFamily)->computeMaxNeededSubSliceSpace(*pHwInfo);
    // XeHP and later products return physical threads
    return maxSubSlice * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * (pHwInfo->gtSystemInfo.ThreadCount / pHwInfo->gtSystemInfo.EUCount);
}

template <typename GfxFamily>
inline uint32_t HwHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 32;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrLocalMemory;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::hvAlign4Required() const {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isTimestampWaitSupported() const {
    return false;
}

template <typename GfxFamily>
const EngineInstancesContainer HwHelperHw<GfxFamily>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);

    EngineInstancesContainer engines;

    if (hwInfo.featureTable.flags.ftrCCSNode) {
        for (uint32_t i = 0; i < hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled; i++) {
            engines.push_back({static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS), EngineUsage::Regular});
        }
    }

    if ((DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) ||
        hwInfo.featureTable.flags.ftrRcsNode) {
        engines.push_back({aub_stream::ENGINE_RCS, EngineUsage::Regular});
    }

    engines.push_back({defaultEngine, EngineUsage::LowPriority});
    engines.push_back({defaultEngine, EngineUsage::Internal});

    if (hwInfo.capabilityTable.blitterOperationsSupported && hwInfo.featureTable.ftrBcsInfo.test(0)) {
        engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::Regular});
        engines.push_back({aub_stream::ENGINE_BCS, EngineUsage::Internal}); // internal usage
    }

    return engines;
};

template <typename GfxFamily>
EngineGroupType HwHelperHw<GfxFamily>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    if (engineType == aub_stream::ENGINE_RCS) {
        return EngineGroupType::RenderCompute;
    }
    if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
        return EngineGroupType::Compute;
    }
    if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::Copy;
    }
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        if (DebugManager.flags.ForceL1Caching.get() == 0) {
            if (l1enabled) {
                return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;
            }
            return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
        } else {
            return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;
        }
    }

    return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                              uint32_t threadsPerEu) {
    if (grfCount > GrfConfig::DefaultGrfNumber) {
        return threadsPerEu / 2u * euCount;
    }
    return threadsPerEu * euCount;
}

template <typename GfxFamily>
uint64_t HwHelperHw<GfxFamily>::getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const {
    return static_cast<uint64_t>((timeStamp & 0xffff'ffff) * frequency);
}

constexpr uint32_t planarYuvMaxHeight = 16128;

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isAssignEngineRoundRobinSupported() const {
    return true;
}

template <typename GfxFamily>
aub_stream::MMIOList HwHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
    aub_stream::MMIOList mmioList;

    if (DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        auto format = static_cast<uint32_t>(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());

        UNRECOVERABLE_IF(format > 0x1F);

        uint32_t value = 1;     // [0] enable
        value |= (format << 3); // [3:7] compression_format

        mmioList.push_back({0x519C, value});
        mmioList.push_back({0xB0F0, value});
        mmioList.push_back({0xE4C0, value});
    }

    return mmioList;
}

template <typename GfxFamily>
bool MemorySynchronizationCommands<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return hwInfo.featureTable.flags.ftrLocalMemory;
    }
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const {
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->getSteppingFromHwRevId(hwInfo) >= REVISION_B) {
        return false;
    }

    auto defaultThreshold = 2048u;
    if (DebugManager.flags.OverrideKernelSizeLimitForSmallDispatch.get() != -1) {
        defaultThreshold = DebugManager.flags.OverrideKernelSizeLimitForSmallDispatch.get();
    }

    if (size >= defaultThreshold) {
        return false;
    }
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return true;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const {
    return ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(hwInfo);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::isLinuxCompletionFenceSupported() const {
    return false;
}

} // namespace NEO
