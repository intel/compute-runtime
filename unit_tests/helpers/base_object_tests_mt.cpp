/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/platform/platform.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(BaseObjectTestsMt, givenObjectOwnershipForEachThreadWhenIncrementingNonAtomicValueThenNoDataRacesAreExpected) {
    CommandQueue *object = new CommandQueue;
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
