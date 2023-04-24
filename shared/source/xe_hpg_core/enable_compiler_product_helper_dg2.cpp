/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_bdw_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_before_xe_hpc.inl"
#include "shared/source/helpers/compiler_product_helper_enable_split_matrix_multiply_accumulate.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"

#include "compiler_product_helper_dg2.inl"
namespace NEO {

static EnableCompilerProductHelper<IGFX_DG2> enableCompilerProductHelperDG2;

template <>
uint32_t CompilerProductHelperHw<IGFX_DG2>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    HardwareIpVersion dg2Config = ipVersion;
    dg2Config.revision = revisionID;
    return dg2Config.value;
}

} // namespace NEO
