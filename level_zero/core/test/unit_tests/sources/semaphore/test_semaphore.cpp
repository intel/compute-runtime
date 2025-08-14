/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

using namespace NEO;
#include "gtest/gtest.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamilyImmediate;
} // namespace L0

namespace L0 {
namespace ult {

using ExternalSemaphoreTest = Test<DeviceFixture>;
HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorAndImportExternalSemaphoreExpIsCalledThenInvalidArgumentsIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    ze_result_t result = zeDeviceImportExternalSemaphoreExt(hDevice, &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(ExternalSemaphoreTest, givenValidParametersWhenReleaseExternalSemaphoreIsCalledThenSuccessIsReturned) {
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->releaseExternalSemaphore();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorWhenInitializeIsCalledThenInvalidArgumentIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_external_semaphore_ext_desc_t desc = {};
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->initialize(hDevice, &desc);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    delete externalSemaphoreImp;
}

HWTEST_F(ExternalSemaphoreTest, DISABLED_givenExternalSemaphoreGetInstanceCalledThenExternalSemaphoreControllerInstanceIsNotNull) {
    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    driverHandle->externalSemaphoreController->startThread();

    EXPECT_NE(driverHandle->externalSemaphoreController, nullptr);
}

HWTEST_F(ExternalSemaphoreTest, DISABLED_givenExternalSemaphoreControllerStartCalledMultipleTimesThenExternalSemaphoreControllerInstanceIsNotNull) {
    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

    driverHandle->externalSemaphoreController->startThread();
    driverHandle->externalSemaphoreController->startThread();

    EXPECT_NE(driverHandle->externalSemaphoreController, nullptr);
}

HWTEST_F(ExternalSemaphoreTest, givenRegularCommandListWhenAppendWaitExternalSemaphoresIsCalledThenInvalidArgumentIsReturned) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_external_semaphore_ext_handle_t hSemaphore = reinterpret_cast<ze_external_semaphore_ext_handle_t>(0x1);
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(ExternalSemaphoreTest, givenRegularCommandListWhenAppendSignalExternalSemaphoresIsCalledThenInvalidArgumentIsReturned) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    ze_external_semaphore_ext_handle_t hSemaphore = reinterpret_cast<ze_external_semaphore_ext_handle_t>(0x1);
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

using MockDriverHandleImp = Mock<L0::DriverHandleImp>;

HWTEST_F(ExternalSemaphoreTest, givenImmediateCommandListWhenAppendSignalExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 1u;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendWaitOnEventFailsWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());

    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 1u;
    ze_event_handle_t event;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 1, &event);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalInternalProxyEventFailsWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingSignalEvent = true;

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 1u;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalEventFailsWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failOnSecondSignalEvent = true;

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 1u;
    ze_event_handle_t waitEvent = {};
    MockEvent signalEvent;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_event_handle_t hSignalEvent = signalEvent.toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, hSignalEvent, 1, &waitEvent);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 2u);
}

HWTEST_F(ExternalSemaphoreTest, givenFailingMemoryManagerWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    DebugManagerStateRestore restorer;

    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto failMemoryManager = std::make_unique<FailMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);

    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(failMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(0);

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenImmediateCommandListWhenAppendWaitExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    ze_external_semaphore_wait_params_ext_t waitParams = {};

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendWaitOnEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_event_handle_t event;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 1, &event);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendWaitOnInternalProxyEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_external_semaphore_wait_params_ext_t waitParams = {};

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingSignalEvent = true;

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_event_handle_t waitEvent = {};
    MockEvent signalEvent;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_event_handle_t hSignalEvent = signalEvent.toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, hSignalEvent, 1, &waitEvent);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 2u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenFailingMemoryManagerWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    DebugManagerStateRestore restorer;

    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto failMemoryManager = std::make_unique<FailMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    driverHandleImp->setMemoryManager(failMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    NEO::debugManager.flags.EnableTimestampPoolAllocator.set(0);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 0u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreControllerWhenAllocateProxyEventMultipleIsCalledMultipleTimesThenSuccessIsReturned) {
    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};

    ze_result_t result = driverHandleImp->externalSemaphoreController->allocateProxyEvent(l0Device->toHandle(), context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxyEvent1), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore1->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Wait));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 1u);

    result = driverHandleImp->externalSemaphoreController->allocateProxyEvent(l0Device->toHandle(), context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxyEvent2), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore2->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Wait));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 2u);
}

HWTEST_F(ExternalSemaphoreTest, givenMaxEventsInPoolCreatedWhenAllocateProxyEventCalledThenEventPoolSizeIncreases) {
    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->eventPoolsMap[l0Device->toHandle()].size(), 0u);

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};

    ze_result_t result = driverHandleImp->externalSemaphoreController->allocateProxyEvent(l0Device->toHandle(), context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxyEvent1), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore1->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Wait));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 1u);
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->eventPoolsMap[l0Device->toHandle()].size(), 1u);

    driverHandleImp->externalSemaphoreController->eventsCreatedFromLatestPoolMap[l0Device->toHandle()] = 20;
    result = driverHandleImp->externalSemaphoreController->allocateProxyEvent(l0Device->toHandle(), context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxyEvent2), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore2->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Wait));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 2u);
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->eventPoolsMap[l0Device->toHandle()].size(), 2u);
}

} // namespace ult
} // namespace L0
