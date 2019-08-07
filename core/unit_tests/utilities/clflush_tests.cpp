/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/utilities/clflush.h"

#include "gtest/gtest.h"

extern void const *lastClFlushedPtr;

TEST(ClFlushTest, whenClFlushIsCalledThenExpectToPassPtrToSystemCall) {
    void const *ptr = reinterpret_cast<void const *>(0x1234);
    NEO::CpuIntrinsics::clFlush(ptr);
    EXPECT_EQ(ptr, lastClFlushedPtr);
}
