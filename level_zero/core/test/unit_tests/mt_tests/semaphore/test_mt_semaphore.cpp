/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/ze_intel_gpu.h"

using namespace NEO;
#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using ExternalSemaphoreMTTest = Test<DeviceFixture>;

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandListImmediateExtSem : public WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>> {
    MockCommandListImmediateExtSem() : WhiteBox<::L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>() {}

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) override {

        appendWaitOnEventsCalledTimes++;

        if (failingWaitOnEvents) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, bool relaxedOrderingDispatch) override {
        appendSignalEventCalledTimes++;

        if (failOnSecondSignalEvent && appendSignalEventCalledTimes == 2) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        if (failingSignalEvent) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    uint32_t appendWaitOnEventsCalledTimes = 0;
    uint32_t appendSignalEventCalledTimes = 0;
    bool failingWaitOnEvents = false;
    bool failingSignalEvent = false;
    bool failOnSecondSignalEvent = false;
};

using MockDriverHandleImp = Mock<L0::DriverHandleImp>;

class MockNEOExternalSemaphore : public NEO::ExternalSemaphore {
  public:
    MockNEOExternalSemaphore() : NEO::ExternalSemaphore() {}
    using NEO::ExternalSemaphore::state;

    bool enqueueWait(uint64_t *fenceValue) override {
        this->state = NEO::ExternalSemaphore::SemaphoreState::Waiting;
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

class MockExternalSemaphoreEvent : public MockEvent {
  public:
    MockExternalSemaphoreEvent() : MockEvent() {}
    ze_result_t hostSynchronize(uint64_t timeout) override {
        hostSynchronizeCalledTimes++;

        if (failHostSynchronize) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    bool failHostSynchronize = false;
    uint32_t hostSynchronizeCalledTimes = 0;
};

HWTEST_F(ExternalSemaphoreMTTest, givenSemaphoreSignalOperationEventWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    externalSemaphore->neoExternalSemaphore = std::make_unique<MockNEOExternalSemaphore>();

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();

    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    driverHandleImp->externalSemaphoreController->startThread();

    auto mockNEOExternalSemaphore = static_cast<MockNEOExternalSemaphore *>(externalSemaphore->neoExternalSemaphore.get());

    std::unique_lock<std::mutex> lock(driverHandleImp->externalSemaphoreController->semControllerMutex);
    driverHandleImp->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandleImp->externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(mockNEOExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    lock.unlock();
}

HWTEST_F(ExternalSemaphoreMTTest, givenHostSynchronizeFailsWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    externalSemaphore->neoExternalSemaphore = std::make_unique<MockNEOExternalSemaphore>();

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();

    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();
    proxyEvent->failHostSynchronize = true;

    driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(externalSemaphore->toHandle())), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(driverHandleImp->externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    driverHandleImp->externalSemaphoreController->startThread();

    auto mockNEOExternalSemaphore = static_cast<MockNEOExternalSemaphore *>(externalSemaphore->neoExternalSemaphore.get());

    std::unique_lock<std::mutex> lock(driverHandleImp->externalSemaphoreController->semControllerMutex);
    driverHandleImp->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandleImp->externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(mockNEOExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);
    lock.unlock();

    EXPECT_EQ(proxyEvent->hostSynchronizeCalledTimes, 1u);
}

HWTEST_F(ExternalSemaphoreMTTest, givenNEOExternalSemaphoreWhenAppendWaitExternalSemaphoresExpIsCalledThenExpectedSemaphoreStateIsReturned) {
    auto externalSemaphore = std::make_unique<ExternalSemaphoreImp>();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();

    externalSemaphore->neoExternalSemaphore = std::make_unique<MockNEOExternalSemaphore>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();
    driverHandleImp->externalSemaphoreController->startThread();

    auto mockNEOExternalSemaphore = static_cast<MockNEOExternalSemaphore *>(externalSemaphore->neoExternalSemaphore.get());
    EXPECT_EQ(mockNEOExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);

    ze_external_semaphore_wait_params_ext_t waitParams = {};

    ze_external_semaphore_ext_handle_t hSemaphore = externalSemaphore->toHandle();
    ze_result_t result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(cmdList.appendWaitOnEventsCalledTimes, 1u);
    EXPECT_EQ(cmdList.appendSignalEventCalledTimes, 0u);

    std::unique_lock<std::mutex> lock(driverHandleImp->externalSemaphoreController->semControllerMutex);
    driverHandleImp->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (mockNEOExternalSemaphore->getState() == NEO::ExternalSemaphore::SemaphoreState::Waiting); });
    EXPECT_EQ(mockNEOExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Waiting);

    mockNEOExternalSemaphore->state = NEO::ExternalSemaphore::SemaphoreState::Signaled;
    lock.unlock();
    driverHandleImp->externalSemaphoreController->semControllerCv.notify_all();

    EXPECT_EQ(mockNEOExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
}

} // namespace ult
} // namespace L0
