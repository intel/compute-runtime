/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

constexpr auto gfxProduct = IGFX_ARROWLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/compiler_product_helper_xe_lpg.inl"

static NEO::EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperARL;
