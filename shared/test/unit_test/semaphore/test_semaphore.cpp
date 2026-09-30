/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_external_semaphore.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenAcquireWaitFenceValueIsCalledThenPassedValueIsReturned) {
    MockExternalSemaphore semaphore;

    EXPECT_EQ(0ull, semaphore.acquireWaitFenceValue(0ull));
    EXPECT_EQ(123ull, semaphore.acquireWaitFenceValue(123ull));
    EXPECT_EQ(123ull, semaphore.acquireWaitFenceValue(123ull));
}

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenAcquireSignalFenceValueIsCalledThenPassedValueIsReturned) {
    MockExternalSemaphore semaphore;

    EXPECT_EQ(0ull, semaphore.acquireSignalFenceValue(0ull));
    EXPECT_EQ(321ull, semaphore.acquireSignalFenceValue(321ull));
    EXPECT_EQ(321ull, semaphore.acquireSignalFenceValue(321ull));
}

TEST(ExternalSemaphoreBaseTest, givenBaseExternalSemaphoreWhenNotImportedThenTypeIsInvalidAndSyncHandleIsZero) {
    MockExternalSemaphore semaphore;

    EXPECT_EQ(ExternalSemaphore::Type::Invalid, semaphore.getType());
    EXPECT_EQ(0u, semaphore.getSyncHandle());
}

TEST(ExternalSemaphoreBaseTest, givenBaseDriverModelWhenExternalSemaphoreOperationsAreCalledThenFalseIsReturned) {
    MockDriverModel driverModel;
    MockExternalSemaphore semaphore;

    const ExternalSemaphoreOperation operations[] = {{&semaphore, 1u}};
    EXPECT_FALSE(driverModel.waitExternalSemaphoresFromCpu(operations));
    EXPECT_FALSE(driverModel.signalExternalSemaphoresFromCpu(operations));
}

} // namespace NEO
