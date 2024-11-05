/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_skl_to_pvc.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_tgllp_to_dg2.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe_hpg_and_xe_hpc.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe_hpg_to_xe2_hpg.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace L0 {

using Family = NEO::XeHpgCoreFamily;
static auto gfxCore = IGFX_XE_HPG_CORE;

template <>
ze_mutable_command_exp_flags_t L0GfxCoreHelperHw<Family>::getPlatformCmdListUpdateCapabilities() const {
    return 0u;
}

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
