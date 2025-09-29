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
#include "shared/test/common/helpers/default_hw_info.h"
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

TEST_F(DirectSubmissionIdleDetectionTests, givenSimulatedCsrLockContentionWhenCheckNewSubmissionsCalledThenTryLockSkipsCsr) {
    // Setup: Make CSR appear idle (should normally be stopped)
    csr->taskCount.store(10u);
    csr->setLatestFlushedTaskCount(10u);
    csr->isBusyReturnValue = false;

    // First, verify normal operation works
    controller->checkNewSubmissions();
    EXPECT_TRUE(controller->directSubmissions[csr.get()].isStopped);
    EXPECT_EQ(1u, csr->stopDirectSubmissionCalledTimes);

    // Reset for contention simulation
    controller->directSubmissions[csr.get()].isStopped = false;
    csr->stopDirectSubmissionCalledTimes = 0;

    // Create a contention-simulating CSR that overrides tryObtainUniqueOwnership
    struct ContentionSimulatingCsr : public TagUpdateMockCommandStreamReceiver {
        using TagUpdateMockCommandStreamReceiver::TagUpdateMockCommandStreamReceiver;

        bool simulateContention = false;

        std::unique_lock<CommandStreamReceiver::MutexType> tryObtainUniqueOwnership() override {
            if (simulateContention) {
                return std::unique_lock<CommandStreamReceiver::MutexType>();
            }
            return TagUpdateMockCommandStreamReceiver::tryObtainUniqueOwnership();
        }
    };

    // Replace the existing CSR with our contention-simulating one
    controller->unregisterDirectSubmission(csr.get());

    DeviceBitfield deviceBitfield(1);
    auto contentionCsr = std::make_unique<ContentionSimulatingCsr>(executionEnvironment, 0, deviceBitfield);
    auto newOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 1,
                                                                     EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                                  PreemptionMode::ThreadGroup, deviceBitfield)));
    contentionCsr->setupContext(*newOsContext);

    contentionCsr->taskCount.store(10u);
    contentionCsr->setLatestFlushedTaskCount(10u);
    contentionCsr->isBusyReturnValue = false;

    controller->registerDirectSubmission(contentionCsr.get());
    controller->checkNewSubmissions();
    controller->directSubmissions[contentionCsr.get()].isStopped = false;
    contentionCsr->stopDirectSubmissionCalledTimes = 0;

    // Enable contention simulation - this will make tryObtainUniqueOwnership() fail
    contentionCsr->simulateContention = true;

    controller->checkNewSubmissions();

    // Verify CSR was NOT stopped due to simulated contended lock (conservative behavior)
    EXPECT_FALSE(controller->directSubmissions[contentionCsr.get()].isStopped);
    EXPECT_EQ(0u, contentionCsr->stopDirectSubmissionCalledTimes);

    // Disable contention simulation and verify normal operation resumes
    contentionCsr->simulateContention = false;

    controller->checkNewSubmissions();

    // Now it should be stopped since no contention exists
    EXPECT_TRUE(controller->directSubmissions[contentionCsr.get()].isStopped);
    EXPECT_EQ(1u, contentionCsr->stopDirectSubmissionCalledTimes);

    controller->unregisterDirectSubmission(contentionCsr.get());
}

