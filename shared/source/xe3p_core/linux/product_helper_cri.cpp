/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_cri.h"

constexpr static auto gfxProduct = IGFX_CRI;

// keep files below
#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/os_interface/linux/product_helper_xe2_and_later_drm_slm.inl"
#include "shared/source/xe3p_core/cri/os_agnostic_product_helper_cri.inl"
#include "shared/source/xe3p_core/os_agnostic_product_helper_xe3p_core.inl"

namespace NEO {

#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isPageFaultSupported() const {
    if (debugManager.flags.EnableRecoverablePageFaults.get() != -1) {
        return !!debugManager.flags.EnableRecoverablePageFaults.get();
    }
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return true;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemPatIndex() const {
    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::useSharedSystemUsm() const {
    return false;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
