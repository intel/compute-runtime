/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
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

class MockNeoExtSemaphore : public NEO::ExternalSemaphore {
  public:
    MockNeoExtSemaphore() : NEO::ExternalSemaphore() {}
    using NEO::ExternalSemaphore::state;

    bool enqueueWait(uint64_t *fenceValue) override {
        this->state = NEO::ExternalSemaphore::SemaphoreState::Signaled;
        return true;
    }

    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override {
        return true;
    }

    bool enqueueSignal(uint64_t *fenceValue) override {
        this->state = NEO::ExternalSemaphore::SemaphoreState::Signaled;
        return true;
    }
};

class MockExtEvent : public MockEvent {
  public:
    MockExtEvent() : MockEvent() {}

    ze_result_t hostSignal(bool allowCounterBased) override {
        hostSignalCalledCnt++;

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t hostSynchronize(uint64_t timeout) override {
        hostSyncCalledCnt++;

        if (failHostSynchronize) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    bool failHostSynchronize = false;
    uint32_t hostSignalCalledCnt = 0;
    uint32_t hostSyncCalledCnt = 0;
};

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

using MockDriverHandle = Mock<L0::DriverHandle>;

HWTEST_F(ExternalSemaphoreTest, givenImmediateCommandListWhenAppendSignalExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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
    EXPECT_EQ(cmdList.appendSignalEventPostWalkerCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenInOrderImmediateCommandListWhenAppendSignalExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.enableInOrderExecution();

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    signalParams.value = 1u;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(cmdList.appendSignalInOrderDependencyCounterCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenInOrderImmediateCommandListWhenAppendWaitForExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.enableInOrderExecution();

    ze_external_semaphore_wait_params_ext_t waitParams = {};

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(cmdList.appendSignalInOrderDependencyCounterCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalEventFailsWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsNotReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenFailingMemoryManagerWhenAppendSignalExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    DebugManagerStateRestore restorer;

    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto failMemoryManager = std::make_unique<FailMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);

    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(failMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_event_handle_t event = nullptr;

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 1, &event);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsNotReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 2u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenFailingMemoryManagerWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    DebugManagerStateRestore restorer;

    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto failMemoryManager = std::make_unique<FailMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();

    driverHandle->setMemoryManager(failMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();

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

constexpr auto extSemaphoreWait = ExternalSemaphoreController::SemaphoreOperation::Wait;
constexpr auto extSemaphoreSignal = ExternalSemaphoreController::SemaphoreOperation::Signal;

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreControllerWhenAllocateProxyEventIsCalledMultipleTimesThenSuccessIsReturned) {
    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    ExternalSemaphoreController *semControl = driverHandle->externalSemaphoreController.get();

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};
    ze_device_handle_t hDevice = l0Device->toHandle();

    ze_result_t result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    semControl->proxyEvents.emplace_back(Event::fromHandle(proxyEvent1), externalSemaphore1->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    semControl->proxyEvents.emplace_back(Event::fromHandle(proxyEvent2), externalSemaphore2->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);
}

HWTEST_F(ExternalSemaphoreTest, givenMaxEventsInPoolCreatedWhenAllocateProxyEventCalledThenEventPoolSizeIncreases) {
    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_device_handle_t hDevice = l0Device->toHandle();

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    ExternalSemaphoreController *semControl = driverHandle->externalSemaphoreController.get();
    EXPECT_EQ(semControl->eventPoolsMap[hDevice].size(), 0u);

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};

    ze_result_t result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    semControl->proxyEvents.emplace_back(Event::fromHandle(proxyEvent1), externalSemaphore1->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    EXPECT_EQ(semControl->eventPoolsMap[hDevice].size(), 1u);

    semControl->eventsCreatedFromLatestPoolMap[hDevice] = 20; // Set current event pool as full.
    result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    semControl->proxyEvents.emplace_back(Event::fromHandle(proxyEvent2), externalSemaphore2->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);
    EXPECT_EQ(semControl->eventPoolsMap[hDevice].size(), 2u);
}

HWTEST_F(ExternalSemaphoreTest, givenInitialSemaphoreWaitEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto extSemaphore = std::make_unique<ExternalSemaphoreImp>();
    extSemaphore->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(extEvent->toBase(), extSemaphore->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    semControl->processProxyEvents();

    auto neoSemaphore = extSemaphore->neoExternalSemaphore.get();
    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extEvent->hostSignalCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->processedProxyEvents.size(), 1u);
    semControl->processedProxyEvents.clear(); // Manually clear processed events array since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenHostSignaledSemaphoreWaitEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto extSemaphore = std::make_unique<ExternalSemaphoreImp>();
    extSemaphore->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(extEvent->toBase(), extSemaphore->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    auto neoSemaphore = extSemaphore->neoExternalSemaphore.get();
    uint64_t fence = 0u;

    neoSemaphore->enqueueWait(&fence);
    semControl->processProxyEvents();

    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extEvent->hostSignalCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->processedProxyEvents.size(), 1u);
    semControl->processedProxyEvents.clear(); // Manually clear processed events array since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenHostSignaledSemaphoreSignalEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto extSemaphore = std::make_unique<ExternalSemaphoreImp>();
    extSemaphore->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(extEvent->toBase(), extSemaphore->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    auto neoSemaphore = extSemaphore->neoExternalSemaphore.get();
    uint64_t fence = 0u;

    neoSemaphore->enqueueWait(&fence);
    semControl->processProxyEvents();

    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extEvent->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->processedProxyEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenHostSynchronizeFailsForSemaphoreSignalEventOperationDuringProcessProxyEventsThenExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto extSemaphore = std::make_unique<ExternalSemaphoreImp>();
    extSemaphore->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    extEvent->failHostSynchronize = true;

    semControl->proxyEvents.emplace_back(extEvent->toBase(), extSemaphore->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    semControl->processProxyEvents();

    auto neoSemaphore = extSemaphore->neoExternalSemaphore.get();
    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(extEvent->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u); // Event should remain in proxy events array since host synchronize failed.
    EXPECT_EQ(semControl->processedProxyEvents.size(), 0u);
    semControl->proxyEvents.clear(); // Manually clear proxy events array since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitAndSignalEventsThenProcessProxyEventsSucceedsOnFirstCallAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto semWait = std::make_unique<ExternalSemaphoreImp>();
    auto semSignal = std::make_unique<ExternalSemaphoreImp>();
    semSignal->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    semWait->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(extEvent->toBase(), semWait->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    semControl->proxyEvents.emplace_back(extEvent->toBase(), semSignal->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);

    semControl->processProxyEvents();

    auto neoSemaphore = semWait->neoExternalSemaphore.get();

    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extEvent->hostSignalCalledCnt, 1u);
    EXPECT_EQ(extEvent->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->processedProxyEvents.size(), 1u);
    semControl->processedProxyEvents.clear(); // Manually clear processed events array since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreSignalAndWaitEventsThenProcessProxyEventsNeedsTwoCallsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = driver->externalSemaphoreController.get();

    auto extEvent = std::make_unique<MockExtEvent>();
    auto semWait = std::make_unique<ExternalSemaphoreImp>();
    auto semSignal = std::make_unique<ExternalSemaphoreImp>();
    semSignal->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    semWait->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    // Similate host synchronize failure for signal event to ensure wait event is
    //   processed first and remains in proxy events array until next process call.
    extEvent->failHostSynchronize = true;

    semControl->proxyEvents.emplace_back(extEvent->toBase(), semSignal->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    semControl->proxyEvents.emplace_back(extEvent->toBase(), semWait->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);

    semControl->processProxyEvents();

    auto neoSemaphore = semWait->neoExternalSemaphore.get();

    EXPECT_EQ(neoSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(extEvent->hostSignalCalledCnt, 1u);
    EXPECT_EQ(extEvent->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    EXPECT_EQ(semControl->processedProxyEvents.size(), 1u);
    // Reset host synchronize failure to allow signal event to be processed in next call.
    extEvent->failHostSynchronize = false;

    semControl->processProxyEvents();
    EXPECT_EQ(extEvent->hostSyncCalledCnt, 2u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    semControl->processedProxyEvents.clear(); // Manually clear processed events array since it only contains mock events.
}

} // namespace ult
} // namespace L0
