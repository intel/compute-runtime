/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
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

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllertimeoutDivisorWhenCreateObjectThentimeoutDivisorIsEqualWithDebugFlag) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionControllerDivisor.set(4);

    DirectSubmissionControllerMock controller;

    EXPECT_EQ(controller.timeoutDivisor, 4);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterDirectSubmissionWorksThenItIsMonitoringItsState) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();

    DeviceBitfield deviceBitfield(1);
    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());
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
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());
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

    while (!controller.sleepCalled) {
    }
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWithNotStartedControllingWhenShuttingDownThenNoHang) {
    DirectSubmissionControllerMock controller;
    EXPECT_NE(controller.directSubmissionControllingThread.get(), nullptr);

    while (!controller.sleepCalled) {
    }
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterCsrsThenTimeoutIsNotAdjusted) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    DirectSubmissionControllerMock controller;
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();

    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr3);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout, 5000);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerWhenRegisterCsrsFromDifferentSubdevicesThenTimeoutIsAdjusted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionControllerDivisor.set(4);
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    DeviceBitfield deviceBitfield1(0b10);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    MockCommandStreamReceiver csr5(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext5(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr5.setupContext(*osContext5.get());

    MockCommandStreamReceiver csr6(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext6(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr6.setupContext(*osContext6.get());

    MockCommandStreamReceiver csr7(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext7(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr7.setupContext(*osContext7.get());

    MockCommandStreamReceiver csr8(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext8(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr8.setupContext(*osContext8.get());

    MockCommandStreamReceiver csr9(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext9(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr9.setupContext(*osContext9.get());

    MockCommandStreamReceiver csr10(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext10(OsContext::create(nullptr, 0,
                                                             EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                          PreemptionMode::ThreadGroup, deviceBitfield1)));
    csr10.setupContext(*osContext10.get());

    DirectSubmissionControllerMock controller;
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();

    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr5);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout, 1250);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout, 312);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout, 312);

    controller.registerDirectSubmission(&csr6);
    EXPECT_EQ(controller.timeout, 312);

    controller.registerDirectSubmission(&csr7);
    EXPECT_EQ(controller.timeout, 312);

    controller.registerDirectSubmission(&csr9);
    EXPECT_EQ(controller.timeout, 312);

    controller.registerDirectSubmission(&csr8);
    EXPECT_EQ(controller.timeout, 78);

    controller.registerDirectSubmission(&csr10);
    EXPECT_EQ(controller.timeout, 78);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
}

TEST(DirectSubmissionControllerTests, givenDirectSubmissionControllerDirectSubmissionControllerDivisorSetWhenRegisterCsrsThenTimeoutIsAdjusted) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionControllerDivisor.set(5);

    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);

    MockCommandStreamReceiver csr(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext(OsContext::create(nullptr, 0,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                        PreemptionMode::ThreadGroup, deviceBitfield)));
    csr.setupContext(*osContext.get());

    MockCommandStreamReceiver csr1(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext1(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr1.setupContext(*osContext1.get());

    MockCommandStreamReceiver csr2(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext2(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr2.setupContext(*osContext2.get());

    MockCommandStreamReceiver csr3(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext3(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr3.setupContext(*osContext3.get());

    MockCommandStreamReceiver csr4(executionEnvironment, 0, deviceBitfield);
    std::unique_ptr<OsContext> osContext4(OsContext::create(nullptr, 0,
                                                            EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular},
                                                                                                         PreemptionMode::ThreadGroup, deviceBitfield)));
    csr4.setupContext(*osContext4.get());

    DirectSubmissionControllerMock controller;
    controller.keepControlling.store(false);
    controller.directSubmissionControllingThread->join();
    controller.directSubmissionControllingThread.reset();

    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr3);
    EXPECT_EQ(controller.timeout, 5000);

    controller.registerDirectSubmission(&csr1);
    EXPECT_EQ(controller.timeout, 1000);

    controller.registerDirectSubmission(&csr2);
    EXPECT_EQ(controller.timeout, 200);

    controller.registerDirectSubmission(&csr4);
    EXPECT_EQ(controller.timeout, 200);

    controller.unregisterDirectSubmission(&csr);
    controller.unregisterDirectSubmission(&csr1);
    controller.unregisterDirectSubmission(&csr2);
    controller.unregisterDirectSubmission(&csr3);
    controller.unregisterDirectSubmission(&csr4);
}

} // namespace NEO