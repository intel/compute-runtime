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
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);

    csr1.setupContext(*osContext.get());
    csr2.setupContext(*osContext.get());

    DirectSubmissionControllerMock controller;
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);

    controller.registerDirectSubmission(&csr1);
    controller.registerDirectSubmission(&csr2);

    controller.startThread();
    controller.waitTillSleep();
    // Nothing to do, we are deep sleeping on condition var
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with new submission to csr1 only, work should be done and wait again
    controller.waitOnConditionVar.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());

    csr1.taskCount = 10;
    csr2.taskCount = 20;
    controller.notifyNewSubmission(&csr1);

    controller.waitTillSleep();

    // Work is done, verify results for csr1
    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    EXPECT_TRUE(controller.directSubmissions[&csr1].isStopped.load());
    EXPECT_EQ(10u, controller.directSubmissions[&csr1].taskCount.load());
    EXPECT_EQ(10u, csr1.peekTaskCount());

    // csr2 is unhandled despite task count is not 0, direct submission was not started on csr2
    EXPECT_FALSE(controller.directSubmissions[&csr2].isActive.load());
    EXPECT_TRUE(controller.directSubmissions[&csr2].isStopped.load());
    EXPECT_EQ(0u, controller.directSubmissions[&csr2].taskCount.load());
    EXPECT_EQ(20u, csr2.peekTaskCount());

    // csr2 should be as well handled when direct submission started
    controller.waitOnConditionVar.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());

    controller.notifyNewSubmission(&csr2);
    controller.waitTillSleep();

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_TRUE(controller.directSubmissions[&csr2].isStopped.load());
    EXPECT_EQ(20u, controller.directSubmissions[&csr2].taskCount.load());
    EXPECT_EQ(20u, csr2.peekTaskCount());
    controller.stopThread();
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    std::this_thread::yield();
    controller.stopThread();
    EXPECT_EQ(controller.directSubmissionControllingThread.get(), nullptr);
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
    controller.waitTillSleep();

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_FALSE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_FALSE(controller.sleepCalled.load());
    EXPECT_FALSE(controller.checkNewSubmissionCalled.load());

    // Wake up controller with paging fence, work should be done and wait again
    controller.waitOnConditionVar.store(false);
    EXPECT_FALSE(controller.waitOnConditionVar.load());

    controller.enqueueWaitForPagingFence(&csr, 10u);
    controller.waitTillSleep();

    EXPECT_TRUE(controller.waitOnConditionVar.load());
    EXPECT_TRUE(controller.sleepCalled.load());
    EXPECT_TRUE(controller.handlePagingFenceRequestsCalled.load());
    EXPECT_TRUE(controller.checkNewSubmissionCalled.load());
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
    controller.stopThread();
}

} // namespace NEO
