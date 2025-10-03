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
    // Controlling not started, wait until controller thread is waiting on condition var, no work done
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Start controlling, no submissions yet, wait until controller thread is waiting on condition var again
    controller.waitOnConditionVar.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());
    csr.startControllingDirectSubmissions();
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with new submissions, work should be done and wait again
    controller.waitOnConditionVar.store(false);
    controller.handlePagingFenceRequestsCalled.store(false);
    controller.sleepCalled.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    csr.taskCount = 10;
    controller.notifyNewSubmission();
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    // Work is done, verify results
    EXPECT_TRUE(controller.waitOnConditionVar.load());
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

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    controller.startControlling();

    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    controller.stopThread();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithNotStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.waitOnConditionVar.load());
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

    // No fence requests, wait until controller thread is waiting on condition var
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    // Wake up controller with paging fence, work should be done and wait again
    controller.waitOnConditionVar = false;
    EXPECT_FALSE(controller.waitOnConditionVar.load());

    controller.enqueueWaitForPagingFence(&csr, 10u);
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);

    // Reset test state
    controller.waitOnConditionVar.store(false);
    controller.handlePagingFenceRequestsCalled.store(false);
    controller.sleepCalled.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());

    // Start controlling, no submissions yet, wait until controller thread is waiting on condition var again
    controller.startControlling();
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Reset test state again
    controller.waitOnConditionVar.store(false);
    controller.handlePagingFenceRequestsCalled.store(false);
    controller.sleepCalled.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());

    // Wake with peging fence in controlling state
    controller.enqueueWaitForPagingFence(&csr, 20u);
    while (!controller.waitOnConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_EQ(20u, csr.pagingFenceValueToUnblock);

    controller.stopThread();
}

} // namespace NEO
