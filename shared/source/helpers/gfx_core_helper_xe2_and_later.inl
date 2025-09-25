/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {

template <>
void GfxCoreHelperHw<Family>::applyAdditionalCompressionSettings(Gmm &gmm, bool isNotCompressed) const {
    gmm.resourceParams.Flags.Info.NotCompressed = isNotCompressed;
    if (!isNotCompressed) {
        gmm.resourceParams.Flags.Info.Cacheable = 0;
        gmm.applyExtraAuxInitFlag();
    }

    if (debugManager.flags.PrintGmmCompressionParams.get()) {
        printf("\n\tFlags.Info.NotCompressed: %u", gmm.resourceParams.Flags.Info.NotCompressed);
    }
}

template <>
bool GfxCoreHelperHw<Family>::isCompressionAppliedForImportedResource(Gmm &gmm) const {
    auto gmmFlags = gmm.gmmResourceInfo->getResourceFlags();
    auto isResourceDenyCompressionEnabled = gmm.gmmResourceInfo->isResourceDenyCompressionEnabled();
    if ((gmmFlags->Info.NotCompressed == 0) && (!isResourceDenyCompressionEnabled)) {
        return true;
    }

    return false;
}

template <typename GfxFamily>
const EngineInstancesContainer GfxCoreHelperHw<GfxFamily>::getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto defaultEngine = getChosenEngineType(hwInfo);
    EngineInstancesContainer engines;

    uint32_t numCcs = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    if (hwInfo.featureTable.flags.ftrCCSNode) {
        for (uint32_t i = 0; i < numCcs; i++) {
            auto engineType = static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS);

            engines.push_back({engineType, EngineUsage::regular});
        }
    }

    if ((debugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) ||
        hwInfo.featureTable.flags.ftrRcsNode) {
        engines.push_back({aub_stream::ENGINE_CCCS, EngineUsage::regular});
    }

    engines.push_back({defaultEngine, EngineUsage::lowPriority});
    engines.push_back({defaultEngine, EngineUsage::internal});

    if (hwInfo.capabilityTable.blitterOperationsSupported) {

        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular});  // Main copy engine
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::internal}); // Internal usage
        }

        uint32_t internalIndex = getInternalCopyEngineIndex(hwInfo);
        for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
            if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                auto engineType = static_cast<aub_stream::EngineType>((i - 1) + aub_stream::ENGINE_BCS1); // Link copy engine
                engines.push_back({engineType, EngineUsage::regular});
                if (i == internalIndex) {
                    engines.push_back({engineType, EngineUsage::internal});
                }
            }
        }
    }

    return engines;
};

template <>
void GfxCoreHelperHw<Family>::applyRenderCompressionFlag(Gmm &gmm, uint32_t isCompressed) const {}

template <>
bool GfxCoreHelperHw<Family>::isTimestampShiftRequired() const {
    return false;
}

template <>
void MemorySynchronizationCommands<Family>::encodeAdditionalTimestampOffsets(LinearStream &commandStream, uint64_t contextAddress, uint64_t globalAddress, bool isBcs) {
    EncodeStoreMMIO<Family>::encode(commandStream, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, contextAddress + sizeof(uint32_t), false, nullptr, isBcs);
    EncodeStoreMMIO<Family>::encode(commandStream, RegisterOffsets::globalTimestampUn, globalAddress + sizeof(uint32_t), false, nullptr, isBcs);
}

template <>
bool GfxCoreHelperHw<Family>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

template <>
bool GfxCoreHelperHw<Family>::isCacheFlushPriorImageReadRequired() const {
    return true;
}

template <>
bool GfxCoreHelperHw<Family>::isExtendedUsmPoolSizeEnabled() const {
    return true;
}

template <>
bool GfxCoreHelperHw<Family>::crossEngineCacheFlushRequired() const {
    return false;
};

} // namespace NEO