TEST_F(DirectSubmissionIdleDetectionTests, givenTryLockContentionThenIsCopyEngineOnDeviceIdleReturnsFalse) {
    struct ContentionSimulatingCsr : public TagUpdateMockCommandStreamReceiver {
        using TagUpdateMockCommandStreamReceiver::TagUpdateMockCommandStreamReceiver;

        bool simulateContention = false;

        std::unique_lock<CommandStreamReceiver::MutexType> tryObtainUniqueOwnership() override {
            if (simulateContention) {
                return std::unique_lock<CommandStreamReceiver::MutexType>();
            }
            return TagUpdateMockCommandStreamReceiver::tryObtainUniqueOwnership();
        }
    };

    // Remove original csr from controller
    controller->unregisterDirectSubmission(csr.get());

    DeviceBitfield deviceBitfield(1);
    auto contCsr = std::make_unique<ContentionSimulatingCsr>(executionEnvironment, 0u, deviceBitfield);
    auto osCtx = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 1,
                                                              EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                           PreemptionMode::ThreadGroup, deviceBitfield)));
    contCsr->setupContext(*osCtx);

    contCsr->taskCount.store(50u);
    contCsr->setLatestFlushedTaskCount(50u);
    contCsr->isBusyReturnValue = false;

    controller->registerDirectSubmission(contCsr.get());
    controller->directSubmissions[contCsr.get()].taskCount = 50u;
    controller->directSubmissions[contCsr.get()].isStopped = false;

    std::optional<TaskCountType> bcsTaskCount(50u);

    // No contention -> true
    EXPECT_TRUE(controller->isCopyEngineOnDeviceIdle(0u, bcsTaskCount));

    // Contention -> false
    contCsr->simulateContention = true;
    EXPECT_FALSE(controller->isCopyEngineOnDeviceIdle(0u, bcsTaskCount));

    // Remove contention -> true again
    contCsr->simulateContention = false;
    EXPECT_TRUE(controller->isCopyEngineOnDeviceIdle(0u, bcsTaskCount));

    controller->unregisterDirectSubmission(contCsr.get());
}

struct DirectSubmissionCheckForCopyEngineIdleTests : public ::testing::Test {
    void SetUp() override {
        controller = std::make_unique<DirectSubmissionControllerMock>();
        executionEnvironment.prepareRootDeviceEnvironments(2);
        executionEnvironment.initializeMemoryManager();
        executionEnvironment.rootDeviceEnvironments[0]->initOsTime();
        executionEnvironment.rootDeviceEnvironments[1]->initOsTime();

        DeviceBitfield deviceBitfield(1);
        ccsCsr = std::make_unique<TagUpdateMockCommandStreamReceiver>(executionEnvironment, 0, deviceBitfield);
        bcsCsr = std::make_unique<TagUpdateMockCommandStreamReceiver>(executionEnvironment, 0, deviceBitfield);
        ccsOsContext.reset(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield)));
        bcsOsContext.reset(OsContext::create(nullptr, 0, 1, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield)));
        ccsCsr->setupContext(*ccsOsContext);
        bcsCsr->setupContext(*bcsOsContext);

        controller->timeoutElapsedReturnValue.store(TimeoutElapsedMode::fullyElapsed);
        controller->registerDirectSubmission(ccsCsr.get());
        controller->registerDirectSubmission(bcsCsr.get());
        bcsCsr->taskCount.store(10u);
        ccsCsr->taskCount.store(10u);
        controller->checkNewSubmissions();
    }

    void TearDown() override {
        controller->unregisterDirectSubmission(ccsCsr.get());
        controller->unregisterDirectSubmission(bcsCsr.get());
    }

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), true, 2u};

    std::unique_ptr<OsContext> osContext;
    std::unique_ptr<TagUpdateMockCommandStreamReceiver> ccsCsr;
    std::unique_ptr<OsContext> ccsOsContext;

    std::unique_ptr<TagUpdateMockCommandStreamReceiver> bcsCsr;
    std::unique_ptr<OsContext> bcsOsContext;
    std::unique_ptr<DirectSubmissionControllerMock> controller;
};

