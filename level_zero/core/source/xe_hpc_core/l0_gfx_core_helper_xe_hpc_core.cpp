/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace L0 {

using Family = NEO::XeHpcCoreFamily;
static auto gfxCore = IGFX_XE_HPC_CORE;

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template <>
bool L0GfxCoreHelperHw<Family>::multiTileCapablePlatform() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsImmediateComputeFlushTask() const {
    return true;
}

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
