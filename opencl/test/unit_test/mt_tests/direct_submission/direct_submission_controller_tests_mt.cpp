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
    csr.initializeTagAllocation();
    *csr.tagAddress = 9u;
    csr.taskCount.store(9u);

    DirectSubmissionControllerMock controller;
    executionEnvironment.directSubmissionController.reset(&controller);
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller.registerDirectSubmission(&csr);
    csr.startControllingDirectSubmissions();
    controller.startThread();

    // No submissions reported, wait until controller thread is sleeping on condition var
    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with new submission, work should be done and sleep again
    controller.sleepOnNoWorkConditionVar = false;
    EXPECT_FALSE(controller.sleepOnNoWorkConditionVar.load());
    csr.notifyNewSubmission();
    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }

    // Work is done, verify results
    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    EXPECT_TRUE(controller.directSubmissions[&csr].isStopped.load());
    EXPECT_EQ(9u, controller.directSubmissions[&csr].taskCount.load());

    controller.stopThread();
    controller.unregisterDirectSubmission(&csr);
    executionEnvironment.directSubmissionController.release();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    controller.startControlling();

    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    controller.stopThread();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithNotStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
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

    // No fence requests, wait until controller thread is sleeping on condition var
    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }

    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    // Wake up controller with paging fence, work should be done and sleep again
    controller.sleepOnNoWorkConditionVar = false;
    EXPECT_FALSE(controller.sleepOnNoWorkConditionVar.load());

    controller.enqueueWaitForPagingFence(&csr, 10u);
    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);

    // Reset test state
    controller.sleepOnNoWorkConditionVar = false;
    controller.handlePagingFenceRequestsCalled = false;
    EXPECT_FALSE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());

    // The same as above but with active controlling
    controller.startControlling();
    controller.enqueueWaitForPagingFence(&csr, 20u);
    while (!controller.sleepOnNoWorkConditionVar.load()) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(controller.sleepOnNoWorkConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_EQ(20u, csr.pagingFenceValueToUnblock);

    controller.stopThread();
}

} // namespace NEO
