/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

TEST(ContextExtraPropertiesTests, givenAnyBufferWhenGettingCsrForBlitOperationThenNullIsReturned) {
    MockContext context;
    MockBuffer buffer;
    auto csr = context.getCommandStreamReceiverForBlitOperation(buffer);
    EXPECT_EQ(nullptr, csr);
}
