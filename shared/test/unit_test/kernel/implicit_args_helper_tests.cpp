/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(ImplicitArgsHelperTest, whenLocalIdsAreGeneratedByRuntimeThenDimensionOrderIsTakedFromInput) {
    for (auto i = 0u; i < HwWalkOrderHelper::walkOrderPossibilties; i++) {
        uint8_t inputDimensionOrder[3] = {2, 0, 1};
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(inputDimensionOrder, true, i);
        EXPECT_EQ(inputDimensionOrder[0], dimOrderForImplicitArgs[0]);
        EXPECT_EQ(inputDimensionOrder[1], dimOrderForImplicitArgs[1]);
        EXPECT_EQ(inputDimensionOrder[2], dimOrderForImplicitArgs[2]);
    }
}

TEST(ImplicitArgsHelperTest, givenIncorrectcInputWhenGettingDimensionOrderThenAbortIsCalled) {
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, true, 0), std::runtime_error);
    EXPECT_THROW(ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, false, HwWalkOrderHelper::walkOrderPossibilties), std::runtime_error);
}

TEST(ImplicitArgsHelperTest, whenLocalIdsAreGeneratedByHwThenProperDimensionOrderIsReturned) {
    for (auto i = 0u; i < HwWalkOrderHelper::walkOrderPossibilties; i++) {
        auto dimOrderForImplicitArgs = ImplicitArgsHelper::getDimensionOrderForLocalIds(nullptr, false, i);
        EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[i], dimOrderForImplicitArgs);
    }
}
