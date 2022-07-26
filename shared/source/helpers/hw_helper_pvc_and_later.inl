/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <>
bool HwHelperHw<Family>::isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.supportsImages;
}

template <>
bool HwHelperHw<Family>::isRcsAvailable(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);
    return (defaultEngine == aub_stream::EngineType::ENGINE_RCS) ||
           (defaultEngine == aub_stream::EngineType::ENGINE_CCCS) || hwInfo.featureTable.flags.ftrRcsNode;
}

template <>
bool HwHelperHw<Family>::isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const {
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isCooperativeEngineSupported(hwInfo)) {
        if (engineGroupType == EngineGroupType::RenderCompute) {
            return false;
        }

        bool isExclusiveContextUsed = (engineGroupType == EngineGroupType::CooperativeCompute);
        return !isRcsAvailable(hwInfo) || isExclusiveContextUsed;
    }

    return true;
}

template <>
uint32_t HwHelperHw<Family>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                     const HardwareInfo &hwInfo, bool isEngineInstanced) const {
    if ((DebugManager.flags.ForceTheoreticalMaxWorkGroupCount.get()) ||
        (DebugManager.flags.OverrideMaxWorkGroupCount.get() != -1)) {
        return maxWorkGroupCount;
    }
    if (!isCooperativeDispatchSupported(engineGroupType, hwInfo)) {
        return 1u;
    }
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    bool requiresLimitation = hwInfoConfig.isCooperativeEngineSupported(hwInfo) &&
                              (engineGroupType != EngineGroupType::CooperativeCompute) &&
                              (!isEngineInstanced);
    if (requiresLimitation) {
        auto ccsCount = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
        UNRECOVERABLE_IF(ccsCount == 0);
        return maxWorkGroupCount / ccsCount;
    }
    return maxWorkGroupCount;
}

template <>
bool HwHelperHw<Family>::isEngineTypeRemappingToHwSpecificRequired() const {
    return true;
}

template <>
size_t HwHelperHw<Family>::getPaddingForISAAllocation() const {
    if (DebugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 0xE00 + (MemoryConstants::pageSize * DebugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 0xE00;
}

template <>
uint32_t HwHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) {
    auto maxThreadsPerEuCount = 1024u / grfCount;
    return maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount;
}

} // namespace NEO
