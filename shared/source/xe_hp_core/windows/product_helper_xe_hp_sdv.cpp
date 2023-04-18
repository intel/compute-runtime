/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/xe_hp_core/hw_cmds_xe_hp_sdv.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

#include "shared/source/xe_hp_core/os_agnostic_product_helper_xe_hp_core.inl"

namespace NEO {
template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    disableRcsExposure(hwInfo);

    enableBlitterOperationsSupport(hwInfo);
    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) const {
    if (GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo, *this) && (getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
        return false;
    }

    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
