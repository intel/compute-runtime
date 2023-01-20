/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncodeMTLTest = ::testing::Test;

MTLTEST_F(CommandEncodeMTLTest, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    uint8_t buffer[64]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}
