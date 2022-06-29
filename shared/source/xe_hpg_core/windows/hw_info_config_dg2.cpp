/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/xe_hpg_core/dg2/os_agnostic_hw_info_config_dg2.inl"
#include "shared/source/xe_hpg_core/os_agnostic_hw_info_config_xe_hpg_core.inl"

#include "os_agnostic_hw_info_config_dg2_extra.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }
    DG2::adjustHardwareInfo(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
