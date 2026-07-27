/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore.h"

#include "gtest/gtest.h"

namespace NEO {

struct MockBaseExternalSemaphore : public ExternalSemaphore {
    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override { return true; }
    bool enqueueWait(uint64_t *fenceValue) override { return true; }
    bool enqueueSignal(uint64_t *fenceValue) override { return true; }
};

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenAcquireWaitFenceValueIsCalledThenPassedValueIsReturned) {
    MockBaseExternalSemaphore semaphore;

    EXPECT_EQ(0ull, semaphore.acquireWaitFenceValue(0ull));
    EXPECT_EQ(123ull, semaphore.acquireWaitFenceValue(123ull));
    EXPECT_EQ(123ull, semaphore.acquireWaitFenceValue(123ull));
}

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenAcquireSignalFenceValueIsCalledThenPassedValueIsReturned) {
    MockBaseExternalSemaphore semaphore;

    EXPECT_EQ(0ull, semaphore.acquireSignalFenceValue(0ull));
    EXPECT_EQ(321ull, semaphore.acquireSignalFenceValue(321ull));
    EXPECT_EQ(321ull, semaphore.acquireSignalFenceValue(321ull));
}

} // namespace NEO
