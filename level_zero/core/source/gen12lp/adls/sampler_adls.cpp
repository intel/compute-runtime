/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_ALDERLAKE_S> : public SamplerCoreFamily<IGFX_GEN12LP_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_ALDERLAKE_S, SamplerProductFamily<IGFX_ALDERLAKE_S>> populateADLS;

} // namespace L0
