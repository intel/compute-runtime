/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/kernel_helpers.h"
#include "test.h"

using namespace NEO;

struct KernelHelperMaxWorkGroupsTests : ::testing::Test {
    uint32_t simd = 8;
    uint32_t threadCount = 8 * 1024;
    uint32_t dssCount = 16;
    uint32_t availableSlm = 64 * KB;
    uint32_t usedSlm = 0;
    uint32_t maxBarrierCount = 32;
    uint32_t numberOfBarriers = 0;
    uint32_t workDim = 3;
    size_t lws[3] = {10, 10, 10};

    uint32_t getMaxWorkGroupCount() {
        return KernelHelper::getMaxWorkGroupCount(simd, threadCount, dssCount, availableSlm, usedSlm,
                                                  maxBarrierCount, numberOfBarriers, workDim, lws);
    }
};

TEST_F(KernelHelperMaxWorkGroupsTests, GivenNoBarriersOrSlmUsedWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithSimd) {
    auto workGroupSize = lws[0] * lws[1] * lws[2];
    auto expected = threadCount / Math::divideAndRoundUp(workGroupSize, simd);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenBarriersWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToBarriersCount) {
    numberOfBarriers = 16;

    auto expected = dssCount * (maxBarrierCount / numberOfBarriers);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenUsedSlmSizeWhenCalculatingMaxWorkGroupsCountThenResultIsCalculatedWithRegardToUsedSlmSize) {
    usedSlm = 4 * KB;

    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

TEST_F(KernelHelperMaxWorkGroupsTests, GivenVariousValuesWhenCalculatingMaxWorkGroupsCountThenLowestResultIsAlwaysReturned) {
    usedSlm = 1 * KB;
    numberOfBarriers = 1;
    dssCount = 1;

    workDim = 1;
    lws[0] = simd;
    threadCount = 1;
    EXPECT_EQ(1u, getMaxWorkGroupCount());

    threadCount = 1024;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    numberOfBarriers = 32;
    EXPECT_EQ(1u, getMaxWorkGroupCount());

    numberOfBarriers = 1;
    EXPECT_NE(1u, getMaxWorkGroupCount());

    usedSlm = availableSlm;
    EXPECT_EQ(1u, getMaxWorkGroupCount());
}
