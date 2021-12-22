/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/sampler_patch_values.h"

using namespace L0;

TEST(SamplerPatchValueTest, givenSamplerAddressingModeWhenGetingPathValueThenCorrectValueReturned) {
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_REPEAT), SamplerPatchValues::AddressRepeat);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER), SamplerPatchValues::AddressClampToBorder);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_CLAMP), SamplerPatchValues::AddressClampToEdge);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_NONE), SamplerPatchValues::AddressNone);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_MIRROR), SamplerPatchValues::AddressMirroredRepeat);
    EXPECT_EQ(getAddrMode(ZE_SAMPLER_ADDRESS_MODE_FORCE_UINT32), SamplerPatchValues::AddressNone);
}