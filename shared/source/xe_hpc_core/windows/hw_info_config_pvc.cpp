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
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_PVC;

#include "shared/source/xe_hpc_core/os_agnostic_hw_info_config_xe_hpc_core.inl"
#include "shared/source/xe_hpc_core/pvc/os_agnostic_hw_info_config_pvc.inl"

namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    enableCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);
    disableRcsExposure(hwInfo);

    return 0;
}

template class ProductHelperHw<gfxProduct>;

} // namespace NEO
