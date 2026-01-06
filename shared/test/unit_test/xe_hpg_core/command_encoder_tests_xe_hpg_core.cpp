/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "neo_aot_platforms.h"

using namespace NEO;

using L3ControlTests = ::testing::Test;

HWTEST2_F(L3ControlTests, givenL3ControlWhenAdjustCalledThenUnTypedDataPortCacheFlushIsSet, IsXeHpgCore) {
    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto l3Control = FamilyType::cmdInitL3Control;
    auto l3ControlOnStart = l3Control;

    adjustL3ControlField<FamilyType>(&l3Control);
    EXPECT_NE(0, memcmp(&l3ControlOnStart, &l3Control, sizeof(L3_CONTROL))); // no change

    EXPECT_FALSE(l3ControlOnStart.getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(l3Control.getUnTypedDataPortCacheFlush());
}

void testProgrammingStateComputeModeXeLpgWithEnabledWa(ExecutionEnvironment &executionEnvironment) {
    using STATE_COMPUTE_MODE = typename XeHpgCoreFamily::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = XeHpgCoreFamily::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= XeHpgCoreFamily::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    XeHpgCoreFamily::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

void testProgrammingStateComputeModeXeLpgWithDisabledWa(ExecutionEnvironment &executionEnvironment) {
    using STATE_COMPUTE_MODE = typename XeHpgCoreFamily::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.isDirty = true;
    properties.zPassAsyncComputeThreadLimit.isDirty = true;
    properties.pixelAsyncComputeThreadLimit.isDirty = true;
    properties.largeGrfMode.isDirty = true;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<XeHpgCoreFamily>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = XeHpgCoreFamily::stateComputeModeZPassAsyncComputeThreadLimitMask | XeHpgCoreFamily::stateComputeModePixelAsyncComputeThreadLimitMask |
                        XeHpgCoreFamily::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

using CommandEncoderXeHpgTests = ::testing::Test;

HWTEST2_F(CommandEncoderXeHpgTests, whenProgrammingStateComputeModeThenProperFieldsAreSet, IsXeLpg) {
    AOT::PRODUCT_CONFIG ipReleases[] = {AOT::MTL_U_A0, AOT::MTL_U_B0, AOT::MTL_H_A0, AOT::MTL_H_B0, AOT::ARL_H_A0, AOT::ARL_H_B0};
    for (auto &ipRelease : ipReleases) {

        MockExecutionEnvironment executionEnvironment{};
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.releaseHelper = ReleaseHelper::create(ipRelease);
        if (rootDeviceEnvironment.releaseHelper->isProgramAllStateComputeCommandFieldsWARequired()) {
            testProgrammingStateComputeModeXeLpgWithEnabledWa(executionEnvironment);
        } else {
            testProgrammingStateComputeModeXeLpgWithDisabledWa(executionEnvironment);
        }
    }
}
