/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncodeDG2Test = ::testing::Test;

DG2TEST_F(CommandEncodeDG2Test, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}
