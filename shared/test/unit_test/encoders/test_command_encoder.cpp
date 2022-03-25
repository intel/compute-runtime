/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

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

HWTEST_F(CommandEncoderTest, GivenDwordStoreWhenAddingStoreDataImmThenExpectDwordProgramming) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    size_t size = EncodeStoreMemory<FamilyType>::getStoreDataImmSize();
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), size);

    uint64_t gpuAddress = 0xFF0000;
    uint32_t dword0 = 0x123;
    uint32_t dword1 = 0x456;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream,
                                                       gpuAddress,
                                                       dword0,
                                                       dword1,
                                                       false,
                                                       false);
    size_t usedAfter = cmdStream.getUsed();
    EXPECT_EQ(size, usedAfter);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_EQ(dword0, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());
}

HWTEST_F(CommandEncoderTest, GivenQwordStoreWhenAddingStoreDataImmThenExpectQwordProgramming) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    size_t size = EncodeStoreMemory<FamilyType>::getStoreDataImmSize();
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), size);

    uint64_t gpuAddress = 0xFF0000;
    uint32_t dword0 = 0x123;
    uint32_t dword1 = 0x456;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream,
                                                       gpuAddress,
                                                       dword0,
                                                       dword1,
                                                       true,
                                                       false);
    size_t usedAfter = cmdStream.getUsed();
    EXPECT_EQ(size, usedAfter);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_EQ(dword0, storeDataImm->getDataDword0());
    EXPECT_EQ(dword1, storeDataImm->getDataDword1());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD, storeDataImm->getDwordLength());
}
