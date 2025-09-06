/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"

constexpr static auto gfxProduct = IGFX_LUNARLAKE;

#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/xe2_hpg_core/lnl/os_agnostic_product_helper_lnl.inl"
#include "shared/source/xe2_hpg_core/os_agnostic_product_helper_xe2_hpg_core.inl"

namespace NEO {

#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::deferMOCSToPatIndex(bool isWddmOnLinux) const {
    return !isWddmOnLinux;
}

template <>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    return false;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemPatIndex() const {
    return 1;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
