/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/test/common/mocks/mock_driver_model.h"

#include "gtest/gtest.h"

namespace NEO {

struct MockBaseExternalSemaphore : public ExternalSemaphore {
    ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override { return ImportResult::success; }
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

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenNotImportedThenTypeIsInvalidAndSyncHandleIsZero) {
    MockBaseExternalSemaphore semaphore;

    EXPECT_EQ(ExternalSemaphore::Type::Invalid, semaphore.getType());
    EXPECT_EQ(0u, semaphore.getSyncHandle());
}

TEST(ExternalSemaphoreBaseTest, givenBaseDriverModelWhenExternalSemaphoreOperationsAreCalledThenFalseIsReturned) {
    MockDriverModel driverModel;
    MockBaseExternalSemaphore semaphore;

    const ExternalSemaphoreOperation operations[] = {{&semaphore, 1u}};
    EXPECT_FALSE(driverModel.waitExternalSemaphoresFromCpu(operations));
    EXPECT_FALSE(driverModel.signalExternalSemaphoresFromCpu(operations));
}

} // namespace NEO
