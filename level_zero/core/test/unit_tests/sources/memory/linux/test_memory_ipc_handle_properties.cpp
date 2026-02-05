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

TEST_F(MemoryIpcHandlePropertiesTest,
       givenFabricAccessibleHandleTypeWhenGettingIpcMemHandleThenIpcMemHandleReturnsSuccess) {
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
       givenNonFabricAccessibleHandleTypeWhenGettingIpcMemHandleThenReservedHandleDataIsNotUsed) {
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

    // Create extended descriptor without fabric accessible flag
    ze_ipc_mem_handle_type_ext_desc_t handleTypeDesc = {};
    handleTypeDesc.stype = ZE_STRUCTURE_TYPE_IPC_MEM_HANDLE_TYPE_EXT_DESC;
    handleTypeDesc.pNext = nullptr;
    handleTypeDesc.typeFlags = 0; // No fabric accessible flag

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, &handleTypeDesc, &ipcHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    // When fabric accessible flag is not set, reservedHandleData pointer is nullptr
    // The normal handle path is used - on Linux this creates a valid fd handle
    if (context->settings.useOpaqueHandle) {
        using IpcDataT = IpcOpaqueMemoryData;
        IpcDataT &ipcData = *reinterpret_cast<IpcDataT *>(ipcHandle.data);
        EXPECT_EQ(ipcData.reservedHandleData[0], 0u);
    }

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

// Mock allocation that fails createInternalHandle
class MockFailingGraphicsAllocation : public NEO::MockGraphicsAllocation {
  public:
    MockFailingGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : NEO::MockGraphicsAllocation(rootDeviceIndex, buffer, sizeIn) {}

    int createInternalHandle(NEO::MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        createInternalHandleCalled = true;
        return -1; // Return error
    }

    bool createInternalHandleCalled = false;
};

struct MemoryIpcHandleErrorPathTest : public MemoryExportImportTest {
    void SetUp() override {
        MemoryExportImportTest::SetUp();
    }
};

TEST_F(MemoryIpcHandleErrorPathTest,
       givenCreateInternalHandleFailsWhenGettingIpcMemHandleThenOutOfMemoryIsReturned) {

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_result_t result = context->allocDeviceMem(device->toHandle(),
                                                 &deviceDesc,
                                                 size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    // Get the allocation data
    NEO::SvmAllocationData *allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);

    // Replace the allocation with our mock that fails
    auto originalAllocation = allocData->gpuAllocations.getDefaultGraphicsAllocation();
    auto originalRootDeviceIndex = originalAllocation->getRootDeviceIndex();
    auto *mockAllocation = new MockFailingGraphicsAllocation(originalRootDeviceIndex, nullptr, size);
    mockAllocation->setGpuBaseAddress(originalAllocation->getGpuBaseAddress());
    allocData->gpuAllocations.removeAllocation(originalRootDeviceIndex);
    allocData->gpuAllocations.addAllocation(mockAllocation);

    ze_ipc_mem_handle_t ipcHandle = {};
    result = context->getIpcMemHandle(ptr, nullptr, &ipcHandle);

    // Should return out of host memory error
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    EXPECT_TRUE(mockAllocation->createInternalHandleCalled);

    // Restore original allocation before cleanup
    allocData->gpuAllocations.removeAllocation(originalRootDeviceIndex);
    allocData->gpuAllocations.addAllocation(originalAllocation);
    delete mockAllocation;

    result = context->freeMem(ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
