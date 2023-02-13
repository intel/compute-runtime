/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename Family>
bool GfxCoreHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo) const {
    if ((DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() == 0) &&
        (DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.get() == 0)) {
        return false;
    }
    return true;
}

template <typename Family>
bool GfxCoreHelperHw<Family>::isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const {
    return !hwInfo.capabilityTable.supportsImages;
}

template <typename Family>
bool GfxCoreHelperHw<Family>::isRcsAvailable(const HardwareInfo &hwInfo) const {
    auto defaultEngine = getChosenEngineType(hwInfo);
    return (defaultEngine == aub_stream::EngineType::ENGINE_RCS) ||
           (defaultEngine == aub_stream::EngineType::ENGINE_CCCS) || hwInfo.featureTable.flags.ftrRcsNode;
}

template <typename Family>
bool GfxCoreHelperHw<Family>::isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (productHelper.isCooperativeEngineSupported(hwInfo)) {
        if (engineGroupType == EngineGroupType::RenderCompute) {
            return false;
        }

        bool isExclusiveContextUsed = (engineGroupType == EngineGroupType::CooperativeCompute);
        return !isRcsAvailable(hwInfo) || isExclusiveContextUsed;
    }

    return true;
}

template <typename Family>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                          const RootDeviceEnvironment &rootDeviceEnvironment, bool isEngineInstanced) const {
    if ((DebugManager.flags.ForceTheoreticalMaxWorkGroupCount.get()) ||
        (DebugManager.flags.OverrideMaxWorkGroupCount.get() != -1)) {
        return maxWorkGroupCount;
    }
    if (!isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment)) {
        return 1u;
    }
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
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

template <typename Family>
bool GfxCoreHelperHw<Family>::isEngineTypeRemappingToHwSpecificRequired() const {
    return true;
}

template <typename Family>
size_t GfxCoreHelperHw<Family>::getPaddingForISAAllocation() const {
    if (DebugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 0xE00 + (MemoryConstants::pageSize * DebugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 0xE00;
}

template <typename Family>
uint32_t GfxCoreHelperHw<Family>::calculateAvailableThreadCount(const HardwareInfo &hwInfo, uint32_t grfCount) const {
    auto maxThreadsPerEuCount = 1024u / grfCount;
    return maxThreadsPerEuCount * hwInfo.gtSystemInfo.EUCount;
}

} // namespace NEO
