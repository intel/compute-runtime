/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest = testing::TestWithParam<AllocationType>;

using MemoryManagerGetAlloctionDataTests = ::testing::Test;

TEST_F(MemoryManagerGetAlloctionDataTests, givenHostMemoryAllocationTypeAndAllocateMemoryFlagAndNullptrWhenAllocationDataIsQueriedThenCorrectFlagsAndSizeAreSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::bufferHostMemory, false, mockDeviceBitfield);
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenNonHostMemoryAllocatoinTypeWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsNotSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::buffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenForceSystemMemoryFlagWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsSet) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::unknown);
    auto lastAllocationIdx = static_cast<int>(AllocationType::count);

    for (int allocationIdx = firstAllocationIdx + 1; allocationIdx != lastAllocationIdx; allocationIdx++) {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, static_cast<AllocationType>(allocationIdx), false, mockDeviceBitfield);
        properties.flags.forceSystemMemory = true;
        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.useSystemMemory);
    }
}

HWTEST_F(MemoryManagerGetAlloctionDataTests, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenAllocateMemoryFlagTrueWhenHostPtrIsNotNullThenAllocationDataHasHostPtrNulled) {
    AllocationData allocData;
    char memory = 0;
    AllocationProperties properties(mockRootDeviceIndex, true, sizeof(memory), AllocationType::buffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_EQ(sizeof(memory), allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::buffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferHostMemoryTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::bufferHostMemory, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferCompressedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.preferCompressed = true;

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenWriteCombinedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::writeCombined, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenDefaultAllocationFlagsWhenAllocationDataIsQueriedThenAllocateMemoryIsFalse) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, false, 0, AllocationType::buffer, false, mockDeviceBitfield);
    properties.flags.preferCompressed = true;
    char memory;
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.allocateMemory);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenDebugModeWhenCertainAllocationTypesAreSelectedThenSystemPlacementIsChoosen) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::buffer;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    debugManager.flags.ForceSystemMemoryPlacement.set(mask);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    allocData.flags.useSystemMemory = false;

    MockMemoryManager::overrideAllocationData(allocData, properties);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;
    allocationType = AllocationType::writeCombined;
    mask |= 1llu << (static_cast<int64_t>(allocationType) - 1);
    debugManager.flags.ForceSystemMemoryPlacement.set(mask);

    AllocationProperties properties2(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocData, properties2);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;

    MockMemoryManager::overrideAllocationData(allocData, properties);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;
    allocationType = AllocationType::image;
    mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    debugManager.flags.ForceSystemMemoryPlacement.set(mask);

    MockMemoryManager::overrideAllocationData(allocData, properties);
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest, givenAllocationTypesWith32BitAnd64kbPagesAllowedWhenAllocationDataIsQueriedThenProperFlagsAreSet) {
    AllocationData allocData;
    auto allocType = GetParam();
    AllocationProperties properties(mockRootDeviceIndex, 0, allocType, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.mockExecutionEnvironment->initGmm();
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.allow32Bit);
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocType, allocData.type);
}

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest, given64kbAllowedAllocationTypeWhenAllocatingThenPreferCompressionOnlyForSpecificTypes) {
    auto allocType = GetParam();
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, allocType, mockDeviceBitfield);

    bool bufferCompressedType = (allocType == AllocationType::buffer);

    properties.flags.preferCompressed = bufferCompressedType;

    MockMemoryManager mockMemoryManager(true, false);
    mockMemoryManager.mockExecutionEnvironment->initGmm();
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    auto allocation = mockMemoryManager.allocateGraphicsMemory(allocData);

    EXPECT_TRUE(mockMemoryManager.allocation64kbPageCreated);
    EXPECT_EQ(mockMemoryManager.preferCompressedFlagPassed, bufferCompressedType);

    mockMemoryManager.freeGraphicsMemory(allocation);
}

using MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest = MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest;

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest, givenAllocationTypesWith32BitAnd64kbPagesDisallowedWhenAllocationDataIsQueriedThenFlagsAreNotSet) {
    AllocationData allocData;
    auto allocType = GetParam();
    AllocationProperties properties(mockRootDeviceIndex, 0, allocType, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocType, allocData.type);
}

static const AllocationType allocationTypesWith32BitAnd64KbPagesAllowed[] = {AllocationType::buffer,
                                                                             AllocationType::bufferHostMemory,
                                                                             AllocationType::scratchSurface,
                                                                             AllocationType::workPartitionSurface,
                                                                             AllocationType::privateSurface,
                                                                             AllocationType::printfSurface,
                                                                             AllocationType::constantSurface,
                                                                             AllocationType::globalSurface,
                                                                             AllocationType::writeCombined,
                                                                             AllocationType::assertBuffer};

