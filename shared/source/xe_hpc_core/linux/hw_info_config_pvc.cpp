/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"

namespace NEO {
constexpr static auto gfxProduct = IGFX_PVC;

#include "shared/source/xe_hpc_core/os_agnostic_hw_info_config_pvc.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableCompression(hwInfo);

    hwInfo->featureTable.embargoFlags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    hwInfo->featureTable.flags.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 20;

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
