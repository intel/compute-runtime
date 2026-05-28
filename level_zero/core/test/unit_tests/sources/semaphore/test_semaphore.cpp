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

class MockEmptyEvent : public ::L0::Event {
  public:
    using BaseClass = ::L0::Event;
    using BaseClass::device;
    using BaseClass::eventPoolOffset;
    using BaseClass::setEventPool;
    using BaseClass::totalEventSize;

    // Need totalEventSize = 1 to simplify eventPoolOffset calculations and
    //   avoid division by zero in EventImp::getPoolIndex, but the actual value
    // is not important for these tests.
    MockEmptyEvent() : BaseClass(0, nullptr) { totalEventSize = 1u; }
    MockEmptyEvent(int idx, ::L0::Device *dev) : BaseClass(idx, dev) {
        eventPoolOffset = static_cast<size_t>(idx);
        totalEventSize = 1u;
    }

    static MockEmptyEvent *fromHandle(ze_event_handle_t handle) { return static_cast<MockEmptyEvent *>(handle); }

    BaseClass *toBase() { return this; }

    ze_result_t destroy() override { return ZE_RESULT_SUCCESS; }

    // Need to override all pure virtual functions of ::L0::Event in order to create an instance, but they won't be called, so they can just return success.
    ze_result_t hostSignal(bool) override { return ZE_RESULT_SUCCESS; };
    ze_result_t hostSynchronize(uint64_t) override { return ZE_RESULT_SUCCESS; };
    ze_result_t queryStatus(int64_t) override { return ZE_RESULT_SUCCESS; };
    ze_result_t reset() override { return ZE_RESULT_SUCCESS; };
    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t *) override { return ZE_RESULT_SUCCESS; };
    ze_result_t queryTimestampsExp(L0::Device *, uint32_t *, ze_kernel_timestamp_result_t *) override { return ZE_RESULT_SUCCESS; };
    ze_result_t queryKernelTimestampsExt(L0::Device *, uint32_t *, ze_event_query_kernel_timestamps_results_ext_properties_t *) override { return ZE_RESULT_SUCCESS; };
    ze_result_t getEventPool(ze_event_pool_handle_t *) override { return ZE_RESULT_SUCCESS; };
    ze_result_t getSignalScope(ze_event_scope_flags_t *) override { return ZE_RESULT_SUCCESS; };
    ze_result_t getWaitScope(ze_event_scope_flags_t *) override { return ZE_RESULT_SUCCESS; };

    uint32_t getPacketsInUse() const override { return 0u; };
    uint32_t getPacketsUsedInLastKernel() override { return 0u; };
    uint64_t getPacketAddress(L0::Device *) override { return 0u; };

    void resetKernelCountAndPacketUsedCount() override {};
    void setPacketsInUse(uint32_t) override {};

    ze_result_t hostEventSetValue(State) override { return ZE_RESULT_SUCCESS; };

    void clearTimestampTagData(uint32_t, NEO::TagNodeBase *) override {};
};

struct MockEventPool : public ::L0::EventPool {
    using BaseClass = ::L0::EventPool;
    using BaseClass::devices;

    MockEventPool() = default;
    MockEventPool(size_t numEvents, ::L0::Device *dev) : BaseClass(numEvents) { devices.push_back(dev); }

    ze_result_t destroy() override {
        destroyCalled++;
        return destroyResult;
    }
    ze_result_t createEvent(const ze_event_desc_t *desc, ze_event_handle_t *eventHandle) override {
        createEventCalled++;
        if (createEventResult == ZE_RESULT_SUCCESS) {
            MockEmptyEvent *event = new MockEmptyEvent(desc->index, devices[0]);
            event->setEventPool(this);
            *eventHandle = event->toHandle();
        }
        return createEventResult;
    }

    ze_result_t destroyResult = ZE_RESULT_SUCCESS;
    ze_result_t createEventResult = ZE_RESULT_SUCCESS;
    uint32_t destroyCalled = 0u;
    uint32_t createEventCalled = 0u;
};

class MockExtSemaphoreController : public L0::ExternalSemaphoreController {
  public:
    MockExtSemaphoreController() : L0::ExternalSemaphoreController() {}

    using L0::ExternalSemaphoreController::allocateProxyEvent;
    using L0::ExternalSemaphoreController::processProxyEvents;
    using L0::ExternalSemaphoreController::releaseEvent;
    using L0::ExternalSemaphoreController::releaseResources;

