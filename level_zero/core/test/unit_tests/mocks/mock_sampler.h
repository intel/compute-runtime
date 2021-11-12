/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/sampler/sampler_hw.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockSamplerHw : public L0::SamplerCoreFamily<gfxCoreFamily> {
    using BaseClass = ::L0::SamplerCoreFamily<gfxCoreFamily>;
    using BaseClass::lodMax;
    using BaseClass::lodMin;
    using BaseClass::samplerState;
};

} // namespace ult
} // namespace L0
