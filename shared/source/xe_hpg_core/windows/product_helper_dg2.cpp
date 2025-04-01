/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/helpers/windows/product_helper_dg2_and_later_discrete.inl"
#include "shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl"
#include "shared/source/xe_hpg_core/os_agnostic_product_helper_xe_hpg_core.inl"

#include "windows_product_helper_dg2_extra.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return false;
}

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableBlitterOperationsSupport(hwInfo);

    hwInfo->workaroundTable.flags.wa_15010089951 = true;

    return 0;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
