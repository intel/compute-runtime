/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/compiler_hw_info_config_base.inl"
#include "shared/source/helpers/compiler_hw_info_config_bdw_and_later.inl"
#include "shared/source/helpers/compiler_hw_info_config_before_xe_hpc.inl"

#include "hw_info_bdw.h"

namespace NEO {

#include "shared/source/gen8/compiler_hw_info_config_bdw.inl"
static EnableCompilerProductHelper<IGFX_BROADWELL> enableCompilerProductHelperBDW;

} // namespace NEO
