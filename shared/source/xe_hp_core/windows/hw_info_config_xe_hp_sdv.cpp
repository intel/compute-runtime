/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/xe_hp_core/hw_cmds_xe_hp_sdv.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

#include "shared/source/xe_hp_core/os_agnostic_hw_info_config_xe_hp_core.inl"

namespace NEO {
template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    disableRcsExposure(hwInfo);

    enableBlitterOperationsSupport(hwInfo);
    return 0;
}

template <>
bool HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    GfxCoreHelper &gfxCoreHelper = GfxCoreHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (gfxCoreHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) && (getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
        return false;
    }

    return true;
}

template <>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
