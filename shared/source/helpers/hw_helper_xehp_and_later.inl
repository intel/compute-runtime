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

#include "aubstream/engine_node.h"

namespace NEO {

template <typename GfxFamily>
void GfxCoreHelperHw<GfxFamily>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (DebugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    auto maxSubSlice = productHelper.computeMaxNeededSubSliceSpace(*hwInfo);
    // XeHP and later products return physical threads
    return maxSubSlice * hwInfo->gtSystemInfo.MaxEuPerSubSlice * (hwInfo->gtSystemInfo.ThreadCount / hwInfo->gtSystemInfo.EUCount);
}

template <typename GfxFamily>
inline uint32_t GfxCoreHelperHw<GfxFamily>::getGlobalTimeStampBits() const {
    return 32;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return hwInfo.featureTable.flags.ftrLocalMemory;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::hvAlign4Required() const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::timestampPacketWriteSupported() const {
    return true;
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
const EngineInstancesContainer GfxCoreHelperHw<GfxFamily>::getGpgpuEngineInstances(const HardwareInfo &hwInfo) const {
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
EngineGroupType GfxCoreHelperHw<GfxFamily>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
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
uint32_t GfxCoreHelperHw<GfxFamily>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
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
uint32_t GfxCoreHelperHw<GfxFamily>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    if (grfCount > GrfConfig::DefaultGrfNumber) {
        return hwInfo.gtSystemInfo.ThreadCount / 2u;
    }
    return hwInfo.gtSystemInfo.ThreadCount;
}

template <typename GfxFamily>
uint64_t GfxCoreHelperHw<GfxFamily>::getGpuTimeStampInNS(uint64_t timeStamp, double frequency) const {
    constexpr uint64_t mask = static_cast<uint64_t>(std::numeric_limits<typename GfxFamily::TimestampPacketType>::max());

    return static_cast<uint64_t>((timeStamp & mask) * frequency);
}

constexpr uint32_t planarYuvMaxHeight = 16128;

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getPlanarYuvMaxHeight() const {
    return planarYuvMaxHeight;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isAssignEngineRoundRobinSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
aub_stream::MMIOList GfxCoreHelperHw<GfxFamily>::getExtraMmioList(const HardwareInfo &hwInfo, const GmmHelper &gmmHelper) const {
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
bool MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(const HardwareInfo &hwInfo) {
    if (DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return hwInfo.featureTable.flags.ftrLocalMemory;
    }
    return false;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::preferSmallWorkgroupSizeForKernel(const size_t size, const HardwareInfo &hwInfo) const {
    if (ProductHelper::get(hwInfo.platform.eProductFamily)->getSteppingFromHwRevId(hwInfo) >= REVISION_B) {
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
bool GfxCoreHelperHw<GfxFamily>::isScratchSpaceSurfaceStateAccessible() const {
    return true;
}
template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMaxScratchSize() const {
    return 256 * KB;
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::platformSupportsImplicitScaling(const NEO::HardwareInfo &hwInfo) const {
    return ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(hwInfo);
}

template <typename GfxFamily>
inline bool GfxCoreHelperHw<GfxFamily>::preferInternalBcsEngine() const {
    auto preferInternalBcsEngine = true;
    if (DebugManager.flags.PreferInternalBcsEngine.get() != -1) {
        preferInternalBcsEngine = static_cast<bool>(DebugManager.flags.PreferInternalBcsEngine.get());
    }

    return preferInternalBcsEngine;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::getMinimalScratchSpaceSize() const {
    return 64U;
}

template <>
bool GfxCoreHelperHw<Family>::isChipsetUniqueUUIDSupported() const {
    return true;
}

} // namespace NEO
