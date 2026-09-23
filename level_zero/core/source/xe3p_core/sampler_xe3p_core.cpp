/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE3P_CORE;

template struct SamplerCoreFamily<gfxCoreFamily>;
static SamplerPopulateFactory<gfxCoreFamily, SamplerCoreFamily<gfxCoreFamily>> populateXe3pCore;

} // namespace L0
