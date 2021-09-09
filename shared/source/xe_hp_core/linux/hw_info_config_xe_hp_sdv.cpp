/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

#include "shared/source/xe_hp_core/os_agnostic_hw_info_config_xe_hp_core.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (allowRenderCompression(*hwInfo)) {
        enableRenderCompression(hwInfo);
    }

    hwInfo->featureTable.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) {
        hwInfo->featureTable.ftrRcsNode = true;
    }

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 20;

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
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesValue() {
    return UNIFIED_SHARED_MEMORY_ACCESS;
}

template <>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    return 2800u;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
