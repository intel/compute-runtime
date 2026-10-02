/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_GEN12LP_CORE;

template struct SamplerCoreFamily<gfxCoreFamily>;
static SamplerPopulateFactory<gfxCoreFamily, SamplerCoreFamily<gfxCoreFamily>> populateGen12Lp;

} // namespace L0