TEST_F(DirectSubmissionCheckForCopyEngineIdleTests, givenCheckBcsForDirectSubmissionStopWhenCCSIdleAndCopyEngineBusyThenDontTerminateDirectSubmission) {
    ccsCsr->setLatestFlushedTaskCount(10u);
    bcsCsr->setLatestFlushedTaskCount(10u);

    ccsCsr->isBusyReturnValue = false;
    bcsCsr->isBusyReturnValue = true;
    controller->directSubmissions[bcsCsr.get()].isStopped = false;
    controller->checkNewSubmissions();
    EXPECT_EQ(controller->directSubmissions[ccsCsr.get()].taskCount, 10u);

    if (ccsCsr->getProductHelper().checkBcsForDirectSubmissionStop()) {
        EXPECT_FALSE(controller->directSubmissions[ccsCsr.get()].isStopped);
        EXPECT_EQ(0u, ccsCsr->stopDirectSubmissionCalledTimes);
    } else {
        EXPECT_TRUE(controller->directSubmissions[ccsCsr.get()].isStopped);
        EXPECT_EQ(1u, ccsCsr->stopDirectSubmissionCalledTimes);
    }
}

TEST_F(DirectSubmissionCheckForCopyEngineIdleTests, givenCheckBcsForDirectSubmissionStopWhenCCSIdleAndCopyEngineUpdatedTaskCountThenDontTerminateDirectSubmission) {
    ccsCsr->setLatestFlushedTaskCount(10u);
    bcsCsr->setLatestFlushedTaskCount(10u);

    ccsCsr->isBusyReturnValue = false;
    bcsCsr->isBusyReturnValue = false;
    controller->directSubmissions[bcsCsr.get()].isStopped = false;
    bcsCsr->taskCount.store(20u);

    controller->checkNewSubmissions();
    EXPECT_EQ(controller->directSubmissions[ccsCsr.get()].taskCount, 10u);

    if (ccsCsr->getProductHelper().checkBcsForDirectSubmissionStop()) {
        EXPECT_FALSE(controller->directSubmissions[ccsCsr.get()].isStopped);
        EXPECT_EQ(0u, ccsCsr->stopDirectSubmissionCalledTimes);
    } else {
        EXPECT_TRUE(controller->directSubmissions[ccsCsr.get()].isStopped);
        EXPECT_EQ(1u, ccsCsr->stopDirectSubmissionCalledTimes);
    }
}

TEST_F(DirectSubmissionCheckForCopyEngineIdleTests, givenCheckBcsForDirectSubmissionStopWhenCCSIdleAndCopyEngineBusyAndDifferentDeviceThenTerminateDirectSubmission) {
    DeviceBitfield deviceBitfield(1);
    TagUpdateMockCommandStreamReceiver secondDeviceCsr(executionEnvironment, 1, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 1, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield)));
    secondDeviceCsr.setupContext(*osContext);
    controller->registerDirectSubmission(&secondDeviceCsr);
    secondDeviceCsr.taskCount.store(10u);
    controller->checkNewSubmissions();

    secondDeviceCsr.setLatestFlushedTaskCount(10u);
    bcsCsr->setLatestFlushedTaskCount(10u);

    secondDeviceCsr.isBusyReturnValue = false;
    bcsCsr->isBusyReturnValue = true;
    controller->directSubmissions[bcsCsr.get()].isStopped = false;
    controller->checkNewSubmissions();
    EXPECT_EQ(controller->directSubmissions[&secondDeviceCsr].taskCount, 10u);
    EXPECT_TRUE(controller->directSubmissions[&secondDeviceCsr].isStopped);
    EXPECT_EQ(1u, secondDeviceCsr.stopDirectSubmissionCalledTimes);
}

TEST_F(DirectSubmissionCheckForCopyEngineIdleTests, givenCheckBcsForDirectSubmissionStopWhenCopyEngineNotStartedThenTerminateDirectSubmission) {
    ccsCsr->setLatestFlushedTaskCount(10u);
    bcsCsr->setLatestFlushedTaskCount(10u);

    ccsCsr->isBusyReturnValue = false;
    bcsCsr->isBusyReturnValue = true;
    controller->directSubmissions[bcsCsr.get()].isStopped = true;

    controller->checkNewSubmissions();
    EXPECT_EQ(controller->directSubmissions[ccsCsr.get()].taskCount, 10u);
    EXPECT_TRUE(controller->directSubmissions[ccsCsr.get()].isStopped);
    EXPECT_EQ(1u, ccsCsr->stopDirectSubmissionCalledTimes);
}

