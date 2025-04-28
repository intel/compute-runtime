/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename Family>
bool GfxCoreHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    if ((debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() == 1) ||
        (debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() == 1) ||
        (debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.get() == 1) ||
        (debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() == 1)) {
        return true;
    }
    if ((debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() == 0) &&
        (debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get() == 0) &&
        (debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.get() == 0) &&
        (debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() == 0)) {
        return false;
    }
    return !hwInfo.capabilityTable.isIntegratedDevice;
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
        if (engineGroupType == EngineGroupType::renderCompute) {
            return false;
        }

        bool isExclusiveContextUsed = (engineGroupType == EngineGroupType::cooperativeCompute);
        return !isRcsAvailable(hwInfo) || isExclusiveContextUsed;
    }

    return true;
}

template <typename Family>
uint32_t GfxCoreHelperHw<Family>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                          const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if ((debugManager.flags.ForceTheoreticalMaxWorkGroupCount.get()) ||
        (debugManager.flags.OverrideMaxWorkGroupCount.get() != -1)) {
        return maxWorkGroupCount;
    }
    if (!isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment)) {
        return 1u;
    }
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    bool requiresLimitation = productHelper.isCooperativeEngineSupported(hwInfo) &&
                              (engineGroupType != EngineGroupType::cooperativeCompute);

    auto ccsCount = hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
    auto numberOfpartsInTileForConcurrentKernels = productHelper.getNumberOfPartsInTileForConcurrentKernel(ccsCount);
    if (requiresLimitation) {
        UNRECOVERABLE_IF(ccsCount == 0);
        numberOfpartsInTileForConcurrentKernels = std::max(numberOfpartsInTileForConcurrentKernels, ccsCount);
    }
    return std::max(maxWorkGroupCount / numberOfpartsInTileForConcurrentKernels, 1u);
}

template <typename Family>
bool GfxCoreHelperHw<Family>::isEngineTypeRemappingToHwSpecificRequired() const {
    return true;
}

template <typename Family>
size_t GfxCoreHelperHw<Family>::getPaddingForISAAllocation() const {
    if (debugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 0xE00 + (MemoryConstants::pageSize * debugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 0xE00;
}

} // namespace NEO