    using ReusableEventPools = L0::ExternalSemaphoreController::ReusableEventPools;

    using L0::ExternalSemaphoreController::eventPools;
    using L0::ExternalSemaphoreController::proxyEvents;
    using L0::ExternalSemaphoreController::syncEvents;
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
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(ExternalSemaphoreTest, givenRegularCommandListWhenAppendSignalExternalSemaphoresIsCalledThenInvalidArgumentIsReturned) {
    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_external_semaphore_signal_params_ext_t signalParams = {};
    ze_external_semaphore_ext_handle_t hSemaphore = reinterpret_cast<ze_external_semaphore_ext_handle_t>(0x1);
    ze_result_t result = cmdList.appendSignalExternalSemaphores(1, &hSemaphore, &signalParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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

using ExtSemReusePools = MockExtSemaphoreController::ReusableEventPools;

HWTEST_F(ExternalSemaphoreTest, givenExternalSemaphoreControllerWhenAllocateProxyEventIsCalledMultipleTimesThenSuccessIsReturned) {
    constexpr uint32_t poolCapacity = ExternalSemaphoreController::poolCapacity;

    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_device_handle_t hDevice = l0Device->toHandle();

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driverHandle->externalSemaphoreController.get());
    ExtSemReusePools &devPools = semControl->eventPools[hDevice];

    EXPECT_EQ(devPools.pools.size(), 0u);
    EXPECT_EQ(devPools.eventSlots, 0u);

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};

    ze_result_t result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    L0::Event *event1 = L0::Event::fromHandle(proxyEvent1);
    semControl->proxyEvents.emplace_back(event1, externalSemaphore1->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    EXPECT_EQ(semControl->eventPools[hDevice].eventSlots, poolCapacity - 1);
    EXPECT_EQ(event1->getPoolIndex(), 0u);

    result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    L0::Event *event2 = L0::Event::fromHandle(proxyEvent2);
    semControl->proxyEvents.emplace_back(event2, externalSemaphore2->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);
    EXPECT_EQ(semControl->eventPools[hDevice].eventSlots, poolCapacity - 2);
    EXPECT_EQ(event2->getPoolIndex(), 1u);
}

HWTEST_F(ExternalSemaphoreTest, givenEventPoolFullWhenAllocateProxyEventCalledThenEventPoolSizeIncreases) {
    constexpr uint32_t poolCapacity = ExternalSemaphoreController::poolCapacity;

    auto externalSemaphore1 = std::make_unique<ExternalSemaphoreImp>();
    auto externalSemaphore2 = std::make_unique<ExternalSemaphoreImp>();

    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_device_handle_t hDevice = l0Device->toHandle();

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driverHandle->externalSemaphoreController.get());
    ExtSemReusePools &devPools = semControl->eventPools[hDevice];

    EXPECT_EQ(devPools.pools.size(), 0u);
    EXPECT_EQ(devPools.eventSlots, 0u);

    ze_event_handle_t proxyEvent1 = {};
    ze_event_handle_t proxyEvent2 = {};

    ze_result_t result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    L0::Event *event1 = L0::Event::fromHandle(proxyEvent1);
    semControl->proxyEvents.emplace_back(event1, externalSemaphore1->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    EXPECT_EQ(devPools.pools.size(), 1u);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 1);
    EXPECT_EQ(event1->getPoolIndex(), 0u);

    devPools.eventSlots = 0; // Simulate current event pool as full.
    result = semControl->allocateProxyEvent(hDevice, context->toHandle(), &proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    L0::Event *event2 = L0::Event::fromHandle(proxyEvent2);
    semControl->proxyEvents.emplace_back(event2, externalSemaphore2->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);
    EXPECT_EQ(devPools.pools.size(), 2u);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 1);
    EXPECT_EQ(event2->getPoolIndex(), 0u); // Index == 0 since new pool was created.
}

HWTEST_F(ExternalSemaphoreTest, givenInitialSemaphoreWaitEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    semControl->processProxyEvents();

    auto neoSem = extSem->neoExternalSemaphore.get();
    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(event->hostSignalCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->syncEvents.size(), 1u);
    semControl->syncEvents.clear(); // Manually clear processed events map since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenHostSignaledSemaphoreWaitEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    auto neoSem = extSem->neoExternalSemaphore.get();
    uint64_t fence = 0u;

    neoSem->enqueueWait(&fence);
    semControl->processProxyEvents();

    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(event->hostSignalCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->syncEvents.size(), 1u);
    semControl->syncEvents.clear(); // Manually clear processed events map since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenHostSignaledSemaphoreSignalEventThenProcessProxyEventsSucceedsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    auto neoSem = extSem->neoExternalSemaphore.get();
    uint64_t fence = 0u;

    neoSem->enqueueWait(&fence);
    semControl->processProxyEvents();

    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(event->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->syncEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenHostSynchronizeFailsForSemaphoreSignalEventOperationDuringProcessProxyEventsThenExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    event->failHostSynchronize = true;

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);

    semControl->processProxyEvents();

    auto neoSem = extSem->neoExternalSemaphore.get();
    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);
    EXPECT_EQ(event->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u); // Event should remain in proxy events array since host synchronize failed.
    EXPECT_EQ(semControl->syncEvents.size(), 0u);
    semControl->proxyEvents.clear(); // Manually clear proxy events array since it only contains mock events.
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitAndSignalEventsThenProcessProxyEventsSucceedsOnFirstCallAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);

    semControl->processProxyEvents();

    auto neoSem = extSem->neoExternalSemaphore.get();

    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(event->hostSignalCalledCnt, 1u);
    EXPECT_EQ(event->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->syncEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreSignalAndWaitEventsThenProcessProxyEventsNeedsTwoCallsAndExpectedStateIsReturned) {
    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driver = std::make_unique<MockDriverHandle>();

    device->setDriverHandle(driver.get());

    driver->externalSemaphoreController = ExternalSemaphoreController::create();
    auto *semControl = reinterpret_cast<MockExtSemaphoreController *>(driver->externalSemaphoreController.get());

    auto event = std::make_unique<MockExtEvent>();
    auto extSem = std::make_unique<ExternalSemaphoreImp>();
    extSem->neoExternalSemaphore = std::make_unique<MockNeoExtSemaphore>();
    // Similate host synchronize failure for signal event to ensure wait event is
    //   processed first and remains in proxy events array until next process call.
    event->failHostSynchronize = true;

    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreSignal);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    semControl->proxyEvents.emplace_back(event->toBase(), extSem->toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl->proxyEvents.size(), 2u);

    semControl->processProxyEvents();

    auto neoSem = extSem->neoExternalSemaphore.get();

    EXPECT_EQ(neoSem->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    EXPECT_EQ(event->hostSignalCalledCnt, 1u);
    EXPECT_EQ(event->hostSyncCalledCnt, 1u);
    EXPECT_EQ(semControl->proxyEvents.size(), 1u);
    EXPECT_EQ(semControl->syncEvents.size(), 1u);
    // Reset host synchronize failure to allow signal event to be processed in next call.
    event->failHostSynchronize = false;

    semControl->processProxyEvents();
    EXPECT_EQ(event->hostSyncCalledCnt, 2u);
    EXPECT_EQ(semControl->proxyEvents.size(), 0u);
    EXPECT_EQ(semControl->syncEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitEventInProxyEventsThenReleaseResourcesSucceeds) {
    MockExtSemaphoreController semControl;

    MockEmptyEvent event;
    ExternalSemaphoreImp extSem;

    semControl.proxyEvents.emplace_back(event.toBase(), extSem.toBase(), 1u, extSemaphoreWait);
    EXPECT_EQ(semControl.proxyEvents.size(), 1u);

    semControl.releaseResources();
    EXPECT_EQ(semControl.proxyEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenSemaphoreWaitEventInSyncEventsThenReleaseResourcesSucceeds) {
    MockExtSemaphoreController semControl;
    MockEmptyEvent event;
    MockNeoExtSemaphore neoSem;

    semControl.syncEvents[&neoSem] = event.toBase();
    EXPECT_EQ(semControl.syncEvents.size(), 1u);

    semControl.releaseResources();
    EXPECT_EQ(semControl.syncEvents.size(), 0u);
}

HWTEST_F(ExternalSemaphoreTest, givenReleasedEventsCauseReuseEventNotEmptyInAllocateProxyEventThenEventSlotIsReused) {
    constexpr uint32_t poolCapacity = ExternalSemaphoreController::poolCapacity;

    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    ze_device_handle_t dev = device->toHandle();

    MockExtSemaphoreController semControl;
    semControl.eventPools.emplace(dev, ExtSemReusePools{});

    ExtSemReusePools &devPools = semControl.eventPools[dev];
    EXPECT_EQ(devPools.pools.size(), 0u);
    EXPECT_EQ(devPools.eventSlots, 0u);

    MockEventPool pool(poolCapacity, ::L0::Device::fromHandle(dev));
    devPools.pools.push_back(&pool);
    devPools.eventSlots = poolCapacity;
    EXPECT_EQ(devPools.pools.size(), 1u);
    EXPECT_EQ(devPools.eventSlots, poolCapacity);

    ze_event_handle_t proxyEvent1 = nullptr;
    ze_event_handle_t proxyEvent2 = nullptr;

    MockEmptyEvent *event1 = nullptr;
    MockEmptyEvent *event2 = nullptr;

    ze_result_t result = ZE_RESULT_SUCCESS;

    result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent1);
    event1 = MockEmptyEvent::fromHandle(proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 1);
    EXPECT_NE(static_cast<void *>(event1), nullptr);
    EXPECT_EQ(event1->getPoolIndex(), 0u);

    result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent2);
    event2 = MockEmptyEvent::fromHandle(proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 2);
    EXPECT_NE(static_cast<void *>(event2), nullptr);
    EXPECT_EQ(event2->getPoolIndex(), 1u);

    semControl.releaseEvent(event1); // Event 1 is pushed onto the reuse stack (index 0).
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 1);
    delete event1; // Manually delete event since it's not actually destroyed in releaseEvent.

    semControl.releaseEvent(event2); // Event 2 is pushed onto the reuse stack (index 1).
    EXPECT_EQ(devPools.eventSlots, poolCapacity);
    delete event2; // Manually delete event.

    // New allocatation should reuse freed event slot (index 1).
    result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent1);
    event1 = MockEmptyEvent::fromHandle(proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 1);
    EXPECT_NE(static_cast<void *>(event1), nullptr);
    EXPECT_EQ(event1->getPoolIndex(), 1u);

    // New allocatation should reuse freed event slot (index 0).
    result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent2);
    event2 = MockEmptyEvent::fromHandle(proxyEvent2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 2);
    EXPECT_NE(static_cast<void *>(event2), nullptr);
    EXPECT_EQ(event2->getPoolIndex(), 0u);

    delete event1; // Manually delete events --> doesn't update event pool slots.
    delete event2;

    result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent1);
    event1 = MockEmptyEvent::fromHandle(proxyEvent1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 3);

    auto altDevice = std::make_unique<MockDeviceImp>(neoDevice);

    event1->device = static_cast<MockDeviceImp *>(altDevice.get()); // Simulate event being signaled on a different device than it was allocated on.
    semControl.releaseEvent(event1);                                // Won't update event pool slots since event device doesn't match allocating device.
    EXPECT_EQ(devPools.eventSlots, poolCapacity - 3);

    delete event1; // Manually delete event.

    devPools.pools[0] = nullptr; // Manually set pool pointer to nullptr since pool is on stack.
    devPools.pools.clear();
    semControl.eventPools.erase(dev);
}