TEST(CommandStreamReceiverGetContextGroupIdTests, givenContextGroupWithPrimaryContextWhenGetContextGroupIdIsCalledThenReturnsPrimaryContextId) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    // Create a primary OsContext and mark it as part of a group
    auto primaryContext = std::make_unique<OsContext>(0, 42, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield));
    primaryContext->setContextGroupCount(8);

    // Create a secondary OsContext and set its primary context
    auto secondaryContext = std::make_unique<OsContext>(0, 99, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield));
    secondaryContext->setPrimaryContext(primaryContext.get());

    // Create CSR and set up with secondary context
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.setupContext(*secondaryContext);

    // Should return the primary context's id (42)
    EXPECT_EQ(42u, csr.getContextGroupId());
}

TEST(CommandStreamReceiverGetContextGroupIdTests, givenContextGroupWithoutPrimaryContextWhenGetContextGroupIdIsCalledThenReturnsOwnContextId) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    // Create an OsContext that is part of a group but has no primary context
    auto context = std::make_unique<OsContext>(0, 55, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, deviceBitfield));
    context->setContextGroupCount(8);
    // Do NOT set primary context

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    csr.setupContext(*context);

    // Should return its own context id (55)
    EXPECT_EQ(55u, csr.getContextGroupId());
}

TEST(DirectSubmissionIdleDetectionWithContextGroupTest, givenDefaultConstructorWhenCreatingControllerThenContextGroupIdleDetectionIsEnabledByDefault) {
    DirectSubmissionControllerMock controller;

    EXPECT_TRUE(controller.isCsrsContextGroupIdleDetectionEnabled);
}

TEST(DirectSubmissionIdleDetectionWithContextGroupTest, givenDirectSubmissionControllerContextGroupIdleDetectionSetWhenCreatingControllerThenContextGroupIdleDetectionIsSetCorrectly) {
    DebugManagerStateRestore restorer;

    for (auto contextGroupIdleDetectionState : {-1, 0, 1}) {
        debugManager.flags.DirectSubmissionControllerContextGroupIdleDetection.set(contextGroupIdleDetectionState);

        DirectSubmissionControllerMock controller;
        if (0 == contextGroupIdleDetectionState) {
            EXPECT_FALSE(controller.isCsrsContextGroupIdleDetectionEnabled);
        } else {
            EXPECT_TRUE(controller.isCsrsContextGroupIdleDetectionEnabled);
        }
    }
}

class MockContextGroupIdleDetectionCsr : public MockCommandStreamReceiver {
  public:
    using MockCommandStreamReceiver::MockCommandStreamReceiver;

    // Context group id mock
    uint32_t mockContextGroupId = 0;
    void setContextGroupId(uint32_t id) { mockContextGroupId = id; }
    uint32_t getContextGroupId() const override { return mockContextGroupId; }

    // Busy state mock
    bool busy = false;
    void setBusy(bool value) { busy = value; }
    bool isBusyWithoutHang(TimeType &) override { return busy; }

    SubmissionStatus flushTagUpdate() override {
        latestFlushedTaskCount.store(taskCount); // Simulate hardware progress
        return SubmissionStatus::success;
    }

    // Mutex mock
    std::recursive_mutex mockMutex;
    std::recursive_mutex &getMutex() { return mockMutex; }
};

class DirectSubmissionIdleDetectionWithContextGroupTests : public ::testing::Test {
  protected:
    void SetUp() override {
        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.initializeMemoryManager();
        executionEnvironment.rootDeviceEnvironments[0]->osTime.reset(new MockOSTime{});

        controller = std::make_unique<DirectSubmissionControllerMock>();
        ASSERT_TRUE(controller->isCsrsContextGroupIdleDetectionEnabled);
    }

    void TearDown() override {
        // Unregister all CSRs that were registered in the test
        for (auto *csr : registeredCsrs) {
            controller->unregisterDirectSubmission(csr);
        }
        registeredCsrs.clear();
    }

