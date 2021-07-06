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
#include "shared/source/xe_hp_core/os_agnostic_hw_info_config_xe_hp_core.inl"

namespace NEO {
template <>
int HwInfoConfigHw<IGFX_XE_HP_SDV>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);

    if (hwHelper.allowRenderCompression(*hwInfo)) {
        enableRenderCompression(hwInfo);
    }

    hwInfo->featureTable.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) {
        hwInfo->featureTable.ftrRcsNode = true;
    }

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
bool HwInfoConfigHw<IGFX_XE_HP_SDV>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) && (hwHelper.getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
        return false;
    }

    return true;
}

template <>
uint64_t HwInfoConfigHw<IGFX_XE_HP_SDV>::getHostMemCapabilitiesValue() {
    return UNIFIED_SHARED_MEMORY_ACCESS;
}

template <>
void HwInfoConfigHw<IGFX_XE_HP_SDV>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}

template <>
uint32_t HwInfoConfigHw<IGFX_XE_HP_SDV>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    return 2800u;
}

template class HwInfoConfigHw<IGFX_XE_HP_SDV>;
} // namespace NEO
