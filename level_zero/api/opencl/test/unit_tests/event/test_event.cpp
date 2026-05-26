/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/event/event.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(EventHandleSpanTests, givenZeroCountAndNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{0, nullptr};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenNonZeroCountAndNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{5, nullptr};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenZeroCountAndNonNullListWhenConstructThenSizeIsZeroAndDataIsNull) {
    cl_event fakeEvent = reinterpret_cast<cl_event>(0x1);
    EventHandleSpan span{0, &fakeEvent};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

TEST(EventHandleSpanTests, givenDefaultConstructedWhenQueryThenSizeIsZeroAndDataIsNull) {
    EventHandleSpan span{};
    EXPECT_EQ(0u, span.size());
    EXPECT_EQ(nullptr, span.data());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
