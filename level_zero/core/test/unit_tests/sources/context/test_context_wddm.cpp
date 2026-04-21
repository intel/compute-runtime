/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
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

    Context *contextImp = Context::fromHandle(L0::Context::fromHandle(hContext));

    bool exportableMemoryFalse = false;
    bool exportableMemoryTrue = true;
    EXPECT_EQ(exportableMemoryFalse, contextImp->isShareableMemory(nullptr, exportableMemoryFalse, neoDevice, false));
    EXPECT_EQ(exportableMemoryTrue, contextImp->isShareableMemory(nullptr, exportableMemoryTrue, neoDevice, false));

    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using GetMemHandlePtrTest = Test<GetMemHandlePtrTestFixture>;
TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithValidHandleThenSuccessIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    // Test Successfully returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_NE(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, false, 0u, 0, 0u, nullptr, false, false).second);
}

TEST_F(GetMemHandlePtrTest, whenCallingGetMemHandlePtrWithInvalidHandleThenNullptrIsReturned) {
    MemoryManagerMemHandleMock *fixtureMemoryManager = static_cast<MemoryManagerMemHandleMock *>(currMemoryManager);

    uint64_t handle = 57;

    driverHandle->failHandleLookup = true;

    // Test Failing returning NT Handle
    fixtureMemoryManager->ntHandle = true;
    EXPECT_EQ(nullptr, context->getMemHandlePtr(device, handle, NEO::AllocationType::buffer, false, 0u, 0, 0u, nullptr, false, false).second);
}

using ContextStaticIpcTest = Test<DeviceFixture>;
TEST_F(ContextStaticIpcTest, givenNullContextWhenCallingStaticIsIPCHandleSharingSupportedWithoutDebugFlagThenReturnsFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableShareableWithoutNTHandle.set(0);

    // Test that the static function can be called without a Context instance
    bool ipcSupported = Context::isIPCHandleSharingSupported();
    EXPECT_FALSE(ipcSupported);
}

TEST_F(ContextStaticIpcTest, givenNullContextWhenCallingStaticIsIPCHandleSharingSupportedWithDebugFlagThenReturnsTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableShareableWithoutNTHandle.set(1);

    // Test that the static function can be called without a Context instance
    bool ipcSupported = Context::isIPCHandleSharingSupported();
    EXPECT_TRUE(ipcSupported);
}

TEST_F(ContextStaticIpcTest, givenDriverHandleWhenCallingStaticIsIPCHandleSharingSupportedThenDriverCanQueryBeforeContextCreation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableShareableWithoutNTHandle.set(1);

    // Test that driver can query IPC support before creating any context on WDDM
    // This proves the static function allows the driver to report capabilities consistently
    bool ipcSupportedBeforeContext = Context::isIPCHandleSharingSupported();
    EXPECT_TRUE(ipcSupportedBeforeContext);

    // Now create a context and verify it returns the same value
    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    bool ipcSupportedAfterContext = Context::isIPCHandleSharingSupported();
    EXPECT_TRUE(ipcSupportedAfterContext);

    // Verify both calls return the same value (platform capability, not instance-specific)
    EXPECT_EQ(ipcSupportedBeforeContext, ipcSupportedAfterContext);

    Context *contextImp = Context::fromHandle(hContext);
    res = contextImp->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

} // namespace ult
} // namespace L0
