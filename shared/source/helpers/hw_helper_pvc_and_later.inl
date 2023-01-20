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
bool GfxCoreHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo) const {
    if ((DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.get() == 0)) {
        return false;
    }
    return true;
}

template <>
bool GfxCoreHelperHw<Family>::isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.supportsImages;
}

template <>
bool GfxCoreHelperHw<Family>::isRcsAvailable(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);
    return (defaultEngine == aub_stream::EngineType::ENGINE_RCS) ||
           (defaultEngine == aub_stream::EngineType::ENGINE_CCCS) || hwInfo.featureTable.flags.ftrRcsNode;
}

template <>
bool GfxCoreHelperHw<Family>::isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const HardwareInfo &hwInfo) const {
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    if (productHelper.isCooperativeEngineSupported(hwInfo)) {
        if (engineGroupType == EngineGroupType::RenderCompute) {
            return false;
        }

        bool isExclusiveContextUsed = (engineGroupType == EngineGroupType::CooperativeCompute);
        return !isRcsAvailable(hwInfo) || isExclusiveContextUsed;
    }

    return true;
}

template <>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                          const HardwareInfo &hwInfo, bool isEngineInstanced) const {
    if ((DebugManager.flags.ForceTheoreticalMaxWorkGroupCount.get()) ||
        (DebugManager.flags.OverrideMaxWorkGroupCount.get() != -1)) {
        return maxWorkGroupCount;
    }
    if (!isCooperativeDispatchSupported(engineGroupType, hwInfo)) {
        return 1u;
    }
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
    bool requiresLimitation = productHelper.isCooperativeEngineSupported(hwInfo) &&
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
bool GfxCoreHelperHw<Family>::isEngineTypeRemappingToHwSpecificRequired() const {
    return true;
}

template <>
size_t GfxCoreHelperHw<Family>::getPaddingForISAAllocation() const {
    if (DebugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 0xE00 + (MemoryConstants::pageSize * DebugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 0xE00;
}

template <>
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    auto maxThreadsPerEuCount = 1024u / grfCount;
    return maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount;
}

} // namespace NEO
