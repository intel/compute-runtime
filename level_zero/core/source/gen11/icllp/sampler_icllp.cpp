/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen11/hw_info.h"

#include "level_zero/core/source/sampler_hw.inl"

namespace L0 {

template <>
struct SamplerProductFamily<IGFX_ICELAKE_LP> : public SamplerCoreFamily<IGFX_GEN11_CORE> {
    using SamplerCoreFamily::SamplerCoreFamily;
};

static SamplerPopulateFactory<IGFX_ICELAKE_LP, SamplerProductFamily<IGFX_ICELAKE_LP>> populateICLLP;

} // namespace L0