    // Helper to create and register a CSR with a given context group id and busy state
    std::unique_ptr<MockContextGroupIdleDetectionCsr> createAndRegisterCsr(uint32_t contextGroupId, bool busy) {
        auto csr = std::make_unique<MockContextGroupIdleDetectionCsr>(executionEnvironment, 0, DeviceBitfield(1));
        auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, static_cast<uint32_t>(registeredCsrs.size()), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, DeviceBitfield(1))));
        osContext->setContextGroupCount(8);
        csr->setupContext(*osContext);
        csr->setContextGroupId(contextGroupId);
        csr->setBusy(busy);
        controller->registerDirectSubmission(csr.get());
        registeredCsrs.push_back(csr.get());
        osContexts.push_back(std::move(osContext));
        return csr;
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DirectSubmissionControllerMock> controller;
    std::vector<MockContextGroupIdleDetectionCsr *> registeredCsrs;
    std::vector<std::unique_ptr<OsContext>> osContexts;
    DebugManagerStateRestore restorer;
};

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenSingleCsrIsNotBusyThenIsDirectSubmissionIdleReturnsTrue) {
    auto csr = createAndRegisterCsr(123, false);
    csr->latestFlushedTaskCount = 1;
    csr->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenSingleCsrIsBusyThenIsDirectSubmissionIdleReturnsFalse) {
    auto csr = createAndRegisterCsr(123, true);
    csr->latestFlushedTaskCount = 1;
    csr->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr->getMutex());
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenAllCsrsInContextGroupAreNotBusyThenIsDirectSubmissionIdleReturnsTrue) {
    // Simulate same context group
    auto csr1 = createAndRegisterCsr(42, false);
    auto csr2 = createAndRegisterCsr(42, false);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenAnyCsrInContextGroupIsBusyThenIsDirectSubmissionIdleReturnsFalse) {
    // Simulate same context group
    auto csr1 = createAndRegisterCsr(42, false);
    auto csr2 = createAndRegisterCsr(42, true);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenAllCsrsInContextGroupAreBusyThenIsDirectSubmissionIdleReturnsFalse) {
    // Simulate same context group
    auto csr1 = createAndRegisterCsr(77, true);
    auto csr2 = createAndRegisterCsr(77, true);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenCsrsAreInDifferentContextGroupsThenIdleDetectionIsIndependentPerGroup) {
    // Different context groups
    auto csr1 = createAndRegisterCsr(77, false);
    auto csr2 = createAndRegisterCsr(88, true);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenCsrInContextGroupIsUnregisteredThenItIsIgnored) {
    // Create and register only one CSR in the group
    auto csr1 = createAndRegisterCsr(88, false);
    // Create a second CSR with the same group, but do not register it
    auto csr2 = std::make_unique<MockContextGroupIdleDetectionCsr>(executionEnvironment, 0, DeviceBitfield(1));
    csr2->setContextGroupId(88);
    csr2->setBusy(true);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenCsrsWithDifferentTaskCountsInContextGroupThenIsDirectSubmissionIdleReturnsExpectedValue) {
    // Simulate same context group
    auto csr1 = createAndRegisterCsr(124, false);
    auto csr2 = createAndRegisterCsr(124, false);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 2; // Not equal
    csr1->setBusy(false);

    csr2->latestFlushedTaskCount = 2;
    csr2->taskCount = 2;
    csr2->setBusy(false);

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock));

    csr1->setBusy(true);
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenCsrInContextGroupHasPendingTaskCountButAllAreNotBusyAfterFlushThenIsDirectSubmissionIdleReturnsTrue) {
    // Simulate same context group
    auto csr1 = createAndRegisterCsr(555, false);
    auto csr2 = createAndRegisterCsr(555, false); // Not busy, only task count matters

    // csr2 has pending work (taskCount > latestFlushedTaskCount), but is not busy
    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 2;

    // After flushTagUpdate and polling, both CSRs are not busy, so the group is considered idle
    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenOtherCsrInGroupIsBusyThenUnlockIsCalledAndReturnsFalse) {
    // Two CSRs in the same group, one is busy, one is not
    auto csr1 = createAndRegisterCsr(123, false);
    auto csr2 = createAndRegisterCsr(123, true); // This one will be busy

    csr1->latestFlushedTaskCount = 5;
    csr1->taskCount = 5;
    csr2->latestFlushedTaskCount = 7;
    csr2->taskCount = 7;

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    // csr2 is busy, so isDirectSubmissionIdle should return false and unlock should be called for csr2
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenSelfIsBusyThenUnlockIsNotCalledAndReturnsFalse) {
    // Single CSR in the group, it is busy
    auto csr = createAndRegisterCsr(123, true);

    csr->latestFlushedTaskCount = 5;
    csr->taskCount = 5;

    std::unique_lock<std::recursive_mutex> lock(csr->getMutex());
    // Only one CSR, so groupCsr == csr, unlock should NOT be called
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenOtherCsrInGroupHasPendingTaskAndRemainsBusyAfterPollingThenUnlockIsCalledAndReturnsFalse) {
    // Create two CSRs in the same group
    auto csr1 = createAndRegisterCsr(123, false);
    auto csr2 = createAndRegisterCsr(123, false);

    // Set up csr1 as the main CSR, csr2 as the "other" in the group
    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;

    // csr2 has pending work (latestFlushedTaskCount != taskCount)
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 2;

    // Simulate that after polling, csr2 is still busy
    csr2->setBusy(false); // Not busy before polling
    // We'll override isBusyWithoutHang to return true after polling
    struct BusyAfterPollingCsr : public MockContextGroupIdleDetectionCsr {
        using MockContextGroupIdleDetectionCsr::MockContextGroupIdleDetectionCsr;
        int callCount = 0;
        bool isBusyWithoutHang(TimeType &) override {
            callCount++;
            // First call (before polling): return false (simulate not busy)
            // Second call (after polling): return true (simulate still busy)
            return callCount >= 2;
        }
    };
    auto busyAfterPollingCsr = std::make_unique<BusyAfterPollingCsr>(executionEnvironment, 0, DeviceBitfield(1));
    busyAfterPollingCsr->setContextGroupId(123);
    busyAfterPollingCsr->latestFlushedTaskCount = 1;
    busyAfterPollingCsr->taskCount = 2;
    controller->registerDirectSubmission(busyAfterPollingCsr.get());
    registeredCsrs.push_back(busyAfterPollingCsr.get());

    std::unique_lock<std::recursive_mutex> lock(csr1->getMutex());
    // Should return false because busyAfterPollingCsr is still busy after polling
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock));
}

