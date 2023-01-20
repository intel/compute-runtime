/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/sampler/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_DG1> : public SamplerCoreFamily<IGFX_GEN12LP_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_DG1, SamplerProductFamily<IGFX_DG1>> populateDG1;

} // namespace L0
