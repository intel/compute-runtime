/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ipc_socket_server.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using ContextIsShareable = Test<DeviceFixture>;
TEST_F(ContextIsShareable, whenCallingisSharedMemoryThenCorrectResultIsReturned) {
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    bool exportableMemoryFalse = false;
    bool exportableMemoryTrue = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice, false));
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryTrue, neoDevice, false));
    // exportDesc set && neoDevice is NOT WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(&desc, exportableMemoryFalse, neoDevice, false));
    // exportDesc unset && neoDevice is NOT WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice, false));
    // exportDesc unset && neoDevice is WDDM
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice, false));
    // exportDesc is set && Exportable Memory is False && neoDevice is WDDM
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(&desc, exportableMemoryFalse, neoDevice, false));

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextIsShareable, whenCreatingContextWithPidfdApproachTrueThenContextSettingsSetCorrectly) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1); // Ignored by WDDM
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));
    // Always true for WDDM (pidfd setting ignored); should be true for non-WDDM when pidfd is enabled.
    EXPECT_TRUE(contextImp->settings.useOpaqueHandle);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextIsShareable, whenCreatingContextWithPidfdApproachFalseThenContextSettingsSetCorrectly) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnablePidFdOrSocketsForIpc.set(0); // Ignored by WDDM.
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto rootEnv = neoDevice->executionEnvironment->rootDeviceEnvironments[0].get();
    auto driverType = rootEnv->osInterface ? rootEnv->osInterface->getDriverModel()->getDriverModelType()
                                           : NEO::DriverModelType::unknown;

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));
    bool useOpaque = contextImp->settings.useOpaqueHandle;
    // Always true for WDDM; should be false for others when pidfd is disabled.
    EXPECT_EQ(useOpaque, driverType == NEO::DriverModelType::wddm);
    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using GetMemHandlePtrTest = Test<GetMemHandlePtrTestFixture>;
TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithValidNTHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    // Test Successfully returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithInvalidHandleThenNullptrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    driverHandle->failHandleLookup = true;

    // Test Failing returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));

    // Test Failing returning fd Handle
    fixtureMemoryManager->ntHandle = false;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithDRMDriverTypeWithNonNTHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    uint64_t handle = 57;

    // Test Successfully returning fd Handle
    fixtureMemoryManager->ntHandle = false;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithWDDMDriverTypeWithNonNTHandleThenNullPtrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    uint64_t handle = 57;

    // Test Successfully returning fd Handle
    fixtureMemoryManager->ntHandle = false;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithPidfdMethodAndPidfdOpenSyscallReturnFailThenPidfdGetNotCalled) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(0);

    // Enable pidfd/sockets for IPC
    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return -1;
    });

    uint64_t handle = 57;

    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(0, NEO::SysCalls::pidfdgetfdCalled);
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithPidfdMethodAndPidfdGetSyscallReturnFailThenCorrectHandleIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(0);

    // Enable pidfd/sockets for IPC
    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdGetfd)> mockPidfdGet(&NEO::SysCalls::sysCallsPidfdGetfd, [](int, int, unsigned int) -> int {
        return -1;
    });
    uint64_t handle = 57;

    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 0u, 0));
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(1, NEO::SysCalls::pidfdgetfdCalled);
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithPidfdMethodAndPidfdGetSyscallFailsWithNegativeValueThenFallbackHandleIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(0);

    // Enable pidfd/sockets for IPC
    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    uint64_t originalHandle = 42;
    static bool pidfdGetFdCalled = false;
    pidfdGetFdCalled = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);

    // Mock pidfd_open to succeed
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return 100; // Valid pidfd
    });

    // Mock pidfd_getfd to fail with different negative values
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdGetfd)> mockPidfdGet(&NEO::SysCalls::sysCallsPidfdGetfd, [](int pidfd, int fd, unsigned int flags) -> int {
        pidfdGetFdCalled = true;
        EXPECT_EQ(100, pidfd); // Should receive the pidfd from pidfd_open
        EXPECT_EQ(42, fd);     // Should receive the original handle
        EXPECT_EQ(0u, flags);  // Flags should be 0
        return -2;             // Fail with a different negative value
    });

    void *result = context->getMemHandlePtr(device, originalHandle, NEO::AllocationType::buffer, 0u, 0);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(1, NEO::SysCalls::pidfdgetfdCalled);
    EXPECT_TRUE(pidfdGetFdCalled);
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithPidfdMethodAndPidfdGetSyscallReturnsZeroThenSuccessfulHandleIsUsed) {
    // Enable opaque IPC handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    uint64_t originalHandle = 123;
    static bool pidfdGetFdCalled = false;
    pidfdGetFdCalled = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);

    // Mock pidfd_open to succeed
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return 200; // Valid pidfd
    });

    // Mock pidfd_getfd to return 0 (which is a valid fd but edge case)
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdGetfd)> mockPidfdGet(&NEO::SysCalls::sysCallsPidfdGetfd, [](int pidfd, int fd, unsigned int flags) -> int {
        pidfdGetFdCalled = true;
        EXPECT_EQ(200, pidfd);
        EXPECT_EQ(123, fd);
        EXPECT_EQ(0u, flags);
        return 0; // Return 0 (valid fd)
    });

    void *result = context->getMemHandlePtr(device, originalHandle, NEO::AllocationType::buffer, 0u, 0);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(1, NEO::SysCalls::pidfdgetfdCalled);
    EXPECT_TRUE(pidfdGetFdCalled);
}

