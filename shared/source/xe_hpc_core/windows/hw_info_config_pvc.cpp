/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"

#include "hw_cmds.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_PVC;

#include "shared/source/xe_hpc_core/os_agnostic_hw_info_config_pvc.inl"

#include "os_agnostic_hw_info_config_pvc_extra.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    hwInfo->featureTable.flags.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;

} // namespace NEO
