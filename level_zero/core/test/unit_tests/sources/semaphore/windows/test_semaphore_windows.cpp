/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
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

class WddmSemaphoreFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};

        auto wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
};

using WddmExternalSemaphoreTest = Test<WddmSemaphoreFixture>;
using MockDriverHandleImp = Mock<L0::DriverHandleImp>;

HWTEST_F(WddmExternalSemaphoreTest, DISABLED_givenValidExternalSemaphoreWhenImportExternalSemaphoreExpIsCalledThenSuccessIsReturned) {
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    l0Device->setDriverHandle(driverHandleImp.get());

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

class MockFailGdi : public MockGdi {
  public:
    MockFailGdi() : MockGdi() {
        signalSynchronizationObjectFromCpu = mockD3DKMTSignalSynchronizationObjectFromCpu;
    }

    static NTSTATUS __stdcall mockD3DKMTSignalSynchronizationObjectFromCpu(IN CONST D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU *) {
        return STATUS_UNSUCCESSFUL;
    }
};

class MockExternalSemaphoreEvent : public MockEvent {
  public:
    MockExternalSemaphoreEvent() : MockEvent() {}
    ze_result_t hostSynchronize(uint64_t timeout) override {
        hostSynchronizeCalledTimes++;

        if (failOnFirstHostSynchronize && hostSynchronizeCalledTimes == 1) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    bool failOnFirstHostSynchronize = false;
    uint32_t hostSynchronizeCalledTimes = 0;
};

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenEnqueueSignalFailsWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned, MatchAny) {
    auto mockGdi = new MockFailGdi();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    static_cast<OsEnvironmentWin *>(l0Device->neoDevice->getExecutionEnvironment()->osEnvironment.get())->gdi.reset(mockGdi);

    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();
    auto externalSemaphoreController = driverHandleImp->externalSemaphoreController;
    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();

    externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphore)), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    externalSemaphoreController->startThread();

    auto externalSemaphore = static_cast<ExternalSemaphoreImp *>(ExternalSemaphore::fromHandle(hSemaphore));
    auto neoExternalSemaphore = externalSemaphore->neoExternalSemaphore.get();

    std::unique_lock<std::mutex> lock(externalSemaphoreController->semControllerMutex);
    externalSemaphoreController->semControllerCv.wait(lock, [&] { return (externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(neoExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);
    lock.unlock();

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenSemaphoreSignalOperationEventWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned, MatchAny) {
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    driverHandleImp->externalSemaphoreController = ExternalSemaphoreController::create();
    auto externalSemaphoreController = driverHandleImp->externalSemaphoreController;
    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();

    externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphore)), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    externalSemaphoreController->startThread();

    auto externalSemaphore = static_cast<ExternalSemaphoreImp *>(ExternalSemaphore::fromHandle(hSemaphore));
    auto neoExternalSemaphore = externalSemaphore->neoExternalSemaphore.get();

    std::unique_lock<std::mutex> lock(externalSemaphoreController->semControllerMutex);
    externalSemaphoreController->semControllerCv.wait(lock, [&] { return (externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(neoExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    lock.unlock();

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenImmediateCommandListWhenAppendWaitExternalSemaphoresExpIsCalledThenSuccessIsReturned, MatchAny) {
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    std::unique_ptr<L0::CommandList> commandList;
    ze_result_t returnValue;

    commandList.reset(CommandList::createImmediate(productFamily, l0Device.get(), &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    commandList->setCmdListContext(context);
    auto &cmdList = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_intel_external_semaphore_wait_params_exp_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenRegularCommandListWhenAppendWaitExternalSemaphoresExpIsCalledThenInvalidArgumentIsReturned, MatchAny) {
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    MockCommandListCoreFamily<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_intel_external_semaphore_wait_params_exp_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

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
        if (failingSignalEvent) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    uint32_t appendWaitOnEventsCalledTimes = 0;
    uint32_t appendSignalEventCalledTimes = 0;
    bool failingWaitOnEvents = false;
    bool failingSignalEvent = false;
};

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenInternalProxyEventFailsToAppendWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned, MatchAny) {
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_intel_external_semaphore_wait_params_exp_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenWaitEventFailsToAppendWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned, MatchAny) {
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_intel_external_semaphore_wait_params_exp_t waitParams = {};
    ze_event_handle_t waitEvent;
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 1, &waitEvent);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(WddmExternalSemaphoreTest, DISABLED_givenSignalEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned, MatchAny) {
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice, neoDevice->getExecutionEnvironment());
    auto driverHandleImp = std::make_unique<MockDriverHandleImp>();
    driverHandleImp->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandleImp.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingSignalEvent = true;

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_intel_external_semaphore_wait_params_exp_t waitParams = {};
    MockEvent signalEvent;

    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, signalEvent.toHandle(), 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
