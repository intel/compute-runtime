/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/spinlock.h"
#include "gtest/gtest.h"
#include <thread>
#include <mutex>

using namespace OCLRT;

TEST(SpinLockTest, givenTwoThreadsThenVerifyThatTheySynchronizeWithSpinLock) {
    std::atomic<bool> threadStarted(false);
    std::atomic<bool> threadFinished(false);
    SpinLock spinLock;
    int sharedCount = 0;

    // Initially acquire spin lock so the worker thread will wait
    std::unique_lock<SpinLock> lock1{spinLock};

    // Start worker thread
    std::thread workerThread([&]() {
        threadStarted = true;
        std::unique_lock<SpinLock> lock2{spinLock};
        sharedCount++;
        EXPECT_EQ(2, sharedCount);
        lock2.unlock();
        threadFinished = true;
    });

    // Wait till worker thread is started
    while (!threadStarted)
        ;
    sharedCount++;
    EXPECT_EQ(1, sharedCount);

    // Release spin lock thus allowing worker thread to proceed
    lock1.unlock();

    // Wait till worker thread finishes
    while (!threadFinished)
        ;
    EXPECT_EQ(2, sharedCount);
    workerThread.join();
}

TEST(SpinLockTest, givenSpinLockThenAttemptedLockingWorks) {
    SpinLock spinLock;
    auto workerThreadFunction = [&spinLock](bool expectedLockAcquired) {
        std::unique_lock<SpinLock> lock{spinLock, std::defer_lock};
        auto lockAcquired = lock.try_lock();
        EXPECT_EQ(expectedLockAcquired, lockAcquired);
    };

    // Expect locking to fail when lock is already taken
    std::unique_lock<SpinLock> lock{spinLock};
    std::thread workerThread1(workerThreadFunction, false);
    workerThread1.join();

    lock.unlock();
    std::thread workerThread2(workerThreadFunction, true);
    workerThread2.join();
}
