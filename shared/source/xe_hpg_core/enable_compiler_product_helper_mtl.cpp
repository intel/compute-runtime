/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_mtl_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"

#include "compiler_product_helper_mtl.inl"
#include "hw_info_mtl.h"

namespace NEO {

static EnableCompilerProductHelper<IGFX_METEORLAKE> enableCompilerProductHelperMTL;

} // namespace NEO
