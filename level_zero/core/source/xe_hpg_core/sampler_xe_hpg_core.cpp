/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE_HPG_CORE;

template struct SamplerCoreFamily<gfxCoreFamily>;
static SamplerPopulateFactory<gfxCoreFamily, SamplerCoreFamily<gfxCoreFamily>> populateXeHpgCore;

} // namespace L0
