/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"

namespace NEO {

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWhenNewSubmissionThenDirectSubmissionsAreChecked) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.rootDeviceEnvironments[0]->initOsTime();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    DirectSubmissionControllerMock controller;
    executionEnvironment.directSubmissionController.reset(&controller);
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller.registerDirectSubmission(&csr);
    controller.startThread();
    // Nothing to do, we are deep sleeping on condition var
    while (!controller.inDeepSleep.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.inDeepSleep.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with new submission, work should be done and wait again
    csr.taskCount = 10;
    controller.notifyNewSubmission();

    // We need to sleep here to give worker thread a chance to wake up, we can not check inDeepSleep immediately here
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    while (!controller.inDeepSleep.load()) {
        std::this_thread::yield();
    }

    // Work is done, verify results
    EXPECT_TRUE(controller.inDeepSleep.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    EXPECT_TRUE(controller.directSubmissions[&csr].isStopped.load());
    EXPECT_EQ(10u, controller.directSubmissions[&csr].taskCount.load());
    EXPECT_EQ(10u, csr.peekTaskCount());

    controller.stopThread();
    controller.unregisterDirectSubmission(&csr);
    executionEnvironment.directSubmissionController.release();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    while (!controller.inDeepSleep.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.inDeepSleep.load());
    controller.stopThread();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWhenEnqueuedWaitForPagingFenceThenRequestHandled) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.rootDeviceEnvironments[0]->initOsTime();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    DirectSubmissionControllerMock controller;
    controller.sleepCalled.store(false);
    controller.startThread();

    // Nothing to do, deep sleep
    while (!controller.inDeepSleep.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.inDeepSleep.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with paging fence, work should be done and wait again
    controller.inDeepSleep.store(false);
    EXPECT_FALSE(controller.inDeepSleep.load());

    controller.enqueueWaitForPagingFence(&csr, 10u);
    while (!controller.inDeepSleep.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.inDeepSleep.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
    controller.stopThread();
}

} // namespace NEO