using GetDataFromIpcHandleTest = Test<GetMemHandlePtrTestFixture>;

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithOpaqueHandleEnabledThenOpaqueDataIsExtracted) {
    // Enable opaque IPC handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = true;

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcOpaqueMemoryData *opaqueData = reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    opaqueData->handle.fd = 123;
    opaqueData->memoryType = 42;
    opaqueData->processId = 456;
    opaqueData->poolOffset = 789;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int pid = 0;
    uint64_t offst = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, pid, offst);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_EQ(123u, handle);
    EXPECT_EQ(42u, type);
    EXPECT_EQ(456u, pid);
    EXPECT_EQ(789u, offst);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithOpaqueHandleDisabledThenRegularDataIsExtracted) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    // Disable opaque handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = false;

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcMemoryData *regularData = reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    regularData->handle = 987;
    regularData->type = 65;
    regularData->poolOffset = 321;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int pid = 0;
    uint64_t offst = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, pid, offst);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_EQ(987u, handle);
    EXPECT_EQ(65u, type);
    EXPECT_EQ(321u, offst);
    // pid should remain 0 for regular data
    EXPECT_EQ(0u, pid);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithNullOSInterfaceThenRegularDataIsExtracted) {
    // Disable opaque handles and set null osInterface
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = false;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset();

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcMemoryData *regularData = reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    regularData->handle = 111;
    regularData->type = 22;
    regularData->poolOffset = 333;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int pid = 0;
    uint64_t offst = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, pid, offst);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_EQ(111u, handle);
    EXPECT_EQ(22u, type);
    EXPECT_EQ(333u, offst);
    EXPECT_EQ(0u, pid);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithZeroValuesThenZeroValuesAreExtracted) {
    // Enable opaque IPC handles
    bool useOpaque = context->settings.useOpaqueHandle;
    context->settings.useOpaqueHandle = true;

    ze_ipc_mem_handle_t ipcHandle = {};
    // All values should be zero by default

    uint64_t handle = 999; // Initialize with non-zero to verify it gets overwritten
    uint8_t type = 99;
    unsigned int pid = 888;
    uint64_t offst = 777;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, pid, offst);

    // Reset opaque IPC handle setting.
    context->settings.useOpaqueHandle = useOpaque;

    EXPECT_EQ(0u, handle);
    EXPECT_EQ(0u, type);
    EXPECT_EQ(0u, pid);
    EXPECT_EQ(0u, offst);
}

