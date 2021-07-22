/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/tools/source/debug/eu_thread.h"

namespace L0 {
namespace ult {

TEST(EuThread, WhenConstructingEuThreadThenCorrectIdsAreSet) {
    ze_device_thread_t devThread = {3, 4, 5, 6};

    EuThread::ThreadId threadId(0, devThread);

    EXPECT_EQ(0u, threadId.tileIndex);
    EXPECT_EQ(3u, threadId.slice);
    EXPECT_EQ(4u, threadId.subslice);
    EXPECT_EQ(5u, threadId.eu);
    EXPECT_EQ(6u, threadId.thread);

    EuThread::ThreadId threadId2(3, 1, 2, 3, 4);

    EXPECT_EQ(3u, threadId2.tileIndex);
    EXPECT_EQ(1u, threadId2.slice);
    EXPECT_EQ(2u, threadId2.subslice);
    EXPECT_EQ(3u, threadId2.eu);
    EXPECT_EQ(4u, threadId2.thread);

    auto castValue = static_cast<uint64_t>(threadId2);

    EXPECT_EQ(threadId2.packed, castValue);
}

TEST(EuThread, GivenEuThreadWhenGettingThreadIdThenValidIdReturned) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    auto id = euThread.getThreadId();

    EXPECT_EQ(threadId.packed, id.packed);
}

TEST(EuThread, GivenEuThreadWhenChangingAndQueryingStatesThenStateIsChanged) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    EXPECT_FALSE(euThread.isStopped());
    EXPECT_TRUE(euThread.isRunning());

    bool result = euThread.stopThread();

    EXPECT_TRUE(result);
    EXPECT_TRUE(euThread.isStopped());
    EXPECT_FALSE(euThread.isRunning());

    result = euThread.stopThread();

    EXPECT_FALSE(result);
    EXPECT_TRUE(euThread.isStopped());
    EXPECT_FALSE(euThread.isRunning());

    result = euThread.resumeThread();

    EXPECT_TRUE(result);
    EXPECT_FALSE(euThread.isStopped());
    EXPECT_TRUE(euThread.isRunning());

    result = euThread.resumeThread();

    EXPECT_FALSE(result);
    EXPECT_FALSE(euThread.isStopped());
    EXPECT_TRUE(euThread.isRunning());
}

TEST(EuThread, GivenEuThreadWhenToStringCalledThenCorrectStringReturned) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    auto threadString = euThread.toString();
    EXPECT_EQ("device index = 0 slice = 3 subslice = 4 eu = 5 thread = 6", threadString);
}

} // namespace ult
} // namespace L0
