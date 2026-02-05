/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/memory_ipc_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/ze_api.h"

namespace L0 {
namespace ult {

using MemoryIpcHandlePropertiesTest = MemoryExportImportTest;

// Windows minimal coverage tests
// Note: Windows does not support shareable allocations via createInternalHandle/peekInternalHandle
// These tests provide minimal coverage for the decision points without expecting valid handles

TEST_F(MemoryIpcHandlePropertiesTest,
       givenFabricAccessibleHandleTypeWhenGettingIpcMemHandleThenApiSucceeds) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableShareableWithoutNTHandle.set(true);
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    // Create extended descriptor for fabric accessible handle
    ze_ipc_mem_handle_type_ext_desc_t handleTypeDesc = {};
    handleTypeDesc.stype = ZE_STRUCTURE_TYPE_IPC_MEM_HANDLE_TYPE_EXT_DESC;
    handleTypeDesc.pNext = nullptr;
    handleTypeDesc.typeFlags = ZE_IPC_MEM_HANDLE_TYPE_FLAG_FABRIC_ACCESSIBLE;

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &handleTypeDesc, &ipcHandle);
    // API call succeeds even though Windows allocations don't support shareable handles
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIpcHandlePropertiesTest,
       givenInvalidStructureTypeWhenGettingIpcMemHandleThenInvalidArgumentIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    // Create extended descriptor with invalid structure type
    ze_ipc_mem_handle_type_ext_desc_t handleTypeDesc = {};
    handleTypeDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES; // Wrong type
    handleTypeDesc.pNext = nullptr;
    handleTypeDesc.typeFlags = ZE_IPC_MEM_HANDLE_TYPE_FLAG_FABRIC_ACCESSIBLE;

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &handleTypeDesc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(MemoryIpcHandlePropertiesTest,
       givenNullPNextWhenGettingIpcMemHandleThenSuccessIsReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    // Call with nullptr pNext (default behavior)
    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, nullptr, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
