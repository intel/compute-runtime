/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

constexpr static auto gfxProduct = IGFX_ARROWLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/linux/product_helper_xe_lpg_linux.inl"
#include "shared/source/xe_hpg_core/xe_lpg/os_agnostic_product_helper_xe_lpg.inl"

template class NEO::ProductHelperHw<gfxProduct>;
