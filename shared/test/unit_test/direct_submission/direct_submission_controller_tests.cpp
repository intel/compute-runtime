/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_thread.h"
#include "shared/source/os_interface/os_time.h"
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

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllertimeoutDivisorWhenCreateObjectThentimeoutDivisorIsEqualWithDebugFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerDivisor.set(4);

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeoutDivisor, 4);
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

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerAndDivisorDisabledWhenIncreaseTimeoutEnabledThenTimeoutIsIncreased) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerMaxTimeout.set(200'000);
    debugManager.flags.DirectSubmissionControllerDivisor.set(1);
    debugManager.flags.DirectSubmissionControllerAdjustOnThrottleAndAcLineStatus.set(0);
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

void fillTimeoutParamsMap(DirectSubmissionControllerMock &controller) {
    controller.timeoutParamsMap.clear();
    for (auto throttle : {QueueThrottle::LOW, QueueThrottle::MEDIUM, QueueThrottle::HIGH}) {
        for (auto acLineStatus : {false, true}) {
            auto key = controller.getTimeoutParamsMapKey(throttle, acLineStatus);
            TimeoutParams params{};
            if (throttle == QueueThrottle::LOW) {
                params.maxTimeout = std::chrono::microseconds{500u};
            } else {
                params.maxTimeout = std::chrono::microseconds{500u + static_cast<size_t>(throttle) * 500u + (acLineStatus ? 0u : 1500u)};
            }
            params.timeout = params.maxTimeout;
            params.timeoutDivisor = 1;
            params.directSubmissionEnabled = true;
            auto keyValue = std::make_pair(key, params);
            bool result = false;
            std::tie(std::ignore, result) = controller.timeoutParamsMap.insert(keyValue);
            EXPECT_TRUE(result);
        }
    }
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerAndAdjustOnThrottleAndAcLineStatusDisabledWhenSetTimeoutParamsForPlatformThenTimeoutParamsMapsIsEmpty) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerAdjustOnThrottleAndAcLineStatus.set(0);
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
    controller.setTimeoutParamsForPlatform(csr.getProductHelper());
    EXPECT_EQ(0u, controller.timeoutParamsMap.size());
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerAndAdjustOnThrottleAndAcLineStatusEnabledWhenThrottleOrAcLineStatusChangesThenTimeoutIsChanged) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerDivisor.set(1);
    debugManager.flags.DirectSubmissionControllerAdjustOnThrottleAndAcLineStatus.set(1);
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
    controller.setTimeoutParamsForPlatform(csr.getProductHelper());
    controller.registerDirectSubmission(&csr);
    EXPECT_TRUE(controller.adjustTimeoutOnThrottleAndAcLineStatus);
    fillTimeoutParamsMap(controller);

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::LOW;
        csr.getAcLineConnectedReturnValue = true;
        csr.taskCount.store(1u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 1u);

        EXPECT_EQ(500u, controller.timeout.count());
        EXPECT_EQ(500u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::MEDIUM;
        csr.getAcLineConnectedReturnValue = true;
        csr.taskCount.store(2u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 2u);

        EXPECT_EQ(1'000u, controller.timeout.count());
        EXPECT_EQ(1'000u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::HIGH;
        csr.getAcLineConnectedReturnValue = true;
        csr.taskCount.store(3u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 3u);

        EXPECT_EQ(1'500u, controller.timeout.count());
        EXPECT_EQ(1'500u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::LOW;
        csr.getAcLineConnectedReturnValue = false;
        csr.taskCount.store(4u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 4u);

        EXPECT_EQ(500u, controller.timeout.count());
        EXPECT_EQ(500u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::MEDIUM;
        csr.getAcLineConnectedReturnValue = false;
        csr.taskCount.store(5u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 5u);

        EXPECT_EQ(2'500u, controller.timeout.count());
        EXPECT_EQ(2'500u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::HIGH;
        csr.getLastDirectSubmissionThrottleReturnValue = QueueThrottle::HIGH;
        csr.getAcLineConnectedReturnValue = false;
        csr.taskCount.store(6u);
        controller.checkNewSubmissions();
        EXPECT_FALSE(controller.directSubmissions[&csr].isStopped);
        EXPECT_EQ(controller.directSubmissions[&csr].taskCount, 6u);

        EXPECT_EQ(3'000u, controller.timeout.count());
        EXPECT_EQ(3'000u, controller.maxTimeout.count());
        EXPECT_EQ(1, controller.timeoutDivisor);
    }

    {
        controller.lowestThrottleSubmitted = QueueThrottle::LOW;
        controller.checkNewSubmissions();
        EXPECT_EQ(QueueThrottle::HIGH, controller.lowestThrottleSubmitted);
    }

    controller.unregisterDirectSubmission(&csr);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterCsrsThenTimeoutIsNotAdjusted) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr3);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
}

TEST(DirectSubmissionControllerTests, givenPowerSavingUintWhenCallingGetThrottleFromPowerSavingUintThenCorrectValueIsReturned) {
    EXPECT_EQ(QueueThrottle::MEDIUM, getThrottleFromPowerSavingUint(0u));
    EXPECT_EQ(QueueThrottle::LOW, getThrottleFromPowerSavingUint(1u));
    EXPECT_EQ(QueueThrottle::LOW, getThrottleFromPowerSavingUint(100u));
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterCsrsFromDifferentSubdevicesThenTimeoutIsAdjusted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerDivisor.set(4);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    DeviceBitfield deviceBitfield1(0b10);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    MockCommandStreamReceiver csr5(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext5(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr5.setupContext(*osContext5.get());

    MockCommandStreamReceiver csr6(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext6(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr6.setupContext(*osContext6.get());

    MockCommandStreamReceiver csr7(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext7(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr7.setupContext(*osContext7.get());

    MockCommandStreamReceiver csr8(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext8(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr8.setupContext(*osContext8.get());

    MockCommandStreamReceiver csr9(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext9(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr9.setupContext(*osContext9.get());

    MockCommandStreamReceiver csr10(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext10(OsContext::create(nullptr, 0, 0,
                                                             EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                          PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr10.setupContext(*osContext10.get());

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr5);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout.count(), 1'250);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout.count(), 312);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout.count(), 312);

    controller.registerDirectSubmission(&csr6);
    EXPECT_EQ(controller.timeout.count(), 312);

    controller.registerDirectSubmission(&csr7);
    EXPECT_EQ(controller.timeout.count(), 312);

    controller.registerDirectSubmission(&csr9);
    EXPECT_EQ(controller.timeout.count(), 312);

    controller.registerDirectSubmission(&csr8);
    EXPECT_EQ(controller.timeout.count(), 78);

    controller.registerDirectSubmission(&csr10);
    EXPECT_EQ(controller.timeout.count(), 78);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerDirectSubmissionControllerDivisorSetWhenRegisterCsrsThenTimeoutIsAdjusted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionControllerDivisor.set(5);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr3);
    EXPECT_EQ(controller.timeout.count(), 5'000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout.count(), 1'000);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout.count(), 200);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout.count(), 200);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
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