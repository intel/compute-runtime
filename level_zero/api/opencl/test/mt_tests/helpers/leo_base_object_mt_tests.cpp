/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

using BaseObjectMtTests = Test<OclFixture>;

TEST_F(BaseObjectMtTests, givenOwnershipHeldWhenAnotherThreadTakesItThenItWaitsUntilTheLockIsReleased) {
    std::promise<void> threadStarted;
    std::promise<void> ownershipTaken;
    auto threadStartedFuture = threadStarted.get_future();
    auto ownershipTakenFuture = ownershipTaken.get_future();

    bool ownsLock = false;
    bool takenAfterRelease = false;
    std::atomic<bool> released{false};

    auto lock = platform->takeOwnership();

    std::thread worker([&]() {
        threadStarted.set_value();
        auto otherLock = platform->takeOwnership();
        ownsLock = otherLock.owns_lock();
        takenAfterRelease = released.load();
        ownershipTaken.set_value();
    });

    threadStartedFuture.wait();
    EXPECT_EQ(std::future_status::timeout, ownershipTakenFuture.wait_for(std::chrono::milliseconds(1)));

    released.store(true);
    lock.unlock();
    worker.join();

    EXPECT_TRUE(ownsLock);
    EXPECT_TRUE(takenAfterRelease);
}

TEST_F(BaseObjectMtTests, givenOwnershipOfOneObjectWhenAnotherObjectIsLockedOnAnotherThreadThenItIsNotBlocked) {
    auto otherPlatform = std::make_unique<Platform>(driverHandle->toHandle());

    auto lock = platform->takeOwnership();

    std::atomic<bool> ownershipTaken{false};
    std::thread worker([&]() {
        auto otherLock = otherPlatform->takeOwnership();
        ownershipTaken.store(otherLock.owns_lock());
    });
    worker.join();

    EXPECT_TRUE(ownershipTaken.load());
}

TEST_F(BaseObjectMtTests, givenConcurrentCastToObjectWhileOwnershipIsHeldThenEveryThreadRecoversTheSameObject) {
    constexpr uint32_t numThreads = 4u;
    cl_platform_id clPlatform = platform;

    auto lock = platform->takeOwnership();

    std::atomic<uint32_t> successfulCasts{0};
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&]() {
            if (castToObject<Platform>(clPlatform) == platform) {
                successfulCasts++;
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(numThreads, successfulCasts.load());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
