/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include "gtest/gtest.h"

using namespace NEO;

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

    void ensureThreadCompletion(ExternalSemaphoreController &controller) {
        while (true) {
            std::unique_lock<std::mutex> lock(controller.semControllerMutex);
            if (controller.proxyEvents.size() == 0) {
                break;
            }
        }
    }
};

using WddmExternalSemaphoreMTTest = Test<WddmSemaphoreFixture>;
using MockDriverHandle = Mock<L0::DriverHandle>;

HWTEST_F(WddmExternalSemaphoreMTTest, givenValidExternalSemaphoreWhenImportExternalSemaphoreExpIsCalledThenSuccessIsReturned) {
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    l0Device->setDriverHandle(driverHandle.get());

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenValidTimelineSemaphoreWhenImportExternalSemaphoreIsCalledThenSuccessIsReturned) {
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    l0Device->setDriverHandle(driverHandle.get());

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_VK_TIMELINE_SEMAPHORE_WIN32;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);
    win32Desc.name = extSemName;

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
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

HWTEST_F(WddmExternalSemaphoreMTTest, givenEnqueueSignalFailsWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned) {
    auto mockGdi = new MockFailGdi();
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    static_cast<OsEnvironmentWin *>(l0Device->neoDevice->getExecutionEnvironment()->osEnvironment.get())->gdi.reset(mockGdi);

    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();

    driverHandle->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphore)), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(driverHandle->externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    driverHandle->externalSemaphoreController->startThread();

    auto externalSemaphore = static_cast<ExternalSemaphoreImp *>(ExternalSemaphore::fromHandle(hSemaphore));
    auto neoExternalSemaphore = externalSemaphore->neoExternalSemaphore.get();

    std::unique_lock<std::mutex> lock(driverHandle->externalSemaphoreController->semControllerMutex);
    driverHandle->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandle->externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(neoExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Initial);
    lock.unlock();

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenSemaphoreSignalOperationEventWhenExternalSemaphoreControllerIsRunningThenExpectedStateIsReturned) {
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    driverHandle->externalSemaphoreController = ExternalSemaphoreController::create();
    auto proxyEvent = std::make_unique<MockExternalSemaphoreEvent>();
    auto hProxyEvent = proxyEvent->toHandle();

    driverHandle->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(hProxyEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphore)), 1u, ExternalSemaphoreController::SemaphoreOperation::Signal));
    EXPECT_EQ(driverHandle->externalSemaphoreController->proxyEvents.size(), 1u);

    proxyEvent->hostSignal(false);
    driverHandle->externalSemaphoreController->startThread();

    auto externalSemaphore = static_cast<ExternalSemaphoreImp *>(ExternalSemaphore::fromHandle(hSemaphore));
    auto neoExternalSemaphore = externalSemaphore->neoExternalSemaphore.get();

    std::unique_lock<std::mutex> lock(driverHandle->externalSemaphoreController->semControllerMutex);
    driverHandle->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandle->externalSemaphoreController->proxyEvents.empty()); });
    EXPECT_EQ(neoExternalSemaphore->getState(), NEO::ExternalSemaphore::SemaphoreState::Signaled);
    lock.unlock();

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenImmediateCommandListWhenAppendWaitExternalSemaphoresExpIsCalledThenSuccessIsReturned) {
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    std::unique_ptr<L0::CommandList> commandList;
    ze_result_t returnValue;

    commandList.reset(CommandList::createImmediate(productFamily, l0Device.get(), &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    commandList->setCmdListContext(context);
    auto &cmdList = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::unique_lock<std::mutex> lock(driverHandle->externalSemaphoreController->semControllerMutex);
    driverHandle->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandle->externalSemaphoreController->proxyEvents.empty()); });
    lock.unlock();

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenRegularCommandListWhenAppendWaitExternalSemaphoresExpIsCalledThenInvalidArgumentIsReturned) {
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    MockCommandListCoreFamily<FamilyType::gfxCoreFamily> cmdList;
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);

    ensureThreadCompletion(*driverHandle->externalSemaphoreController);

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenInternalProxyEventFailsToAppendWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsNotReturned) {
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;
    cmdList.skipAppendWaitOnSingleEvent = true;

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 0, nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    ensureThreadCompletion(*driverHandle->externalSemaphoreController);

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenWaitEventFailsToAppendWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsReturned) {
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingWaitOnEvents = true;

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    ze_event_handle_t waitEvent = nullptr;
    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, nullptr, 1, &waitEvent);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);

    ensureThreadCompletion(*driverHandle->externalSemaphoreController);

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenSignalEventFailsWhenAppendWaitExternalSemaphoresExpIsCalledThenErrorIsNotReturned) {
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    driverHandle->setMemoryManager(mockMemoryManager.get());
    l0Device->setDriverHandle(driverHandle.get());

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(l0Device.get(), l0Device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    MockCommandListImmediateExtSem<FamilyType::gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.cmdQImmediate = queue.get();
    cmdList.initialize(l0Device.get(), NEO::EngineGroupType::renderCompute, 0u);
    cmdList.setCmdListContext(context);
    cmdList.failingSignalEvent = true;

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    ze_external_semaphore_wait_params_ext_t waitParams = {};
    MockEvent signalEvent;

    result = cmdList.appendWaitExternalSemaphores(1, &hSemaphore, &waitParams, signalEvent.toHandle(), 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    std::unique_lock<std::mutex> lock(driverHandle->externalSemaphoreController->semControllerMutex);
    driverHandle->externalSemaphoreController->semControllerCv.wait(lock, [&] { return (driverHandle->externalSemaphoreController->proxyEvents.empty()); });
    lock.unlock();

    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
