/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_dg2_and_later.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/mtl/os_agnostic_product_helper_mtl.inl"

template class NEO::ProductHelperHw<gfxProduct>;
