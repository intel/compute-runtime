/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"

namespace NEO {

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerTimeoutWhenCreateObjectThenTimeoutIsEqualWithDebugFlag) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionControllerTimeout.set(14);

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeout, 14);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterDirectSubmissionWorksThenItIsMonitoringItsState) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.taskCount.store(5u);

    DirectSubmissionControllerMock controller;
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();
    controller.registerDirectSubmission(&csr);

    controller.checkNewSubmissions();
    EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 5u);

    csr.taskCount.store(6u);
    controller.checkNewSubmissions();
    EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 6u);

    controller.checkNewSubmissions();
    EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 6u);

    controller.checkNewSubmissions();
    EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 6u);

    csr.taskCount.store(8u);
    controller.checkNewSubmissions();
    EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 8u);

    controller.unregisterDirectSubmission(&csr);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenTimeoutThenDirectSubmissionsAreChecked) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.initializeTagAllocation();
    *csr.tagAddress = 9u;
    csr.taskCount.store(9u);

    DirectSubmissionControllerMock controller;
    executionEnvironment.directSubmissionController.reset(&controller);
    csr.startControllingDirectSubmissions();
    controller.registerDirectSubmission(&csr);

    while (controller.directSubmissions[&csr].taskCount != 9u) {
    }
    while (!controller.directSubmissions[&csr].isStopped) {
    }

    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 9u);

    controller.unregisterDirectSubmission(&csr);
    executionEnvironment.directSubmissionController.release();
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWithStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    controller.startControlling();
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWithNotStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();
}

} // namespace NEO