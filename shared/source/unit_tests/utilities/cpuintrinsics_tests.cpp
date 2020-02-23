/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>

extern std::atomic<uintptr_t> lastClFlushedPtr;
extern std::atomic<uint32_t> pauseCounter;

TEST(CpuIntrinsicsTest, whenClFlushIsCalledThenExpectToPassPtrToSystemCall) {
    uintptr_t flushAddr = 0x1234;
    void const *ptr = reinterpret_cast<void const *>(flushAddr);
    NEO::CpuIntrinsics::clFlush(ptr);
    EXPECT_EQ(flushAddr, lastClFlushedPtr);
}

TEST(CpuIntrinsicsTest, whenPauseCalledThenExpectToIncreaseCounter) {
    uint32_t oldCount = pauseCounter.load();
    NEO::CpuIntrinsics::pause();
    EXPECT_EQ(oldCount + 1, pauseCounter);
}
