/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE2_HPG_CORE;

template struct KernelHw<gfxCoreFamily>;
static KernelPopulateFactory<gfxCoreFamily, KernelHw<gfxCoreFamily>> populateXe2HpgCore;

} // namespace L0
