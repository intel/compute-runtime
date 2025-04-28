/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {
template <typename Family>
bool GfxCoreHelperHw<Family>::isFenceAllocationRequired(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isCpuImageTransferPreferred(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isRcsAvailable(const HardwareInfo &hwInfo) const {
    return true;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isCooperativeDispatchSupported(const EngineGroupType engineGroupType, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return true;
}

template <typename GfxFamily>
uint32_t GfxCoreHelperHw<GfxFamily>::adjustMaxWorkGroupCount(uint32_t maxWorkGroupCount, const EngineGroupType engineGroupType,
                                                             const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if ((debugManager.flags.ForceTheoreticalMaxWorkGroupCount.get()) ||
        (debugManager.flags.OverrideMaxWorkGroupCount.get() != -1)) {
        return maxWorkGroupCount;
    }
    return 1u;
}

template <typename GfxFamily>
bool GfxCoreHelperHw<GfxFamily>::isEngineTypeRemappingToHwSpecificRequired() const {
    return false;
}

template <typename Family>
size_t GfxCoreHelperHw<Family>::getPaddingForISAAllocation() const {
    if (debugManager.flags.ForceExtendedKernelIsaSize.get() >= 1) {
        return 512 + (MemoryConstants::pageSize * debugManager.flags.ForceExtendedKernelIsaSize.get());
    }
    return 512;
}
} // namespace NEO
