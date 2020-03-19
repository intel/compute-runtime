/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_TIGERLAKE_LP> : public SamplerCoreFamily<IGFX_GEN12LP_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_TIGERLAKE_LP, SamplerProductFamily<IGFX_TIGERLAKE_LP>> populateTGLLP;

} // namespace L0