TEST_F(DirectSubmissionIdleDetectionWithContextGroupTests, whenContextGroupIdleDetectionDisabledThenOnlySelfIsChecked) {
    controller->isCsrsContextGroupIdleDetectionEnabled = false;

    // Create two CSRs in the same group
    auto csr1 = createAndRegisterCsr(99, false);
    auto csr2 = createAndRegisterCsr(99, true);

    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;
    csr2->latestFlushedTaskCount = 1;
    csr2->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock1(csr1->getMutex());
    std::unique_lock<std::recursive_mutex> lock2(csr2->getMutex());

    // When group idle detection is disabled, only the queried CSR's state matters
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock1));  // csr1 is not busy
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr2.get(), lock2)); // csr2 is busy

    // Now make csr1 busy and csr2 idle, and check again
    csr1->setBusy(true);
    csr2->setBusy(false);

    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock1)); // csr1 is busy
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr2.get(), lock2));  // csr2 is not busy
}

class DirectSubmissionContextGroupCompositeKeyTests : public ::testing::Test {
  protected:
    void SetUp() override {
        executionEnvironment.prepareRootDeviceEnvironments(2);
        executionEnvironment.initializeMemoryManager();
        executionEnvironment.rootDeviceEnvironments[0]->osTime.reset(new MockOSTime{});
        executionEnvironment.rootDeviceEnvironments[1]->osTime.reset(new MockOSTime{});
        controller = std::make_unique<DirectSubmissionControllerMock>();
        ASSERT_TRUE(controller->isCsrsContextGroupIdleDetectionEnabled);
    }

