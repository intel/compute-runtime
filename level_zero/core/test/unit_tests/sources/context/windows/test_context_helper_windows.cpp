/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"
namespace L0 {
namespace ult {

using ContextGetVirtualAddressSpaceTests = Test<DeviceFixture>;
TEST_F(ContextGetVirtualAddressSpaceTests, givenWddmDriverModelWhenCallingGetVirtualAddressSpaceIpcHandleThenUnsupportedErrorIsReturned) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_ipc_mem_handle_t ipcHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, contextImp->getVirtualAddressSpaceIpcHandle(device, &ipcHandle));
    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextGetVirtualAddressSpaceTests, givenWddmDriverModelWhenCallingPutVirtualAddressSpaceIpcHandleThenUnsupportedErrorIsReturned) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    ze_ipc_mem_handle_t ipcHandle{};
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, contextImp->putVirtualAddressSpaceIpcHandle(ipcHandle));
    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using ContextTestWindows = Test<DeviceFixture>;

HWTEST2_F(ContextTestWindows, whenCreatingContextWithSvmHeapDisabledThenContextSettingsSetCorrectly, IsMTL) {
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface());
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));
    EXPECT_FALSE(contextImp->contextSettings.enableSvmHeapReservation);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using ContextShareableMemoryTests = Test<DeviceFixture>;

TEST_F(ContextShareableMemoryTests, givenIntegratedDeviceAndEnableShareableWithoutNTHandleDisabledWhenCallingIsShareableMemoryThenReturnsFalse) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(0);

    // Set device as integrated
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    // Test with exportableMemory = false, shareableWithoutNTHandle = true
    bool result = contextImp->isShareableMemory(nullptr, false, neoDevice, true);
    EXPECT_FALSE(result);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextShareableMemoryTests, givenIntegratedDeviceAndEnableShareableWithoutNTHandleMinusOneWhenCallingIsShareableMemoryThenReturnsFalse) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(-1);

    // Set device as integrated
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    // Test with exportableMemory = false, shareableWithoutNTHandle = true
    bool result = contextImp->isShareableMemory(nullptr, false, neoDevice, true);
    EXPECT_FALSE(result);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextShareableMemoryTests, givenIntegratedDeviceAndEnableShareableWithoutNTHandleEnabledWhenCallingIsShareableMemoryThenReturnsTrue) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(1);

    // Set device as integrated
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    // Test with exportableMemory = false, shareableWithoutNTHandle = true
    bool result = contextImp->isShareableMemory(nullptr, false, neoDevice, true);
    EXPECT_TRUE(result);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextShareableMemoryTests, givenDiscreteDeviceWhenCallingIsShareableMemoryThenReturnsShareableWithoutNTHandleValue) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(0);

    // Set device as discrete (not integrated)
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    // Test with exportableMemory = false, shareableWithoutNTHandle = true -> should return true (discrete device bypasses flag check)
    bool result = contextImp->isShareableMemory(nullptr, false, neoDevice, true);
    EXPECT_TRUE(result);

    // Test with exportableMemory = false, shareableWithoutNTHandle = false -> should return false
    result = contextImp->isShareableMemory(nullptr, false, neoDevice, false);
    EXPECT_FALSE(result);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ContextShareableMemoryTests, givenExportableMemoryTrueWhenCallingIsShareableMemoryThenAlwaysReturnsTrue) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(0);

    // Set device as integrated
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    ContextImp *contextImp = static_cast<ContextImp *>(L0::Context::fromHandle(hContext));

    // Test with exportableMemory = true -> should always return true regardless of other parameters
    bool result = contextImp->isShareableMemory(nullptr, true, neoDevice, false);
    EXPECT_TRUE(result);

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0
