/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

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

    bool result = euThread.stopThread(0x1234);

    EXPECT_TRUE(result);
    EXPECT_TRUE(euThread.isStopped());
    EXPECT_FALSE(euThread.isRunning());
    EXPECT_EQ(0x1234u, euThread.getMemoryHandle());

    result = euThread.stopThread(0x5678);

    EXPECT_FALSE(result);
    EXPECT_TRUE(euThread.isStopped());
    EXPECT_FALSE(euThread.isRunning());
    EXPECT_EQ(0x5678u, euThread.getMemoryHandle());

    result = euThread.resumeThread();
    EXPECT_EQ(EuThread::invalidHandle, euThread.getMemoryHandle());

    EXPECT_TRUE(result);
    EXPECT_FALSE(euThread.isStopped());
    EXPECT_TRUE(euThread.isRunning());

    result = euThread.resumeThread();

    EXPECT_FALSE(result);
    EXPECT_FALSE(euThread.isStopped());
    EXPECT_TRUE(euThread.isRunning());
    EXPECT_EQ(EuThread::invalidHandle, euThread.getMemoryHandle());
}

TEST(EuThread, GivenEuThreadWhenToStringCalledThenCorrectStringReturned) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    auto threadString = euThread.toString();
    EXPECT_EQ("device index = 0 slice = 3 subslice = 4 eu = 5 thread = 6", threadString);
}

TEST(EuThread, GivenThreadStateRunningWhenVerifyingStopWithOddCounterThenTrueReturnedAndStateStopped) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);
    EXPECT_TRUE(euThread.isRunning());

    EXPECT_TRUE(euThread.verifyStopped(1));
    EXPECT_TRUE(euThread.isStopped());
}

TEST(EuThread, GivenThreadStateStoppedWhenVerifyingStopWithOddCounterThenTrueReturnedAndStateStopped) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);
    euThread.stopThread(1u);

    EXPECT_TRUE(euThread.verifyStopped(1));
    EXPECT_TRUE(euThread.isStopped());
}

TEST(EuThread, GivenThreadStateStoppedWhenVerifyingStopWithEvenCounterThenFalseReturnedAndStateRunning) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);
    euThread.stopThread(1u);

    EXPECT_FALSE(euThread.verifyStopped(2));
    EXPECT_TRUE(euThread.isRunning());
}

TEST(EuThread, GivenEnabledErrorLogsWhenThreadStateStoppedAndVerifyingStopWithEvenCounterThenErrorMessageIsPrinted) {

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    ze_device_thread_t devThread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);
    euThread.stopThread(1u);

    ::testing::internal::CaptureStderr();

    EXPECT_FALSE(euThread.verifyStopped(2));
    EXPECT_TRUE(euThread.isRunning());

    auto message = ::testing::internal::GetCapturedStderr();
    EXPECT_STREQ("\nERROR: Thread: device index = 0 slice = 0 subslice = 0 eu = 0 thread = 0 state STOPPED when thread is running. Switching to RUNNING", message.c_str());
}

TEST(EuThread, GivenThreadStateRunningWhenVerifyingStopWithOddCounterForSecondStopThenTrueIsReturnedAndStateStopped) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);
    euThread.resumeThread();

    EXPECT_TRUE(euThread.verifyStopped(3));
    EXPECT_TRUE(euThread.isStopped());
}

TEST(EuThread, GivenThreadStateRunningWhenVerifyingStopWithEvenCounteThenFalseIsReturnedAndStateRunning) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);
    euThread.verifyStopped(1);
    euThread.resumeThread();

    EXPECT_FALSE(euThread.verifyStopped(2));
    EXPECT_TRUE(euThread.isRunning());
}

TEST(EuThread, GivenThreadStateStoppedWhenVerifyingStopWithOddCounterBiggerByMoreThanTwoThenTrueIsReturnedAndStateStopped) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);
    euThread.verifyStopped(1);

    EXPECT_TRUE(euThread.verifyStopped(7));
    EXPECT_TRUE(euThread.isStopped());
}

TEST(EuThread, GivenEnabledErrorLogsWhenThreadStateStoppedAndVerifyingStopWithOddCounterBiggerByMoreThanTwoThenErrorMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    ze_device_thread_t devThread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);

    ::testing::internal::CaptureStderr();

    EXPECT_TRUE(euThread.verifyStopped(7));
    EXPECT_TRUE(euThread.isStopped());

    auto message = ::testing::internal::GetCapturedStderr();
    EXPECT_STREQ("\nERROR: Thread: device index = 0 slice = 0 subslice = 0 eu = 0 thread = 0 state out of sync.", message.c_str());
}

TEST(EuThread, GivenEnabledErrorLogsWhenThreadStateRunningAndVerifyingStopWithOddCounterEqualToPreviousThenErrorMessageIsPrinted) {

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::LOG_ERROR);

    ze_device_thread_t devThread = {0, 0, 0, 0};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    euThread.verifyStopped(1);
    euThread.resumeThread();

    ::testing::internal::CaptureStderr();

    EXPECT_TRUE(euThread.verifyStopped(1));
    EXPECT_TRUE(euThread.isStopped());

    auto message = ::testing::internal::GetCapturedStderr();
    EXPECT_STREQ("\nERROR: Thread: device index = 0 slice = 0 subslice = 0 eu = 0 thread = 0 state RUNNING when thread is stopped. Switching to STOPPED", message.c_str());
}

TEST(EuThread, GivenThreadStateStoppedWhenVerifyingStopWithEvenCounterBiggerByMoreThanTwoThenFalseIsReturnedAndStateRunning) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);
    euThread.verifyStopped(1);

    EXPECT_FALSE(euThread.verifyStopped(8));
    EXPECT_TRUE(euThread.isRunning());
}

TEST(EuThread, GivenEuThreadWhenGettingLastCounterThenCorrectValueIsReturned) {
    ze_device_thread_t devThread = {3, 4, 5, 6};
    EuThread::ThreadId threadId(0, devThread);
    EuThread euThread(threadId);

    EXPECT_EQ(0u, euThread.getLastCounter());
    euThread.verifyStopped(1);
    EXPECT_EQ(1u, euThread.getLastCounter());
    euThread.verifyStopped(9);
    EXPECT_EQ(9u, euThread.getLastCounter());
}

} // namespace ult
} // namespace L0