INSTANTIATE_TEST_SUITE_P(Allow32BitAnd64kbPagesTypes,
                         MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest,
                         ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesAllowed));

static const AllocationType allocationTypesWith32BitAnd64KbPagesNotAllowed[] = {AllocationType::commandBuffer,
                                                                                AllocationType::timestampPacketTagBuffer,
                                                                                AllocationType::profilingTagBuffer,
                                                                                AllocationType::image,
                                                                                AllocationType::instructionHeap,
                                                                                AllocationType::sharedResourceCopy};

INSTANTIATE_TEST_SUITE_P(Disallow32BitAnd64kbPagesTypes,
                         MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest,
                         ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesNotAllowed));

TEST(MemoryManagerTest, givenForced32BitSetWhenGraphicsMemoryFor32BitAllowedTypeIsAllocatedThen32BitAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    if constexpr (is64bit) {
        EXPECT_TRUE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    } else {
        EXPECT_FALSE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    }

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledShareableWhenGraphicsAllocationIsAllocatedThenAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.shareable = true;

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.flags.shareable, 1u);

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledShareableWhenGraphicsAllocationIsCalledAndSystemMemoryFailsThenNullAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.shareable = true;

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.flags.shareable, 1u);

    memoryManager.failAllocateSystemMemory = true;
    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_EQ(nullptr, allocation);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenForced32BitEnabledWhenGraphicsMemoryWihtoutAllow32BitFlagIsAllocatedThenNon32BitAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    allocData.flags.allow32Bit = false;

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->is32BitAllocation());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenForced32BitDisabledWhenGraphicsMemoryWith32BitFlagFor32BitAllowedTypeIsAllocatedThenNon32BitAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.setForce32BitAllocations(false);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->is32BitAllocation());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryMustBeHostMemoryAndIsAllocatedWithNullptrForBufferThen64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::bufferHostMemory, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & MemoryConstants::page64kMask);
    EXPECT_EQ(0u, allocation->getGpuAddress() & MemoryConstants::page64kMask);
    EXPECT_EQ(0u, allocation->getUnderlyingBufferSize() & MemoryConstants::page64kMask);
    EXPECT_EQ(MemoryPool::system64KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryWithoutAllow64kbPagesFlagsIsAllocatedThenNon64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    allocData.flags.allow64kbPages = false;

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocation64kbPageCreated);
    EXPECT_TRUE(memoryManager.allocationCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryWith128kbAlignmentCreatedThen64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    allocData.flags.allow64kbPages = true;
    allocData.alignment = 2 * MemoryConstants::pageSize64k;

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    EXPECT_TRUE(memoryManager.allocation64kbPageCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryWith32kbAlignmentCreatedThen64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::buffer, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    allocData.flags.allow64kbPages = true;
    allocData.alignment = MemoryConstants::pageSize64k / 2;

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    EXPECT_TRUE(memoryManager.allocation64kbPageCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenDisabled64kbPagesWhenGraphicsMemoryMustBeHostMemoryAndIsAllocatedWithNullptrForBufferThenNon64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::bufferHostMemory, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocation64kbPageCreated);
    EXPECT_TRUE(memoryManager.allocationCreated);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenForced32BitAndEnabled64kbPagesWhenGraphicsMemoryMustBeHostMemoryAndIsAllocatedWithNullptrForBufferThen32BitAllocationOver64kbIsChosen) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::bufferHostMemory, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    if constexpr (is64bit) {
        EXPECT_TRUE(allocation->is32BitAllocation());
    } else {
        EXPECT_FALSE(allocation->is32BitAllocation());
    }

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryIsAllocatedWithHostPtrForBufferThenExistingMemoryIsUsedForAllocation) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, false, 1, AllocationType::bufferHostMemory, false, mockDeviceBitfield);

    char memory[1];
    memoryManager.getAllocationData(allocData, properties, &memory, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(is32bit, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGraphicsMemoryAllocationInDevicePoolFailsThenFallbackAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocationCreated);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedThenAllocateGraphicsMemoryInPreferredPoolCanAllocateInDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedAndAllocateInDevicePoolFailsWithErrorThenAllocateGraphicsMemoryInPreferredPoolReturnsNullptr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePoolWithError = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield});
    ASSERT_EQ(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThenAllocatingMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::svmZeroCopy, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allocateMemory);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThen64kbPagesAreAllowedAnd32BitAllocationIsDisallowed) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::svmZeroCopy, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    EXPECT_FALSE(allocData.flags.allow32Bit);
}

