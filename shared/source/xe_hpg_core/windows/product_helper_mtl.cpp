/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/os_agnostic_product_helper_xe_lpg.inl"
#include "shared/source/xe_hpg_core/xe_lpg/windows/product_helper_xe_lpg_windows.inl"

template class NEO::ProductHelperHw<gfxProduct>;
