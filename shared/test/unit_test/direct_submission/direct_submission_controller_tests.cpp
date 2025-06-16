/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/direct_submission/direct_submission_controller_mock.h"

namespace NEO {

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerTimeoutWhenCreateObjectThenTimeoutIsEqualWithDebugFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerTimeout.set(14);

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeout.count(), 14);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterDirectSubmissionWorksThenItIsMonitoringItsState) {
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
    csr.taskCount.store(5u);

    DirectSubmissionControllerMock controller;
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
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

TEST(DirectSubmissionControllerTests, givenDebugFlagSetWhenCheckingIfTimeoutElapsedThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    DirectSubmissionControllerMock defaultController;
    defaultController.timeoutElapsedCallBase.store(true);
    EXPECT_EQ(1, defaultController.bcsTimeoutDivisor);
    EXPECT_EQ(std::chrono::microseconds{defaultController.timeout}, defaultController.getSleepValue());

    debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.set(2);

    DirectSubmissionControllerMock controller;
    controller.timeoutElapsedCallBase.store(true);
    EXPECT_EQ(2, controller.bcsTimeoutDivisor);
    EXPECT_EQ(std::chrono::microseconds{controller.timeout / controller.bcsTimeoutDivisor}, controller.getSleepValue());

    auto now = SteadyClock::now();
    controller.timeSinceLastCheck = now;
    controller.cpuTimestamp = now;

    defaultController.timeSinceLastCheck = now;
    defaultController.cpuTimestamp = now;

    EXPECT_EQ(TimeoutElapsedMode::notElapsed, controller.timeoutElapsed());
    EXPECT_EQ(TimeoutElapsedMode::notElapsed, defaultController.timeoutElapsed());

    controller.cpuTimestamp = now + std::chrono::microseconds{controller.timeout / controller.bcsTimeoutDivisor};
    defaultController.cpuTimestamp = now + std::chrono::microseconds{defaultController.timeout - std::chrono::microseconds{1}};

    EXPECT_EQ(TimeoutElapsedMode::bcsOnly, controller.timeoutElapsed());
    EXPECT_EQ(TimeoutElapsedMode::notElapsed, defaultController.timeoutElapsed());

    controller.cpuTimestamp = now + std::chrono::microseconds{controller.timeout};
    defaultController.cpuTimestamp = now + std::chrono::microseconds{defaultController.timeout};
    EXPECT_EQ(TimeoutElapsedMode::fullyElapsed, controller.timeoutElapsed());
    EXPECT_EQ(TimeoutElapsedMode::fullyElapsed, defaultController.timeoutElapsed());
}

TEST(DirectSubmissionControllerTests, givenDebugFlagSetWhenCheckingNewSubmissionThenStopOnlyBcsEngines) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerBcsTimeoutDivisor.set(2);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    executionEnvironment.rootDeviceEnvironments[0]->initOsTime();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver bcsCsr(executionEnvironment, 0, deviceBitfield);
    MockCommandStreamReceiver ccsCsr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> bcsOsContext(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield)));
    std::unique_ptr<OsContext> ccsOsContext(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield)));
    bcsCsr.setupContext(*bcsOsContext.get());
    ccsCsr.setupContext(*ccsOsContext.get());
    bcsCsr.taskCount.store(5u);
    ccsCsr.taskCount.store(5u);

    DirectSubmissionControllerMock controller;
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller.registerDirectSubmission(&bcsCsr);
    controller.registerDirectSubmission(&ccsCsr);
    controller.checkNewSubmissions();

    EXPECT_FALSE(controller.directSubmissions[&bcsCsr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&bcsCsr].taskCount, 5u);
    EXPECT_FALSE(controller.directSubmissions[&ccsCsr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&ccsCsr].taskCount, 5u);

    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::bcsOnly);

    auto timeSinceLastCheck = controller.timeSinceLastCheck;

    bcsCsr.taskCount.store(6u);
    ccsCsr.taskCount.store(6u);
    controller.checkNewSubmissions();
    EXPECT_FALSE(controller.directSubmissions[&bcsCsr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&bcsCsr].taskCount, 6u);
    EXPECT_FALSE(controller.directSubmissions[&ccsCsr].isStopped);
    EXPECT_EQ(controller.directSubmissions[&ccsCsr].taskCount, 5u);

    controller.checkNewSubmissions();
    EXPECT_TRUE(controller.directSubmissions[&bcsCsr].isStopped);
    EXPECT_FALSE(controller.directSubmissions[&ccsCsr].isStopped);

    EXPECT_EQ(timeSinceLastCheck, controller.timeSinceLastCheck);

    controller.unregisterDirectSubmission(&bcsCsr);
    controller.unregisterDirectSubmission(&ccsCsr);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenIncreaseTimeoutEnabledThenTimeoutIsIncreased) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerTimeout.set(5'000);
    debugManager.flags.DirectSubmissionControllerMaxTimeout.set(200'000);
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
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller.registerDirectSubmission(&csr);
    {
        csr.taskCount.store(1u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 1u);

        auto previousTimestamp = controller.lastTerminateCpuTimestamp;
        controller.cpuTimestamp += std::chrono::microseconds(5'000);
        controller.checkNewSubmissions();
        EXPECT_EQ(std::chrono::duration_cast<std::chrono::microseconds>(controller.lastTerminateCpuTimestamp - previousTimestamp).count(), 5'000);
        EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 1u);
        EXPECT_EQ(controller.timeout.count(), 5'000);
        EXPECT_EQ(controller.maxTimeout.count(), 200'000);
    }
    {
        csr.taskCount.store(2u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 2u);

        auto previousTimestamp = controller.lastTerminateCpuTimestamp;
        controller.cpuTimestamp += std::chrono::microseconds(5'500);
        controller.checkNewSubmissions();
        EXPECT_EQ(std::chrono::duration_cast<std::chrono::microseconds>(controller.lastTerminateCpuTimestamp - previousTimestamp).count(), 5'500);
        EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 2u);
        EXPECT_EQ(controller.timeout.count(), 8'250);
    }
    {
        csr.taskCount.store(3u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 3u);

        auto previousTimestamp = controller.lastTerminateCpuTimestamp;
        controller.cpuTimestamp += controller.maxTimeout;
        controller.checkNewSubmissions();
        EXPECT_EQ(std::chrono::duration_cast<std::chrono::microseconds>(controller.lastTerminateCpuTimestamp - previousTimestamp).count(), controller.maxTimeout.count());
        EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 3u);
        EXPECT_EQ(controller.timeout.count(), controller.maxTimeout.count());
    }
    {
        controller.timeout = std::chrono::microseconds(5'000);
        csr.taskCount.store(4u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 4u);

        auto previousTimestamp = controller.lastTerminateCpuTimestamp;
        controller.cpuTimestamp += controller.maxTimeout * 2;
        controller.checkNewSubmissions();
        EXPECT_EQ(std::chrono::duration_cast<std::chrono::microseconds>(controller.lastTerminateCpuTimestamp - previousTimestamp).count(), controller.maxTimeout.count() * 2);
        EXPECT_TRUE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 4u);
        EXPECT_EQ(controller.timeout.count(), 5'000);
    }
    controller.unregisterDirectSubmission(&csr);
}

TEST(DirectSubmissionControllerTests, givenPowerSavingUintWhenCallingGetThrottleFromPowerSavingUintThenCorrectValueIsReturned) {
    EXPECT_EQ(QueueThrottle::MEDIUM, getThrottleFromPowerSavingUint(0u));
    EXPECT_EQ(QueueThrottle::LOW, getThrottleFromPowerSavingUint(1u));
    EXPECT_EQ(QueueThrottle::LOW, getThrottleFromPowerSavingUint(100u));
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenEnqueueWaitForPagingFenceThenWaitInQueue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    DirectSubmissionControllerMock controller;
    EXPECT_TRUE(controller.pagingFenceRequests.empty());
    controller.enqueueWaitForPagingFence(&csr, 10u);
    EXPECT_FALSE(controller.pagingFenceRequests.empty());

    auto request = controller.pagingFenceRequests.front();
    EXPECT_EQ(request.csr, &csr);
    EXPECT_EQ(request.pagingFenceValue, 10u);
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    controller.handlePagingFenceRequests(lock, false);
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);

    // Do nothing when queue is empty
    csr.pagingFenceValueToUnblock = 0u;
    controller.handlePagingFenceRequests(lock, false);
    EXPECT_EQ(0u, csr.pagingFenceValueToUnblock);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenDrainPagingFenceQueueThenPagingFenceHandled) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);

    DirectSubmissionControllerMock controller;
    EXPECT_TRUE(controller.pagingFenceRequests.empty());
    controller.enqueueWaitForPagingFence(&csr, 10u);
    EXPECT_FALSE(controller.pagingFenceRequests.empty());

    auto request = controller.pagingFenceRequests.front();
    EXPECT_EQ(request.csr, &csr);
    EXPECT_EQ(request.pagingFenceValue, 10u);

    controller.drainPagingFenceQueue();
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenEnqueueWaitForPagingFenceWithCheckSubmissionsThenCheckSubmissions) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    DirectSubmissionControllerMock controller;
    EXPECT_TRUE(controller.pagingFenceRequests.empty());
    controller.enqueueWaitForPagingFence(&csr, 10u);
    EXPECT_FALSE(controller.pagingFenceRequests.empty());

    auto request = controller.pagingFenceRequests.front();
    EXPECT_EQ(request.csr, &csr);
    EXPECT_EQ(request.pagingFenceValue, 10u);

    csr.taskCount.store(5u);
    controller.registerDirectSubmission(&csr);

    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    controller.timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller.handlePagingFenceRequests(lock, true);
    EXPECT_EQ(10u, csr.pagingFenceValueToUnblock);
    EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 5u);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenCheckTimeoutElapsedThenReturnCorrectValue) {
    DirectSubmissionControllerMock controller;
    controller.timeout = std::chrono::seconds(5);
    controller.timeoutElapsedCallBase.store(true);

    controller.timeSinceLastCheck = controller.getCpuTimestamp() - std::chrono::seconds(10);
    EXPECT_EQ(TimeoutElapsedMode::fullyElapsed, controller.timeoutElapsed());

    controller.timeSinceLastCheck = controller.getCpuTimestamp() - std::chrono::seconds(1);
    EXPECT_EQ(TimeoutElapsedMode::notElapsed, controller.timeoutElapsed());
}

struct TagUpdateMockCommandStreamReceiver : public MockCommandStreamReceiver {

    TagUpdateMockCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    SubmissionStatus flushTagUpdate() override {
        flushTagUpdateCalledTimes++;
        return SubmissionStatus::success;
    }

    bool isBusy() override {
        return isBusyReturnValue;
    }

    uint32_t flushTagUpdateCalledTimes = 0;
    bool isBusyReturnValue = false;
};

struct DirectSubmissionIdleDetectionTests : public ::testing::Test {
    void SetUp() override {
        controller = std::make_unique<DirectSubmissionControllerMock>();
        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.initializeMemoryManager();
        executionEnvironment.rootDeviceEnvironments[0]->osTime.reset(new MockOSTime{});

        DeviceBitfield deviceBitfield(1);
        csr = std::make_unique<TagUpdateMockCommandStreamReceiver>(executionEnvironment, 0, deviceBitfield);
        osContext.reset(OsContext::create(nullptr, 0, 0,
                                          EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                       PreemptionMode::ThreadGroup, deviceBitfield)));
        csr->setupContext(*osContext);

        controller->timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
        controller->registerDirectSubmission(csr.get());
        csr->taskCount.store(10u);
        controller->checkNewSubmissions();
    }

    void TearDown() override {
        controller->unregisterDirectSubmission(csr.get());
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContext> osContext;
    std::unique_ptr<TagUpdateMockCommandStreamReceiver> csr;
    std::unique_ptr<DirectSubmissionControllerMock> controller;
};

TEST_F(DirectSubmissionIdleDetectionTests, givenLatestFlushedTaskSameAsTaskCountAndGpuBusyThenDontTerminateDirectSubmission) {
    csr->setLatestFlushedTaskCount(10u);
    csr->isBusyReturnValue = true;

    controller->checkNewSubmissions();
    EXPECT_FALSE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(0u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(0u, csr->flushTagUpdateCalledTimes);
}

TEST_F(DirectSubmissionIdleDetectionTests, givenLatestFlushedTaskSameAsTaskCountAndGpuHangThenTerminateDirectSubmission) {
    csr->setLatestFlushedTaskCount(10u);
    csr->isBusyReturnValue = true;
    csr->isGpuHangDetectedReturnValue = true;

    controller->checkNewSubmissions();
    EXPECT_TRUE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(1u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(0u, csr->flushTagUpdateCalledTimes);
}

TEST_F(DirectSubmissionIdleDetectionTests, givenLatestFlushedTaskSameAsTaskCountAndGpuIdleThenTerminateDirectSubmission) {
    csr->setLatestFlushedTaskCount(10u);
    csr->isBusyReturnValue = false;

    controller->checkNewSubmissions();
    EXPECT_TRUE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(1u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(0u, csr->flushTagUpdateCalledTimes);
}

TEST_F(DirectSubmissionIdleDetectionTests, givenLatestFlushedTaskLowerThanTaskCountAndGpuBusyThenFlushTagAndDontTerminateDirectSubmission) {
    csr->isBusyReturnValue = true;

    controller->checkNewSubmissions();
    EXPECT_FALSE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(0u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(1u, csr->flushTagUpdateCalledTimes);
}

TEST_F(DirectSubmissionIdleDetectionTests, givenLatestFlushedTaskLowerThanTaskCountAndGpuIdleThenFlushTagAndTerminateDirectSubmission) {
    csr->isBusyReturnValue = false;

    controller->checkNewSubmissions();
    EXPECT_TRUE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(1u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(1u, csr->flushTagUpdateCalledTimes);
}

TEST_F(DirectSubmissionIdleDetectionTests, givenDebugFlagSetWhenTaskCountNotUpdatedAndGpuBusyThenTerminateDirectSubmission) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerIdleDetection.set(false);
    controller->unregisterDirectSubmission(csr.get());
    controller = std::make_unique<DirectSubmissionControllerMock>();
    controller->timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
    controller->registerDirectSubmission(csr.get());

    csr->taskCount.store(10u);
    controller->checkNewSubmissions();
    csr->setLatestFlushedTaskCount(10u);
    csr->isBusyReturnValue = true;

    controller->checkNewSubmissions();
    EXPECT_TRUE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(controller->directSubmissions[csr.get()].taskCount, 10u);
    EXPECT_EQ(1u, csr->stopDirectSubmissionCalledTimes);
    EXPECT_EQ(0u, csr->flushTagUpdateCalledTimes);
}

} // namespace NEO