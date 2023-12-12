/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/sampler_patch_values.h"

using namespace L0;

TEST(SamplerPatchValueTest, givenSamplerAddressingModeWhenGetingPathValueThenCorrectValueReturned) {
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_REPEAT), SamplerPatchValues::addressRepeat);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER), SamplerPatchValues::addressClampToBorder);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_CLAMP), SamplerPatchValues::addressClampToEdge);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_NONE), SamplerPatchValues::addressNone);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_MIRROR), SamplerPatchValues::addressMirroredRepeat);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_FORCE_UINT32), SamplerPatchValues::addressNone);
}