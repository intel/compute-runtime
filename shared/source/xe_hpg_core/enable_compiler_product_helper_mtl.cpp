/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"

constexpr auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/xe_hpg_core/xe_lpg/compiler_product_helper_xe_lpg.inl"

static NEO::EnableCompilerProductHelper<gfxProduct> enableCompilerProductHelperMTL;
