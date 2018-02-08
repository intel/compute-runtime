/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/spinlock.h"
#include "gtest/gtest.h"
#include <thread>

using namespace OCLRT;

TEST(SpinLockTest, givenTwoThreadsThenVerifyThatTheySynchronizeWithSpinLock) {
    std::atomic_flag syncLock = ATOMIC_FLAG_INIT;
    std::atomic<bool> threadStarted(false);
    std::atomic<bool> threadFinished(false);
    SpinLock lock1;
    int sharedCount = 0;

    // Initially acquire spin lock so the worker thread will wait
    lock1.enter(syncLock);

    // Start worker thread
    std::thread t([&]() {
        threadStarted = true;
        SpinLock lock2;
        lock2.enter(syncLock);
        sharedCount++;
        EXPECT_EQ(2, sharedCount);
        lock2.leave(syncLock);
        threadFinished = true;
    });

    // Wait till worker thread is started
    while (!threadStarted) {
    };
    sharedCount++;
    EXPECT_EQ(1, sharedCount);

    // Release spin lock thus allowing worker thread to proceed
    lock1.leave(syncLock);

    // Wait till worker thread finishes
    while (!threadFinished) {
    };
    EXPECT_EQ(2, sharedCount);
    t.join();
}