inline int mockPrctl(int option, unsigned long arg) {
    return 0;
}

using IsOpaqueHandleSupportedTest = Test<GetMemHandlePtrTestFixture>;

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithWDDMDriverThenTrueIsReturnedAndHandleTypeIsNT) {
    // Set WDDM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::ntHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledThenTrueIsReturnedAndHandleTypeIsFd) {
    // Ensure opaque handles are enabled
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);

    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> prctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueDisabledThenFalseIsReturnedAndHandleTypeIsFd) {
    // Turn off opaque handles with EnablePidFdOrSocketsForIpc debug flag
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(0);

    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> prctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledThenPrctlIsCalledSuccessfully) {
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> sysCallsPrctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    // Enable debug flag to trigger mock prctl call
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledAndPrctlFailsThenFalseIsReturnedAndHandleTypeIsFd) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(0);
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);

    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    // Save original sysCallsPrctl and override to simulate failure
    auto prctlOrig = NEO::SysCalls::sysCallsPrctl;
    NEO::SysCalls::sysCallsPrctl = [](int, unsigned long) -> int {
        return -1; // Simulate failure
    };

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);

    // Restore original sysCallsPrctl
    NEO::SysCalls::sysCallsPrctl = prctlOrig;
}

using ContextSystemBarrierTest = Test<DeviceFixture>;
TEST_F(ContextSystemBarrierTest, whenCallingSystemBarrierWithUnknownDriverModelThenUnsupportedErrorIsReturned) {
    auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<NEO::OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    ze_context_handle_t hContext = nullptr;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ASSERT_EQ(ZE_RESULT_SUCCESS, driverHandle->createContext(&desc, 0u, nullptr, &hContext));

    auto *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, contextImp->systemBarrier(device->toHandle()));

    EXPECT_EQ(ZE_RESULT_SUCCESS, contextImp->destroy());
}

