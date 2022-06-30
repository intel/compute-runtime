/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

#include "shared/source/xe_hp_core/os_agnostic_hw_info_config_xe_hp_core.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    hwInfo->featureTable.flags.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }

    enableBlitterOperationsSupport(hwInfo);
    return 0;
}

template <>
bool HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) && (getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
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
