/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "opencl/source/sampler/sampler.h"
using Family = NEO::Gen12LpFamily;
constexpr static auto gfxCore = IGFX_GEN12LP_CORE;

namespace NEO {
template <>
void SamplerHw<Family>::appendSamplerStateParams(void *samplerState) {
    reinterpret_cast<Family::SAMPLER_STATE *>(samplerState)->setLowQualityCubeCornerModeEnable(false);
}
} // namespace NEO

#include "opencl/source/sampler/sampler_tgllp_and_later.inl"