TEST_F(ContextSystemBarrierTest, whenCallingSystemBarrierWithNullDeviceThenInvalidArgumentIsReturned) {
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_result_t barrierRes = contextImp->systemBarrier(nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, barrierRes);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextSystemBarrierTest, givenDiscreteDeviceWhenCallingSystemBarrierThenBarrierIsExecutedAndSuccessIsReturned) {
    auto &rootDeviceEnvironment = *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface());
    auto drmMock = new DrmMock(rootDeviceEnvironment);
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::DriverModel>(drmMock));

    uint32_t barrierMemory = 0xFFFFFFFF;

    auto mockIoctlHelper = new NEO::MockIoctlHelper(*drmMock);
    mockIoctlHelper->pciBarrierMmapReturnValue = &barrierMemory;
    drmMock->ioctlHelper.reset(mockIoctlHelper);

    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo->capabilityTable.isIntegratedDevice = false;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_result_t barrierRes = contextImp->systemBarrier(static_cast<Device *>(device)->toHandle());

    EXPECT_EQ(ZE_RESULT_SUCCESS, barrierRes);
    EXPECT_TRUE(mockIoctlHelper->pciBarrierMmapCalled);
    EXPECT_EQ(0u, barrierMemory);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(IsOpaqueHandleSupportedTest, givenDRMDriverAndOpaqueEnabledAndPrctlFailsAndSocketFallbackEnabledThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(0);

    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    context->settings.useOpaqueHandle = true;

    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> prctlBackup(&NEO::SysCalls::sysCallsPrctl);
    NEO::SysCalls::sysCallsPrctl = [](int, unsigned long) -> int {
        return -1; // Simulate failure
    };

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, givenDRMDriverAndOpaqueEnabledAndForceIpcSocketFallbackThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(1);

    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    context->settings.useOpaqueHandle = true;

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(GetMemHandlePtrTest, givenDRMDriverAndPidfdFailsWithSocketFallbackEnabledThenSocketClientIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(0);

    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::socketCalled)> socketCalledBackup(&NEO::SysCalls::socketCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::connectCalled)> connectCalledBackup(&NEO::SysCalls::connectCalled, 0);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> mockSocket(&NEO::SysCalls::sysCallsSocket, [](int, int, int) -> int {
        return 10;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsConnect)> mockConnect(&NEO::SysCalls::sysCallsConnect, [](int, const struct sockaddr *, socklen_t) -> int {
        return -1; // Connection fails
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetsockopt)> mockSetsockopt(&NEO::SysCalls::sysCallsSetsockopt, [](int, int, int, const void *, socklen_t) -> int {
        return 0;
    });

    uint64_t handle = 57;

    void *result = context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 1234, 0);
    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_GT(NEO::SysCalls::socketCalled, 0);
}

TEST_F(GetMemHandlePtrTest, givenDRMDriverAndPidfdFailsAndSocketFallbackSucceedsThenValidPtrReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(0);

    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::socketCalled)> socketCalledBackup(&NEO::SysCalls::socketCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::connectCalled)> connectCalledBackup(&NEO::SysCalls::connectCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::sendCalled)> sendCalledBackup(&NEO::SysCalls::sendCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::recvmsgCalled)> recvmsgCalledBackup(&NEO::SysCalls::recvmsgCalled, 0);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> mockSocket(&NEO::SysCalls::sysCallsSocket, [](int, int, int) -> int {
        return 10;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsConnect)> mockConnect(&NEO::SysCalls::sysCallsConnect, [](int, const struct sockaddr *, socklen_t) -> int {
        return 0; // Connection succeeds
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetsockopt)> mockSetsockopt(&NEO::SysCalls::sysCallsSetsockopt, [](int, int, int, const void *, socklen_t) -> int {
        return 0;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSend)> mockSend(&NEO::SysCalls::sysCallsSend, [](int, const void *, size_t len, int) -> ssize_t {
        return static_cast<ssize_t>(len); // Send succeeds
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRecvmsg)> mockRecvmsg(&NEO::SysCalls::sysCallsRecvmsg, [](int, struct msghdr *msg, int) -> ssize_t {
        if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int))) {
            struct cmsghdr *cmsg = CMSG_FIRSTHDR(msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            *reinterpret_cast<int *>(CMSG_DATA(cmsg)) = 200; // Return fd 200
        }
        if (msg->msg_iov && msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            NEO::IpcSocketResponsePayload *payload = static_cast<NEO::IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = true;
        }
        return static_cast<ssize_t>(msg->msg_iov ? msg->msg_iov[0].iov_len : 0);
    });

    uint64_t handle = 57;

    void *result = context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 1234, 0);
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_GT(NEO::SysCalls::socketCalled, 0);
    EXPECT_GT(NEO::SysCalls::connectCalled, 0);
}

TEST_F(GetMemHandlePtrTest, givenDRMDriverAndPidfdFailsAndSocketConnectsButRequestHandleFailsThenNullptrReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(0);

    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::socketCalled)> socketCalledBackup(&NEO::SysCalls::socketCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::connectCalled)> connectCalledBackup(&NEO::SysCalls::connectCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::sendCalled)> sendCalledBackup(&NEO::SysCalls::sendCalled, 0);
    VariableBackup<decltype(NEO::SysCalls::recvmsgCalled)> recvmsgCalledBackup(&NEO::SysCalls::recvmsgCalled, 0);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> mockSocket(&NEO::SysCalls::sysCallsSocket, [](int, int, int) -> int {
        return 10;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsConnect)> mockConnect(&NEO::SysCalls::sysCallsConnect, [](int, const struct sockaddr *, socklen_t) -> int {
        return 0; // Connection succeeds
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetsockopt)> mockSetsockopt(&NEO::SysCalls::sysCallsSetsockopt, [](int, int, int, const void *, socklen_t) -> int {
        return 0;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSend)> mockSend(&NEO::SysCalls::sysCallsSend, [](int, const void *, size_t len, int) -> ssize_t {
        return static_cast<ssize_t>(len); // Send succeeds
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsRecvmsg)> mockRecvmsg(&NEO::SysCalls::sysCallsRecvmsg, [](int, struct msghdr *msg, int) -> ssize_t {
        if (msg->msg_iov && msg->msg_iovlen > 0 && msg->msg_iov[0].iov_base) {
            NEO::IpcSocketResponsePayload *payload = static_cast<NEO::IpcSocketResponsePayload *>(msg->msg_iov[0].iov_base);
            payload->success = false; // Server reports failure
        }
        return static_cast<ssize_t>(msg->msg_iov ? msg->msg_iov[0].iov_len : 0);
    });

    uint64_t handle = 57;

    void *result = context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 1234, 0);
    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_GT(NEO::SysCalls::socketCalled, 0);
    EXPECT_GT(NEO::SysCalls::connectCalled, 0);
}

TEST_F(GetMemHandlePtrTest, givenDRMDriverAndForceIpcSocketFallbackThenPidfdIsSkipped) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(1);

    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::socketCalled)> socketCalledBackup(&NEO::SysCalls::socketCalled, 0);

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> mockSocket(&NEO::SysCalls::sysCallsSocket, [](int, int, int) -> int {
        return 10;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsConnect)> mockConnect(&NEO::SysCalls::sysCallsConnect, [](int, const struct sockaddr *, socklen_t) -> int {
        return -1;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetsockopt)> mockSetsockopt(&NEO::SysCalls::sysCallsSetsockopt, [](int, int, int, const void *, socklen_t) -> int {
        return 0;
    });

    uint64_t handle = 57;

    void *result = context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 1234, 0);
    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(0, NEO::SysCalls::pidfdopenCalled); // pidfd should be skipped
    EXPECT_GT(NEO::SysCalls::socketCalled, 0);    // Socket should be attempted
}

TEST_F(GetMemHandlePtrTest, givenDRMDriverAndPidfdSucceedsThenSocketFallbackNotUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);
    debugManager.flags.ForceIpcSocketFallback.set(0);

    context->settings.useOpaqueHandle = true;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);
    fixtureMemoryManager->ntHandle = false;

    VariableBackup<decltype(NEO::SysCalls::pidfdopenCalled)> pidfdOpenCalledBackup(&NEO::SysCalls::pidfdopenCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::pidfdgetfdCalled)> pidfdGetFdCalledBackup(&NEO::SysCalls::pidfdgetfdCalled, 0u);
    VariableBackup<decltype(NEO::SysCalls::socketCalled)> socketCalledBackup(&NEO::SysCalls::socketCalled, 0);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdOpen)> mockPidfdOpen(&NEO::SysCalls::sysCallsPidfdOpen, [](pid_t, unsigned int) -> int {
        return 100;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsPidfdGetfd)> mockPidfdGet(&NEO::SysCalls::sysCallsPidfdGetfd, [](int, int, unsigned int) -> int {
        return 200;
    });

    uint64_t handle = 57;

    void *result = context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, 1234, 0);
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(1, NEO::SysCalls::pidfdgetfdCalled);
    EXPECT_EQ(0, NEO::SysCalls::socketCalled); // Socket should not be used
}

