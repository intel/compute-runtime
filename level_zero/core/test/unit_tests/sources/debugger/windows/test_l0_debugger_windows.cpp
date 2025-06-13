/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_wddm_eudebug.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include <algorithm>
#include <memory>

using namespace NEO;

namespace L0 {
namespace ult {

struct L0DebuggerWindowsFixture {
    void setUp() {
        debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        debugManager.flags.EnableHostUsmAllocationPool.set(0);
        executionEnvironment = new NEO::ExecutionEnvironment;
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        executionEnvironment->prepareRootDeviceEnvironments(1);
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddm = new WddmEuDebugInterfaceMock(*rootDeviceEnvironment);
        wddm->callBaseDestroyAllocations = false;
        wddm->callBaseMapGpuVa = false;
        wddm->callBaseWaitFromCpu = false;
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        wddm->init();

        executionEnvironment->memoryManager.reset(new MockWddmMemoryManager(*executionEnvironment));

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void tearDown() {
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    WddmEuDebugInterfaceMock *wddm = nullptr;
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    MockGdi *gdi = nullptr;
};

using L0DebuggerWindowsTest = Test<L0DebuggerWindowsFixture>;

TEST_F(L0DebuggerWindowsTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
}

TEST_F(L0DebuggerWindowsTest, givenWindowsOSWhenL0DebuggerIsCreatedAddressModeIsSingleSpace) {
    EXPECT_TRUE(device->getL0Debugger()->getSingleAddressSpaceSbaTracking());
}

HWTEST_F(L0DebuggerWindowsTest, givenDebuggingEnabledAndCommandQueuesAreCreatedAndDestroyedThanDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue1 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);

    auto commandQueue2 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);

    commandQueue1->destroy();
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    commandQueue2->destroy();
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);
}

TEST_F(L0DebuggerWindowsTest, givenAllocateGraphicsMemoryWhenAllocationRegistrationIsRequiredThenAllocationIsRegistered) {
    auto memoryManager = executionEnvironment->memoryManager.get();

    EXPECT_LE(3u, wddm->registerAllocationTypeCalled); // At least 1xSBA + 1xMODULE_DEBUG + 1xSTATE_SAVE_AREA during DebuggerL0 init
    uint32_t registerAllocationTypeCalled = wddm->registerAllocationTypeCalled;
    for (auto allocationType : {AllocationType::debugContextSaveArea,
                                AllocationType::debugSbaTrackingBuffer,
                                AllocationType::debugModuleArea,
                                AllocationType::kernelIsa}) {
        auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0u, MemoryConstants::pageSize, allocationType}));
        EXPECT_EQ(++registerAllocationTypeCalled, wddm->registerAllocationTypeCalled);

        WddmAllocation::RegistrationData registrationData = {0};
        registrationData.gpuVirtualAddress = wddmAlloc->getGpuAddress();
        registrationData.size = wddmAlloc->getUnderlyingBufferSize();

        EXPECT_EQ(0, memcmp(wddm->registerAllocationTypePassedParams.allocData, &registrationData, sizeof(registrationData)));
        memoryManager->freeGraphicsMemory(wddmAlloc);
    }
}

TEST_F(L0DebuggerWindowsTest, givenAllocateGraphicsMemoryWhenAllocationRegistrationIsNotRequiredThenAllocationIsNotRegistered) {
    auto memoryManager = executionEnvironment->memoryManager.get();

    EXPECT_LE(3u, wddm->registerAllocationTypeCalled); // At least 1xSBA + 1xMODULE_DEBUG + 1xSTATE_SAVE_AREA during DebuggerL0 init
    uint32_t registerAllocationTypeCalled = wddm->registerAllocationTypeCalled;
    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0u, MemoryConstants::pageSize, AllocationType::buffer}));
    EXPECT_EQ(registerAllocationTypeCalled, wddm->registerAllocationTypeCalled);
    memoryManager->freeGraphicsMemory(wddmAlloc);
}

TEST_F(L0DebuggerWindowsTest, givenDebuggerL0NotifyModuleCreateCalledAndCreateDebugDataEscapeFailedThenModuleCreateNotifyEscapeIsNotCalled) {
    wddm->createDebugDataPassedParam.ntStatus = STATUS_UNSUCCESSFUL;
    auto debugger = static_cast<DebuggerL0 *>(neoDevice->getDebugger());
    debugger->notifyModuleCreate((void *)0x12345678, 0x1000, 0x80000000);
    EXPECT_EQ(1u, wddm->createDebugDataCalled);
    EXPECT_EQ_VAL(0xDEADDEADu, wddm->createDebugDataPassedParam.param.hElfAddressPtr);
    EXPECT_EQ(0u, wddm->moduleCreateNotifyCalled);
}