TEST(MemoryManagerTest, givenTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::tagBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenHostFunctionTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::hostFunction, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalFenceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::globalFence, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::preemption, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenSyncTokenTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequestedAndAllow64kPages) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::syncDispatchToken, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allow64kbPages);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThen64kbPagesAllowed) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::preemption, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
}

TEST(MemoryManagerTest, givenAllocationTypeWhenGetAllocationDataIsCalledThen48BitResourceIsTrue) {
    MockMemoryManager mockMemoryManager;

    for (auto &type : {AllocationType::preemption, AllocationType::deferredTasksList, AllocationType::syncDispatchToken}) {
        AllocationData allocData;
        AllocationProperties properties{mockRootDeviceIndex, 1, type, mockDeviceBitfield};
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
        EXPECT_TRUE(allocData.flags.resource48Bit);
    }
}

TEST(MemoryManagerTest, givenMCSTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::mcs, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::globalSurface, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenWriteCombinedTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::writeCombined, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenInternalHostMemoryTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::internalHostMemory, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenFillPatternTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::fillPattern, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

using GetAllocationDataTestHw = ::testing::Test;

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::linearStream, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenTimestampPacketTagBufferTypeWhenGetAllocationDataIsCalledThenLocalMemoryIsRequestedAndRequireCpuAccess) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForceLocalMemoryAccessMode.set(0u);
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::timestampPacketTagBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenProfilingTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::profilingTagBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagEnabledWhenAllocateMemoryThenAllocationDataIsMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield};
    properties.flags.multiOsContextCapable = true;

    AllocationData allocData;
    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagDisabledWhenAllocateMemoryThenAllocationDataIsNotMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield};
    properties.flags.multiOsContextCapable = false;

    AllocationData allocData;
    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenConstantSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::constantSurface, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

HWTEST_F(GetAllocationDataTestHw, givenInternalHeapTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::internalHeap, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenGpuTimestampDeviceBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::gpuTimestampDeviceBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}
HWTEST_F(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::gpuTimestampDeviceBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::linearStream, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenConstantSurfaceAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::constantSurface, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}
HWTEST_F(GetAllocationDataTestHw, givenKernelIsaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::kernelIsa, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_NE(defaultHwInfo->featureTable.flags.ftrLocalMemory, allocData.flags.useSystemMemory);

    AllocationProperties properties2{mockRootDeviceIndex, 1, AllocationType::kernelIsaInternal, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_NE(defaultHwInfo->featureTable.flags.ftrLocalMemory, allocData.flags.useSystemMemory);
}

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::linearStream, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontUseSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::printfSurface, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenAssertAllocationWhenGetAllocationDataIsCalledThenDontUseSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::assertBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenExternalHostMemoryWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, false, 1, AllocationType::externalHostPtr, false, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, hostPtr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocData.hostPtr, hostPtr);
}

TEST(MemoryManagerTest, GivenAllocationPropertiesWhenGettingAllocationDataThenSameRootDeviceIndexIsUsed) {
    const uint32_t rootDevicesCount = 100u;

    AllocationData allocData;
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), true, rootDevicesCount};
    MockMemoryManager mockMemoryManager{executionEnvironment};
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, 0u);

    AllocationProperties properties2{rootDevicesCount - 1, 1, AllocationType::buffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, properties2.rootDeviceIndex);
}

TEST(MemoryManagerTest, givenMapAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, false, 1, AllocationType::mapAllocation, false, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, hostPtr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocData.hostPtr, hostPtr);
}

HWTEST_F(GetAllocationDataTestHw, givenRingBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 0x10000u, AllocationType::ringBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x10000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenSemaphoreBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 0x1000u, AllocationType::semaphoreBuffer, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x1000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenDefaultIsUsedThenExpectNoFlagsChanged) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionBufferPlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionBufferPlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenDefaultIsUsedThenExpectNoFlagsChanged) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionSemaphorePlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionSemaphorePlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideToNo48BitThenExpect48BitFlagFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionBufferAddressing.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionBufferAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::ringBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);

    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingWhenOverrideToNo48BitThenExpect48BitFlagFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionSemaphoreAddressing.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DirectSubmissionSemaphoreAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::semaphoreBuffer, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);

    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenForceNonSystemMaskWhenAllocationTypeMatchesMaskThenExpectSystemFlagFalse) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::buffer;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    debugManager.flags.ForceNonSystemMemoryPlacement.set(mask);

    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::buffer, mockDeviceBitfield);
    allocationData.flags.useSystemMemory = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenForceNonSystemMaskWhenAllocationTypeNotMatchesMaskThenExpectSystemFlagTrue) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::buffer;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    debugManager.flags.ForceNonSystemMemoryPlacement.set(mask);

    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::commandBuffer, mockDeviceBitfield);
    allocationData.flags.useSystemMemory = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDebugContextSaveAreaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::debugContextSaveArea, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.zeroMemory);
}

