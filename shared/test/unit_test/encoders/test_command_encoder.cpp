/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ELKHARTLAKE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_TIGERLAKE_LP);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_LAKEFIELD);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ICELAKE_LP);

using CommandEncoderTest = Test<DeviceFixture>;

using Platforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ROCKETLAKE>;
HWTEST2_F(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, Platforms) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 76ul);
}

HWTEST2_F(CommandEncoderTest, givenGLLPWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsTGLLP) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 200ul);
}

HWTEST2_F(CommandEncoderTest, givenDG1WhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG1) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 200ul);
}

HWTEST2_F(CommandEncoderTest, givenEHLWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsEHL) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 88ul);
}

HWTEST2_F(CommandEncoderTest, givenRKLWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsRKL) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 104ul);
}

HWTEST2_F(CommandEncoderTest, givenLFKWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsLKF) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 88ul);
}

HWTEST2_F(CommandEncoderTest, givenICLLPWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsICLLP) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 88ul);
}
