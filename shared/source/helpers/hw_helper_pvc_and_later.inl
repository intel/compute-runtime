/*
 * Copyright (C) 2021 Intel Corporation
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
    if (isCooperativeEngineSupported(hwInfo)) {
        if (engineGroupType == EngineGroupType::RenderCompute) {
            return false;
        }

        bool isExclusiveContextUsed = (engineGroupType == EngineGroupType::CooperativeCompute);
        return !isRcsAvailable(hwInfo) || isExclusiveContextUsed;
    }

    return true;
}

template <>
bool HwHelperHw<Family>::isTimestampWaitSupported() const {
    return true;
}

template <>
uint32_t HwHelperHw<Family>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                     const HardwareInfo &hwInfo, bool isEngineInstanced) const {
    if (!isCooperativeDispatchSupported(engineGroupType, hwInfo)) {
        return 1u;
    }

    bool requiresLimitation = this->isCooperativeEngineSupported(hwInfo) &&
                              (engineGroupType != EngineGroupType::CooperativeCompute) &&
                              (!isEngineInstanced) &&
                              (DebugManager.flags.OverrideMaxWorkGroupCount.get() == -1);
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

} // namespace NEO