TEST(MemoryManagerTest, givenAllocationTypeConstantOrGlobalSurfaceWhenGetAllocationDataIsCalledThenZeroMemoryFlagIsSet) {
    MockMemoryManager mockMemoryManager;
    AllocationProperties propertiesGlobal{mockRootDeviceIndex, 1, AllocationType::globalSurface, mockDeviceBitfield};
    AllocationProperties propertiesConstant{mockRootDeviceIndex, 1, AllocationType::constantSurface, mockDeviceBitfield};
    {
        AllocationData allocData;
        mockMemoryManager.getAllocationData(allocData, propertiesGlobal, nullptr, mockMemoryManager.createStorageInfoFromProperties(propertiesGlobal));
        EXPECT_TRUE(allocData.flags.zeroMemory);
    }
    {
        AllocationData allocData;
        mockMemoryManager.getAllocationData(allocData, propertiesConstant, nullptr, mockMemoryManager.createStorageInfoFromProperties(propertiesConstant));
        EXPECT_TRUE(allocData.flags.zeroMemory);
    }
}

TEST(MemoryManagerTest, givenPropertiesWithOsContextWhenGetAllocationDataIsCalledThenOsContextIsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, AllocationType::debugContextSaveArea, mockDeviceBitfield};

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    MockOsContext osContext(0u, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0])[0]));

    properties.osContext = &osContext;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(&osContext, allocData.osContext);
}

TEST(MemoryManagerTest, givenPropertiesWithGpuAddressWhenGetAllocationDataIsCalledThenGpuAddressIsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::debugContextSaveArea, mockDeviceBitfield};

    properties.gpuAddress = 0x4000;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(properties.gpuAddress, allocData.gpuAddress);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationTypeAndPlatrormSupportReadOnlyAllocationAndBliterTransferNotRequiredThenAllocationIsSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 1u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationTypeAndSupportReadOnlyButPlatformDoesNotAndBliterTransferNotRequiredThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = false;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationTypeAndPlatrormSupportReadOnlyAllocationAndBliterTransferRequiredThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = true;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationTypeDoesNotSupportReadOnlyButPlatformDoesAndBliterTransferNotRequiredThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;

    mockGa.hasAllocationReadOnlyTypeResult = false;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationIsCommandBufferAndItIsSetAsNotReadOnlyThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;
    mockGa.setAllocationType(AllocationType::commandBuffer);

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;
    AllocationProperties properties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.cantBeReadOnly = true;
    properties.flags.multiOsContextCapable = false;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(properties,
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationIsCommandBufferAndMultiContextCapableIsTrueThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;
    mockGa.setAllocationType(AllocationType::commandBuffer);

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    AllocationProperties properties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::commandBuffer, mockDeviceBitfield);
    properties.flags.cantBeReadOnly = false;
    properties.flags.multiOsContextCapable = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(properties,
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenSingleAddressSpaceSbaTrackingWhenAllocationIsCommandBufferAndMultiContextCapableIsFalseThenAllocationIsNotSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.setDebuggingMode(NEO::DebuggingMode::offline);
    auto debugger = new MockDebugger;
    debugger->singleAddressSpaceSbaTracking = true;
    executionEnvironment.rootDeviceEnvironments[0]->debugger.reset(debugger);

    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;
    mockGa.setAllocationType(AllocationType::commandBuffer);

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    AllocationProperties properties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::commandBuffer, mockDeviceBitfield);
    properties.flags.cantBeReadOnly = false;
    properties.flags.multiOsContextCapable = false;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(properties,
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 0u);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationTypeAndPlatrormSupportReadOnlyAllocationBliterAndAllocationTypeOtherThanCmdBufferTransferNotRequiredThenAllocationIsSetAsReadOnly) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->supportReadOnlyAllocationsResult = true;
    std::unique_ptr<ProductHelper> productHelper = std::move(mockProductHelper);
    std::swap(executionEnvironment.rootDeviceEnvironments[0]->productHelper, productHelper);
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    MockGraphicsAllocation mockGa;
    mockGa.setAllocationType(AllocationType::buffer);

    mockGa.hasAllocationReadOnlyTypeResult = true;

    memoryManager.mockGa = &mockGa;
    memoryManager.returnMockGAFromDevicePool = true;

    AllocationProperties properties(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.cantBeReadOnly = true;
    properties.flags.multiOsContextCapable = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(properties,
                                                                          nullptr);
    EXPECT_EQ(allocation, &mockGa);
    EXPECT_EQ(mockGa.setAsReadOnlyCalled, 1u);
}

