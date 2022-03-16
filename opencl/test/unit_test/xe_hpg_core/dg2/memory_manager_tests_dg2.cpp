/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenLinearStreamTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenLinearStreamWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenKernelIsaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenInternalHeapTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenRingBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenSemaphoreBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontUseSystemMemoryAndRequireCpuAccess, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenGpuTimestampDeviceBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenLinearStreamAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenConstantSurfaceAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess, IGFX_DG2);

using MemoryManagerTestsDg2 = ::testing::Test;

DG2TEST_F(MemoryManagerTestsDg2, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInPreferredPoolThenLocalMemoryPoolIsNotUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::LINEAR_STREAM, mockDeviceBitfield}, nullptr);
    EXPECT_NE(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

DG2TEST_F(MemoryManagerTestsDg2, givenLinearStreamTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenLinearStreamWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenEnabledLocalMemoryWhenAllocateInternalHeapInSystemPoolThenLocalMemoryPoolIsNotUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::KERNEL_ISA, mockDeviceBitfield}, nullptr);
    EXPECT_NE(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

DG2TEST_F(MemoryManagerTestsDg2, givenKernelIsaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::KERNEL_ISA, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

DG2TEST_F(MemoryManagerTestsDg2, givenRingBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 0x10000u, AllocationType::RING_BUFFER, 1};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x10000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenSemaphoreBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 0x1000u, AllocationType::SEMAPHORE_BUFFER, 1};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x1000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenConstantSurfaceTypeWhenGetAllocationDataIsCalledThenLocalMemoryIsRequestedWithoutCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::CONSTANT_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenPrintfAllocationWhenGetAllocationDataIsCalledThenUseSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PRINTF_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

DG2TEST_F(MemoryManagerTestsDg2, givenGpuTimestampTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}
