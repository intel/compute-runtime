/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE_HPG_CORE;

template struct KernelHw<gfxCoreFamily>;
static KernelPopulateFactory<gfxCoreFamily, KernelHw<gfxCoreFamily>> populateXeHpgCore;

} // namespace L0