class ContextWhiteboxForSetIPCHandleData : public ::L0::ContextImp {
  public:
    ContextWhiteboxForSetIPCHandleData(L0::DriverHandle *driverHandle) : L0::ContextImp(driverHandle) {}

    using ::L0::ContextImp::setIPCHandleData;
};

using SetIPCHandleDataSocketTest = Test<DeviceFixture>;

TEST_F(SetIPCHandleDataSocketTest, givenOpaqueHandleWithFdTypeAndSocketFallbackEnabledThenSocketServerIsInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> socketBackup(&NEO::SysCalls::sysCallsSocket);
    VariableBackup<decltype(NEO::SysCalls::sysCallsBind)> bindBackup(&NEO::SysCalls::sysCallsBind);
    VariableBackup<decltype(NEO::SysCalls::sysCallsListen)> listenBackup(&NEO::SysCalls::sysCallsListen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> dupBackup(&NEO::SysCalls::sysCallsDup);

    NEO::SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    NEO::SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    NEO::SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    NEO::SysCalls::sysCallsDup = [](int oldfd) -> int { return oldfd + 100; };

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 12345;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x1000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
    driverHandle->shutdownIpcSocketServer();
}

TEST_F(SetIPCHandleDataSocketTest, givenOpaqueHandleWithFdTypeAndSocketFallbackDisabledThenSocketServerIsNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(0);

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 67890;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x2000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(SetIPCHandleDataSocketTest, givenOpaqueHandleWithNtHandleTypeAndSocketFallbackEnabledThenSocketServerIsNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 11111;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x3000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::ntHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    uint64_t reservedValue = 0;
    memcpy(&reservedValue, &handleIterator->second->opaqueData.handle.reserved, sizeof(reservedValue));
    EXPECT_EQ(handle, reservedValue);

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(SetIPCHandleDataSocketTest, givenNonOpaqueHandleWithSocketFallbackEnabledThenSocketServerIsNotInitialized) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 22222;
    L0::IpcMemoryData ipcData = {};
    uint64_t ptrAddress = 0x4000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcMemoryData>(&mockAllocation, handle, ipcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    EXPECT_EQ(handle, handleIterator->second->ipcData.handle);

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(SetIPCHandleDataSocketTest, givenOpaqueHandleWithFdTypeAndSocketServerInitFailsThenHandleStillAdded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> socketBackup(&NEO::SysCalls::sysCallsSocket);

    NEO::SysCalls::sysCallsSocket = [](int, int, int) -> int { return -1; };

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 33333;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x5000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
}

TEST_F(SetIPCHandleDataSocketTest, givenOpaqueHandleWithFdTypeAndRegisterHandleFailsThenHandleStillAdded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableIpcSocketFallback.set(1);

    VariableBackup<decltype(NEO::SysCalls::sysCallsSocket)> socketBackup(&NEO::SysCalls::sysCallsSocket);
    VariableBackup<decltype(NEO::SysCalls::sysCallsBind)> bindBackup(&NEO::SysCalls::sysCallsBind);
    VariableBackup<decltype(NEO::SysCalls::sysCallsListen)> listenBackup(&NEO::SysCalls::sysCallsListen);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDup)> dupBackup(&NEO::SysCalls::sysCallsDup);

    NEO::SysCalls::sysCallsSocket = [](int, int, int) -> int { return 5; };
    NEO::SysCalls::sysCallsBind = [](int, const struct sockaddr *, socklen_t) -> int { return 0; };
    NEO::SysCalls::sysCallsListen = [](int, int) -> int { return 0; };
    NEO::SysCalls::sysCallsDup = [](int) -> int { return -1; };

    ContextWhiteboxForSetIPCHandleData contextWhitebox(driverHandle.get());

    NEO::MockGraphicsAllocation mockAllocation;
    uint64_t handle = 44444;
    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    uint64_t ptrAddress = 0x6000;
    uint8_t type = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory);

    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(&mockAllocation, handle, opaqueIpcData, ptrAddress, type, nullptr, L0::IpcHandleType::fdHandle);

    auto &ipcHandleMap = driverHandle->getIPCHandleMap();
    EXPECT_EQ(1u, ipcHandleMap.size());

    auto handleIterator = ipcHandleMap.find(handle);
    ASSERT_NE(handleIterator, ipcHandleMap.end());

    int fdValue = 0;
    memcpy(&fdValue, &handleIterator->second->opaqueData.handle.fd, sizeof(fdValue));
    EXPECT_EQ(handle, static_cast<uint64_t>(fdValue));

    delete handleIterator->second;
    driverHandle->getIPCHandleMap().clear();
    driverHandle->shutdownIpcSocketServer();
}

} // namespace ult
} // namespace L0