TEST(MemoryManagerTest, givenEnableLocalMemoryAndMemoryManagerWhenBufferTypeIsPassedThenAllocateGraphicsMemoryInPreferredPool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

using MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest = testing::TestWithParam<std::tuple<AllocationType, bool>>;

TEST_P(MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest, givenAllocationTypesHaveToBeForcedTo48BitThenAllocationDataResource48BitIsSet) {
    AllocationType allocationType;
    bool propertiesFlag48Bit;

    std::tie(allocationType, propertiesFlag48Bit) = GetParam();

    AllocationProperties properties(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    properties.flags.resource48Bit = propertiesFlag48Bit;

    AllocationData allocationData;
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocationData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocationData.flags.resource48Bit);
}

using MemoryManagerGetAlloctionDataOptionallyForcedTo48BitTest = testing::TestWithParam<std::tuple<AllocationType, bool>>;

TEST_P(MemoryManagerGetAlloctionDataOptionallyForcedTo48BitTest, givenAllocationTypesOptionalFor48BitThenAllocationDataResource48BitIsSet) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    AllocationType allocationType;
    bool propertiesFlag48Bit;

    std::tie(allocationType, propertiesFlag48Bit) = GetParam();

    AllocationProperties properties(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    properties.flags.resource48Bit = propertiesFlag48Bit;

    AllocationData allocationData;
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocationData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_EQ(gfxCoreHelper.is48ResourceNeededForCmdBuffer(), allocationData.flags.resource48Bit);
}

using MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest = testing::TestWithParam<std::tuple<AllocationType, bool>>;

TEST_P(MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest, givenAllocationTypesHaveNotToBeForcedTo48BitThenAllocationDataResource48BitIsSetProperly) {
    AllocationType allocationType;
    bool propertiesFlag48Bit;

    std::tie(allocationType, propertiesFlag48Bit) = GetParam();

    AllocationProperties properties(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    properties.flags.resource48Bit = propertiesFlag48Bit;

    AllocationData allocationData;
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocationData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocationData.flags.resource48Bit, propertiesFlag48Bit);
}

static const AllocationType allocationHaveToBeForcedTo48Bit[] = {
    AllocationType::image,
    AllocationType::indirectObjectHeap,
    AllocationType::instructionHeap,
    AllocationType::internalHeap,
    AllocationType::kernelIsa,
    AllocationType::linearStream,
    AllocationType::mcs,
    AllocationType::scratchSurface,
    AllocationType::workPartitionSurface,
    AllocationType::sharedImage,
    AllocationType::sharedResourceCopy,
    AllocationType::surfaceStateHeap,
    AllocationType::timestampPacketTagBuffer,
    AllocationType::semaphoreBuffer,
    AllocationType::deferredTasksList,
    AllocationType::syncDispatchToken,
};

static const AllocationType allocationsOptionallyForcedTo48Bit[] = {
    AllocationType::commandBuffer,
    AllocationType::ringBuffer,
};

static const AllocationType allocationHaveNotToBeForcedTo48Bit[] = {
    AllocationType::buffer,
    AllocationType::bufferHostMemory,
    AllocationType::constantSurface,
    AllocationType::externalHostPtr,
    AllocationType::fillPattern,
    AllocationType::globalSurface,
    AllocationType::internalHostMemory,
    AllocationType::mapAllocation,
    AllocationType::printfSurface,
    AllocationType::privateSurface,
    AllocationType::profilingTagBuffer,
    AllocationType::sharedBuffer,
    AllocationType::svmCpu,
    AllocationType::svmGpu,
    AllocationType::svmZeroCopy,
    AllocationType::syncBuffer,
    AllocationType::tagBuffer,
    AllocationType::globalFence,
    AllocationType::writeCombined,
    AllocationType::debugContextSaveArea,
};

INSTANTIATE_TEST_SUITE_P(ForceTo48Bit,
                         MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest,
                         ::testing::Combine(
                             ::testing::ValuesIn(allocationHaveToBeForcedTo48Bit),
                             ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(NotForceTo48Bit,
                         MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest,
                         ::testing::Combine(
                             ::testing::ValuesIn(allocationHaveNotToBeForcedTo48Bit),
                             ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(OptionallyForceTo48Bit,
                         MemoryManagerGetAlloctionDataOptionallyForcedTo48BitTest,
                         ::testing::Combine(
                             ::testing::ValuesIn(allocationsOptionallyForcedTo48Bit),
                             ::testing::Bool()));