    void TearDown() override {
        // Unregister all CSRs that were registered in the test
        for (auto *csr : registeredCsrs) {
            controller->unregisterDirectSubmission(csr);
        }
        registeredCsrs.clear();
    }

    // Helper to create and register a CSR with a given rootDeviceIndex and contextGroupId
    std::unique_ptr<MockContextGroupIdleDetectionCsr> createAndRegisterCsr(uint32_t rootDeviceIndex, uint32_t contextGroupId) {
        auto csr = std::make_unique<MockContextGroupIdleDetectionCsr>(executionEnvironment, rootDeviceIndex, DeviceBitfield(1));
        auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, rootDeviceIndex, static_cast<uint32_t>(registeredCsrs.size()), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, PreemptionMode::ThreadGroup, DeviceBitfield(1))));
        osContext->setContextGroupCount(8);
        csr->setupContext(*osContext);
        csr->setContextGroupId(contextGroupId);
        controller->registerDirectSubmission(csr.get());
        registeredCsrs.push_back(csr.get());
        osContexts.push_back(std::move(osContext));
        return csr;
    }

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), true, 2u};
    std::unique_ptr<DirectSubmissionControllerMock> controller;
    std::vector<MockContextGroupIdleDetectionCsr *> registeredCsrs;
    std::vector<std::unique_ptr<OsContext>> osContexts;
    DebugManagerStateRestore restorer;
};

TEST_F(DirectSubmissionContextGroupCompositeKeyTests, givenCsrsWithSameContextGroupIdButDifferentRootDeviceIndexWhenCheckingDirectSubmissionIdleThenCsrsAreNotGrouped) {
    // Create two CSRs with the same contextGroupId but different rootDeviceIndex
    auto csr0 = createAndRegisterCsr(0, 42);
    auto csr1 = createAndRegisterCsr(1, 42);

    csr0->latestFlushedTaskCount = 1;
    csr0->taskCount = 1;
    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;

    // Only csr0 should be in its group, and only csr1 in its own group
    std::unique_lock<std::recursive_mutex> lock0(csr0->getMutex());
    std::unique_lock<std::recursive_mutex> lock1(csr1->getMutex());

    // Make csr1 busy, csr0 idle
    csr0->setBusy(false);
    csr1->setBusy(true);

    // csr0's group should be idle (csr1's busy state should not affect it)
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr0.get(), lock0));
    // csr1's group should not be idle (csr1 is busy)
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock1));
}

TEST_F(DirectSubmissionContextGroupCompositeKeyTests, givenCsrsWithSameContextGroupIdAndRootDeviceIndexWhenCheckingDirectSubmissionIdleThenCsrsAreGrouped) {
    // Create two CSRs with the same rootDeviceIndex and contextGroupId
    auto csr0 = createAndRegisterCsr(0, 77);
    auto csr1 = createAndRegisterCsr(0, 77);

    csr0->latestFlushedTaskCount = 1;
    csr0->taskCount = 1;
    csr1->latestFlushedTaskCount = 1;
    csr1->taskCount = 1;

    std::unique_lock<std::recursive_mutex> lock0(csr0->getMutex());
    std::unique_lock<std::recursive_mutex> lock1(csr1->getMutex());

    // Both not busy: group is idle
    csr0->setBusy(false);
    csr1->setBusy(false);
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr0.get(), lock0));
    EXPECT_TRUE(controller->isDirectSubmissionIdle(csr1.get(), lock1));

    // One busy: group is not idle
    csr1->setBusy(true);
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr0.get(), lock0));
    EXPECT_FALSE(controller->isDirectSubmissionIdle(csr1.get(), lock1));
}
} // namespace NEO
