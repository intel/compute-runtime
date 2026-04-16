/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_nvlp.h"

constexpr static auto gfxProduct = IGFX_NVL;

#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/os_interface/linux/product_helper_xe2_and_later_drm_slm.inl"
#include "shared/source/os_interface/linux/product_helper_xe3p_and_later.inl"
#include "shared/source/xe3p_core/nvlp/os_agnostic_product_helper_nvlp.inl"
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
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemPatIndex() const {
    return 1;
}

template <>
bool ProductHelperHw<gfxProduct>::useSharedSystemUsm() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::areSecondaryContextsSupported() const {
    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
