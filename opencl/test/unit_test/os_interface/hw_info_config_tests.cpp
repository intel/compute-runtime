/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/hw_info_config_tests.h"

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

void HwInfoConfigTest::SetUp() {
    PlatformFixture::SetUp();

    pInHwInfo = pPlatform->getClDevice(0)->getHardwareInfo();

    testPlatform = &pInHwInfo.platform;
    testSkuTable = &pInHwInfo.featureTable;
    testWaTable = &pInHwInfo.workaroundTable;
    testSysInfo = &pInHwInfo.gtSystemInfo;

    outHwInfo = {};
}

void HwInfoConfigTest::TearDown() {
    PlatformFixture::TearDown();
}

HWTEST_F(HwInfoConfigTest, givenDebugFlagSetWhenAskingForHostMemCapabilitesThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);

    DebugManager.flags.EnableHostUsmSupport.set(0);
    EXPECT_EQ(0u, hwInfoConfig->getHostMemCapabilities(&pInHwInfo));

    DebugManager.flags.EnableHostUsmSupport.set(1);
    EXPECT_NE(0u, hwInfoConfig->getHostMemCapabilities(&pInHwInfo));
}

TEST_F(HwInfoConfigTest, WhenParsingHwInfoConfigThenCorrectValuesAreReturned) {
    uint64_t hwInfoConfig = 0x0;

    bool success = parseHwInfoConfigString("1x1x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100010001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 1u);

    success = parseHwInfoConfigString("7x1x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x700010001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 7u);

    success = parseHwInfoConfigString("1x7x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100070001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 7u);

    success = parseHwInfoConfigString("1x1x7", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100010007u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 7u);

    success = parseHwInfoConfigString("2x4x16", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(0x200040010u, hwInfoConfig);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 2u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 8u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 8u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 128u);
}

TEST_F(HwInfoConfigTest, givenInvalidHwInfoWhenParsingHwInfoConfigThenErrorIsReturned) {
    uint64_t hwInfoConfig = 0x0;
    bool success = parseHwInfoConfigString("1", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x3", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("65536x3x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x65536x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x3x65536", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("65535x65535x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x65535x65535", hwInfoConfig);
    EXPECT_FALSE(success);
}

HWTEST_F(HwInfoConfigTest, whenConvertingTimestampsToCsDomainThenNothingIsChanged) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    uint64_t timestampData = 0x1234;
    uint64_t initialData = timestampData;
    hwInfoConfig->convertTimestampsFromOaToCsDomain(timestampData);
    EXPECT_EQ(initialData, timestampData);
}

HWTEST_F(HwInfoConfigTest, givenSamplerStateWhenAdjustSamplerStateThenNothingIsChanged) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    auto context = clUniquePtr(new MockContext());
    auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
    auto state = FamilyType::cmdInitSamplerState;
    auto initialState = state;
    hwInfoConfig->adjustSamplerState(&state, pInHwInfo);

    EXPECT_EQ(0, memcmp(&initialState, &state, sizeof(SAMPLER_STATE)));
}

HWTEST_F(HwInfoConfigTest, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    bool ret = hwInfoConfig->isAdditionalStateBaseAddressWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}

HWTEST_F(HwInfoConfigTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    bool ret = hwInfoConfig->isMaxThreadsForWorkgroupWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}
