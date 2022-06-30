/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(BaseObjectTestsMt, givenObjectOwnershipForEachThreadWhenIncrementingNonAtomicValueThenNoDataRacesAreExpected) {
    MockCommandQueue *object = new MockCommandQueue;
    object->takeOwnership();
    uint32_t counter = 0;
    const uint32_t loopCount = 50;
    const uint32_t numThreads = 3;

    auto incrementNonAtomicValue = [&](CommandQueue *obj) {
        for (uint32_t i = 0; i < loopCount; i++) {
            obj->takeOwnership();
            counter++;
            obj->releaseOwnership();
        }
    };

    EXPECT_EQ(0U, object->getCond().peekNumWaiters());

    std::thread t1(incrementNonAtomicValue, object);
    std::thread t2(incrementNonAtomicValue, object);
    std::thread t3(incrementNonAtomicValue, object);

    while (object->getCond().peekNumWaiters() != numThreads) {
        std::this_thread::yield();
    }

    EXPECT_EQ(0u, counter);
    object->releaseOwnership();

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(loopCount * numThreads, counter);

    object->release();
}
} // namespace NEO
