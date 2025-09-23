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

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWhenTimeoutThenDirectSubmissionsAreChecked) {
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
    controller.startThread();
    csr.startControllingDirectSubmissions();
    controller.registerDirectSubmission(&csr);

    while (controller.directSubmissions[&csr].taskCount != 9u) {
        std::this_thread::yield();
    }
    while (!controller.directSubmissions[&csr].isStopped) {
        std::this_thread::yield();
    }
    {
        std::lock_guard<std::mutex> lock(controller.directSubmissionsMutex);
        EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
        EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 9u);
    }
    controller.stopThread();
    controller.unregisterDirectSubmission(&csr);
    executionEnvironment.directSubmissionController.release();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);
    controller.startControlling();

    while (!controller.sleepCalled) {
        std::this_thread::yield();
    }
    controller.stopThread();
}

TEST(DirectSubmissionControllerTestsMt, givenDirectSubmissionControllerWithNotStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    controller.startThread();
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    while (!controller.sleepCalled) {
        std::this_thread::yield();
    }
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
    while (!controller.sleepCalled) {
        std::this_thread::yield();
    }
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);

    controller.enqueueWaitForPagingFence(&csr, 10u);
    // Wait until csr is not updated
    while (csr.pagingFenceValueToUnblock == 0u) {
        std::this_thread::yield();
    }
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);

    // Verify that controller is able to handle requests during controlling
    controller.startControlling();

    controller.enqueueWaitForPagingFence(&csr, 20u);

    while (csr.pagingFenceValueToUnblock == 10u) {
        std::this_thread::yield();
    }
    EXPECT_EQ(20u, csr.pagingFenceValueToUnblock);

    controller.stopThread();
}

} // namespace NEO