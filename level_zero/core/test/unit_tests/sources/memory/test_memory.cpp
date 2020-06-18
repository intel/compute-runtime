/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using MemoryTest = Test<DeviceFixture>;

TEST_F(MemoryTest, givenDevicePointerThenDriverGetAllocPropertiesReturnsDeviceHandle) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocDeviceMem(device->toHandle(),
                                                      ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    memoryProperties.version = ZE_MEMORY_ALLOCATION_PROPERTIES_VERSION_CURRENT;
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_DEVICE);
    EXPECT_EQ(deviceHandle, device->toHandle());

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

using DeviceMemorySizeTest = Test<DeviceFixture>;

TEST_F(DeviceMemorySizeTest, givenSizeGreaterThanLimitThenDeviceAllocationFails) {
    size_t size = neoDevice->getHardwareCapabilities().maxMemAllocSize + 1;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocDeviceMem(nullptr,
                                                      ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_SIZE, result);
}

TEST_F(MemoryTest, givenSharedPointerThenDriverGetAllocPropertiesReturnsDeviceHandle) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocSharedMem(device->toHandle(),
                                                      ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                                      ZE_HOST_MEM_ALLOC_FLAG_DEFAULT,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    memoryProperties.version = ZE_MEMORY_ALLOCATION_PROPERTIES_VERSION_CURRENT;
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_SHARED);
    EXPECT_EQ(deviceHandle, device->toHandle());

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenHostPointerThenDriverGetAllocPropertiesReturnsNullDevice) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_result_t result = driverHandle->allocHostMem(ZE_HOST_MEM_ALLOC_FLAG_DEFAULT,
                                                    size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    ze_memory_allocation_properties_t memoryProperties = {};
    memoryProperties.version = ZE_MEMORY_ALLOCATION_PROPERTIES_VERSION_CURRENT;
    ze_device_handle_t deviceHandle;

    result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_HOST);
    EXPECT_EQ(deviceHandle, nullptr);

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST_F(MemoryTest, givenSystemAllocatedPointerThenDriverGetAllocPropertiesReturnsUnknownType) {
    size_t size = 10;
    int *ptr = new int[size];

    ze_memory_allocation_properties_t memoryProperties = {};
    memoryProperties.version = ZE_MEMORY_ALLOCATION_PROPERTIES_VERSION_CURRENT;
    ze_device_handle_t deviceHandle;
    ze_result_t result = driverHandle->getMemAllocProperties(ptr, &memoryProperties, &deviceHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(memoryProperties.type, ZE_MEMORY_TYPE_UNKNOWN);

    delete[] ptr;
}

TEST_F(MemoryTest, givenSharedPointerAndDeviceHandleAsNullThenDriverReturnsSuccessAndReturnsPointerToSharedAllocation) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ASSERT_NE(nullptr, device->toHandle());
    ze_result_t result = driverHandle->allocSharedMem(nullptr,
                                                      ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                                      ZE_HOST_MEM_ALLOC_FLAG_DEFAULT,
                                                      size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = driverHandle->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