HWTEST_F(ExternalSemaphoreTest, givenEventPoolCreateEventFailsInAllocateProxyEventThenErrorIsReturned) {
    constexpr uint32_t poolCapacity = ExternalSemaphoreController::poolCapacity;

    auto device = std::make_unique<MockDeviceImp>(neoDevice);
    ze_device_handle_t dev = device->toHandle();

    MockExtSemaphoreController semControl;
    semControl.eventPools.emplace(dev, ExtSemReusePools{});

    ExtSemReusePools &devPools = semControl.eventPools[dev];
    EXPECT_EQ(devPools.pools.size(), 0u);
    EXPECT_EQ(devPools.eventSlots, 0u);

    MockEventPool pool(poolCapacity, ::L0::Device::fromHandle(dev));
    devPools.pools.push_back(&pool);
    devPools.eventSlots = poolCapacity;
    EXPECT_EQ(devPools.pools.size(), 1u);
    EXPECT_EQ(devPools.eventSlots, poolCapacity);

    ze_event_handle_t proxyEvent = nullptr;

    pool.createEventResult = ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    ze_result_t result = semControl.allocateProxyEvent(dev, context->toHandle(), &proxyEvent);
    EXPECT_EQ(result, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
    EXPECT_EQ(devPools.eventSlots, poolCapacity);
    EXPECT_EQ(static_cast<void *>(proxyEvent), nullptr);
    devPools.pools[0] = nullptr; // Manually set pool pointer to nullptr since pool is on stack.
    devPools.pools.clear();
    semControl.eventPools.erase(dev);
}

} // namespace ult
} // namespace L0
