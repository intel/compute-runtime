/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/mtl/os_agnostic_product_helper_mtl.inl"
#include "shared/source/xe_hpg_core/os_agnostic_product_helper_xe_hpg_core.inl"

template class NEO::ProductHelperHw<gfxProduct>;
