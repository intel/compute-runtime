/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/context/context_imp.h"
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
    // Enable pidfd/sockets for IPC
    context->contextSettings.enablePidfdOrSockets = true;

    // Set up DRM driver model to use pidfd method
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
    // Enable pidfd/sockets for IPC
    context->contextSettings.enablePidfdOrSockets = true;

    // Set up DRM driver model to use pidfd method
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
    // Enable pidfd/sockets for IPC
    context->contextSettings.enablePidfdOrSockets = true;

    // Set up DRM driver model to use pidfd method
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
    // Enable pidfd/sockets for IPC
    context->contextSettings.enablePidfdOrSockets = true;

    // Set up DRM driver model to use pidfd method
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

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1, NEO::SysCalls::pidfdopenCalled);
    EXPECT_EQ(1, NEO::SysCalls::pidfdgetfdCalled);
    EXPECT_TRUE(pidfdGetFdCalled);
}

using GetDataFromIpcHandleTest = Test<GetMemHandlePtrTestFixture>;

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithOpaqueHandleEnabledThenOpaqueDataIsExtracted) {
    // Enable opaque handles
    context->contextSettings.enablePidfdOrSockets = true;

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcOpaqueMemoryData *opaqueData = reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    opaqueData->handle.fd = 123;
    opaqueData->memoryType = 42;
    opaqueData->processId = 456;
    opaqueData->poolOffset = 789;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int processId = 0;
    uint64_t poolOffset = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, processId, poolOffset);

    EXPECT_EQ(123u, handle);
    EXPECT_EQ(42u, type);
    EXPECT_EQ(456u, processId);
    EXPECT_EQ(789u, poolOffset);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithOpaqueHandleDisabledThenRegularDataIsExtracted) {
    // Disable opaque handles
    context->contextSettings.enablePidfdOrSockets = false;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcMemoryData *regularData = reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    regularData->handle = 987;
    regularData->type = 65;
    regularData->poolOffset = 321;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int processId = 0;
    uint64_t poolOffset = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, processId, poolOffset);

    EXPECT_EQ(987u, handle);
    EXPECT_EQ(65u, type);
    EXPECT_EQ(321u, poolOffset);
    // processId should remain 0 for regular data
    EXPECT_EQ(0u, processId);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithWDDMDriverThenOpaqueHandleIsForced) {
    // Initially disable opaque handles
    context->contextSettings.enablePidfdOrSockets = false;
    // Set WDDM driver model which should force opaque handles
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcOpaqueMemoryData *opaqueData = reinterpret_cast<IpcOpaqueMemoryData *>(ipcHandle.data);
    opaqueData->handle.fd = 555;
    opaqueData->memoryType = 77;
    opaqueData->processId = 888;
    opaqueData->poolOffset = 999;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int processId = 0;
    uint64_t poolOffset = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, processId, poolOffset);

    // Should extract opaque data even though enablePidfdOrSockets was false
    EXPECT_EQ(555u, handle);
    EXPECT_EQ(77u, type);
    EXPECT_EQ(888u, processId);
    EXPECT_EQ(999u, poolOffset);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithNullOSInterfaceThenRegularDataIsExtracted) {
    // Disable opaque handles and set null osInterface
    context->contextSettings.enablePidfdOrSockets = false;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset();

    ze_ipc_mem_handle_t ipcHandle = {};
    IpcMemoryData *regularData = reinterpret_cast<IpcMemoryData *>(ipcHandle.data);
    regularData->handle = 111;
    regularData->type = 22;
    regularData->poolOffset = 333;

    uint64_t handle = 0;
    uint8_t type = 0;
    unsigned int processId = 0;
    uint64_t poolOffset = 0;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, processId, poolOffset);

    EXPECT_EQ(111u, handle);
    EXPECT_EQ(22u, type);
    EXPECT_EQ(333u, poolOffset);
    EXPECT_EQ(0u, processId);
}