TEST_F(L0DebuggerWindowsTest, givenDebuggerL0NotifyModuleCreateCalledAndModuleCreateNotifyEscapeIsFailedThenModuleIsNotRegistered) {
    wddm->moduleCreateNotificationPassedParam.ntStatus = STATUS_UNSUCCESSFUL;
    auto debugger = static_cast<DebuggerL0 *>(neoDevice->getDebugger());
    debugger->notifyModuleCreate((void *)0x12345678, 0x1000, 0x80000000);
    EXPECT_EQ(1u, wddm->createDebugDataCalled);
    EXPECT_EQ_VAL(0x12345678u, wddm->createDebugDataPassedParam.param.hElfAddressPtr);
    EXPECT_EQ(1u, wddm->moduleCreateNotifyCalled);
    EXPECT_EQ(0xDEADDEADu, wddm->moduleCreateNotificationPassedParam.param.hElfAddressPtr);
}

TEST_F(L0DebuggerWindowsTest, givenDebuggerL0NotifyModuleCreateCalledThenCreateDebugDataAndModuleCreateNotifyEscapesAreCalled) {
    auto debugger = static_cast<DebuggerL0 *>(neoDevice->getDebugger());
    debugger->notifyModuleCreate((void *)0x12345678, 0x1000, 0x80000000);
    EXPECT_EQ(1u, wddm->createDebugDataCalled);
    EXPECT_EQ(ELF_BINARY, wddm->createDebugDataPassedParam.param.DebugDataType);
    EXPECT_EQ(0x1000u, wddm->createDebugDataPassedParam.param.DataSize);
    EXPECT_EQ_VAL(0x12345678u, wddm->createDebugDataPassedParam.param.hElfAddressPtr);

    EXPECT_EQ(1u, wddm->moduleCreateNotifyCalled);
    EXPECT_TRUE(wddm->moduleCreateNotificationPassedParam.param.IsCreate);
    EXPECT_EQ(0x1000u, wddm->moduleCreateNotificationPassedParam.param.Modulesize);
    EXPECT_EQ_VAL(0x12345678u, wddm->moduleCreateNotificationPassedParam.param.hElfAddressPtr);
    EXPECT_EQ(0x80000000, wddm->moduleCreateNotificationPassedParam.param.LoadAddress);
}

TEST_F(L0DebuggerWindowsTest, givenDebuggerL0NotifyModuleDestroyCalledThenModuleDestroyNotifyEscapeIsCalled) {
    auto debugger = static_cast<DebuggerL0 *>(neoDevice->getDebugger());
    debugger->notifyModuleDestroy(0x80000000);

    EXPECT_EQ(1u, wddm->moduleCreateNotifyCalled);
    EXPECT_FALSE(wddm->moduleCreateNotificationPassedParam.param.IsCreate);
    EXPECT_EQ(0u, wddm->moduleCreateNotificationPassedParam.param.Modulesize);
    EXPECT_EQ_VAL(0ull, wddm->moduleCreateNotificationPassedParam.param.hElfAddressPtr);
    EXPECT_EQ(0x80000000u, wddm->moduleCreateNotificationPassedParam.param.LoadAddress);
}

TEST_F(L0DebuggerWindowsTest, givenDebuggerL0NotifyModuleDestroyCalledAndModuleDestroyNotifyEscapeIsFailedThenErrorMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(255);

    testing::internal::CaptureStderr();
    wddm->moduleCreateNotificationPassedParam.ntStatus = STATUS_UNSUCCESSFUL;
    auto debugger = static_cast<DebuggerL0 *>(neoDevice->getDebugger());
    debugger->notifyModuleDestroy(0x80000000);

    EXPECT_EQ(1u, wddm->moduleCreateNotifyCalled);
    EXPECT_TRUE(hasSubstr(testing::internal::GetCapturedStderr(), std::string("KM_ESCAPE_EUDBG_UMD_MODULE_DESTROY_NOTIFY: Failed - Status:")));
}

TEST_F(L0DebuggerWindowsTest, givenProgramDebuggingEnabledAndDebugAttachAvailableWhenInitializingDriverThenSuccessIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    auto hwInfo = *NEO::defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);

    WddmEuDebugInterfaceMock *wddm = new WddmEuDebugInterfaceMock(*executionEnvironment->rootDeviceEnvironments[0]);
    wddm->callBaseDestroyAllocations = false;
    wddm->callBaseMapGpuVa = false;
    wddm->callBaseWaitFromCpu = false;

    auto osInterface = new OSInterface();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();
    executionEnvironment->memoryManager.reset(new MockWddmMemoryManager(*executionEnvironment));

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();

    driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;
    wddm->debugAttachAvailable = true;

    ze_result_t result = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
