/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/sampler/sampler.h"
#include "level_zero/core/source/sampler/sampler_hw.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

using Sampler = WhiteBox<::L0::Sampler>;

template <>
struct Mock<Sampler> : public Sampler {
    Mock() = default;

    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD2(create, ze_result_t(Device *, const ze_sampler_desc_t *));
};

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockSamplerHw : public L0::SamplerCoreFamily<gfxCoreFamily> {
    using BaseClass = ::L0::SamplerCoreFamily<gfxCoreFamily>;
    using BaseClass::lodMax;
    using BaseClass::lodMin;
    using BaseClass::samplerState;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
