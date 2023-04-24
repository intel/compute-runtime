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
#include "shared/source/helpers/compiler_product_helper_enable_split_matrix_multiply_accumulate.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hpc_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds.h"

#include "compiler_product_helper_pvc.inl"

namespace NEO {

static EnableCompilerProductHelper<IGFX_PVC> enableCompilerProductHelperPVC;

template <>
uint32_t CompilerProductHelperHw<IGFX_PVC>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    HardwareIpVersion pvcConfig = ipVersion;
    pvcConfig.revision = revisionID & PVC::pvcSteppingBits;
    return pvcConfig.value;
}

} // namespace NEO