TEST_F(GetDataFromIpcHandleTest, whenCallingGetDataFromIpcHandleWithZeroValuesThenZeroValuesAreExtracted) {
    context->contextSettings.enablePidfdOrSockets = true;

    ze_ipc_mem_handle_t ipcHandle = {};
    // All values should be zero by default

    uint64_t handle = 999; // Initialize with non-zero to verify it gets overwritten
    uint8_t type = 99;
    unsigned int processId = 888;
    uint64_t poolOffset = 777;

    context->getDataFromIpcHandle(device, ipcHandle, handle, type, processId, poolOffset);

    EXPECT_EQ(0u, handle);
    EXPECT_EQ(0u, type);
    EXPECT_EQ(0u, processId);
    EXPECT_EQ(0u, poolOffset);
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

    // Disable opaque handles in settings (should be overridden by WDDM)
    context->contextSettings.enablePidfdOrSockets = false;

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::ntHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledThenTrueIsReturnedAndHandleTypeIsFd) {
    // Set DRM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> sysCallsPrctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    // Enable opaque handles in settings
    context->contextSettings.enablePidfdOrSockets = true;

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueDisabledThenFalseIsReturnedAndHandleTypeIsFd) {
    // Set DRM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> sysCallsPrctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    // Disable opaque handles in settings
    context->contextSettings.enablePidfdOrSockets = false;

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithNullOSInterfaceThenSettingsValueIsReturnedAndHandleTypeIsFd) {
    // Set null OS interface (unknown driver model) on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset();

    // Test with opaque handles enabled
    context->contextSettings.enablePidfdOrSockets = true;
    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);

    // Test with opaque handles disabled
    context->contextSettings.enablePidfdOrSockets = false;
    handleType = IpcHandleType::maxHandle;
    result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithUnknownDriverModelThenSettingsValueIsReturnedAndHandleTypeIsFd) {
    // Set mock driver model (unknown type) on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModel>());

    // Test with opaque handles enabled
    context->contextSettings.enablePidfdOrSockets = true;
    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);

    // Test with opaque handles disabled
    context->contextSettings.enablePidfdOrSockets = false;
    handleType = IpcHandleType::maxHandle;
    result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedMultipleTimesWithSameConfigurationThenConsistentResultsAreReturned) {
    // Set WDDM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    context->contextSettings.enablePidfdOrSockets = false;

    // Call multiple times and verify consistent results
    for (int i = 0; i < 3; i++) {
        IpcHandleType handleType = IpcHandleType::maxHandle;
        bool result = context->isOpaqueHandleSupported(&handleType);

        EXPECT_TRUE(result);
        EXPECT_EQ(IpcHandleType::ntHandle, handleType);
    }
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledThenPrctlIsCalledSuccessfully) {
    // Set DRM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    VariableBackup<decltype(NEO::SysCalls::sysCallsPrctl)> sysCallsPrctlBackup{&NEO::SysCalls::sysCallsPrctl, mockPrctl};

    // Enable opaque handles in settings (this will trigger the prctl call)
    context->contextSettings.enablePidfdOrSockets = true;

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_TRUE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);
}

TEST_F(IsOpaqueHandleSupportedTest, whenCallingIsOpaqueHandleSupportedWithDRMDriverAndOpaqueEnabledAndPrctlFailsThenFalseIsReturnedAndHandleTypeIsFd) {
    // Set DRM driver model on the execution environment that the context will actually use
    auto &executionEnvironment = driverHandle->getMemoryManager()->peekExecutionEnvironment();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());

    // Enable opaque handles in settings (this will trigger the prctl call)
    context->contextSettings.enablePidfdOrSockets = true;

    // Save original sysCallsPrctl and override to simulate failure
    auto originalSysCallsPrctl = NEO::SysCalls::sysCallsPrctl;
    NEO::SysCalls::sysCallsPrctl = [](int, unsigned long) -> int {
        return -1; // Simulate failure
    };

    IpcHandleType handleType = IpcHandleType::maxHandle;
    bool result = context->isOpaqueHandleSupported(&handleType);

    EXPECT_FALSE(result);
    EXPECT_EQ(IpcHandleType::fdHandle, handleType);

    // Restore original sysCallsPrctl
    NEO::SysCalls::sysCallsPrctl = originalSysCallsPrctl;
}

} // namespace ult
} // namespace L0
