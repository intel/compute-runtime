/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

using namespace NEO;

using MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest = testing::TestWithParam<AllocationType>;

using MemoryManagerGetAlloctionDataTests = ::testing::Test;

TEST_F(MemoryManagerGetAlloctionDataTests, givenHostMemoryAllocationTypeAndAllocateMemoryFlagAndNullptrWhenAllocationDataIsQueriedThenCorrectFlagsAndSizeAreSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER_HOST_MEMORY, false, mockDeviceBitfield);
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenNonHostMemoryAllocatoinTypeWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsNotSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenForceSystemMemoryFlagWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsSet) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::UNKNOWN);
    auto lastAllocationIdx = static_cast<int>(AllocationType::COUNT);

    for (int allocationIdx = firstAllocationIdx + 1; allocationIdx != lastAllocationIdx; allocationIdx++) {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, static_cast<AllocationType>(allocationIdx), false, mockDeviceBitfield);
        properties.flags.forceSystemMemory = true;
        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.useSystemMemory);
    }
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenMultiRootDeviceIndexAllocationPropertiesWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsSet) {
    {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);
        properties.flags.crossRootDeviceAccess = true;

        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.useSystemMemory);
        EXPECT_TRUE(allocData.flags.crossRootDeviceAccess);
    }

    {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::IMAGE, false, mockDeviceBitfield);
        properties.flags.crossRootDeviceAccess = true;

        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_TRUE(allocData.flags.useSystemMemory);
        EXPECT_TRUE(allocData.flags.crossRootDeviceAccess);
    }
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenDisabledCrossRootDeviceAccsessFlagInAllocationPropertiesWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsNotSet) {
    {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);
        properties.flags.crossRootDeviceAccess = false;

        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_FALSE(allocData.flags.useSystemMemory);
        EXPECT_FALSE(allocData.flags.crossRootDeviceAccess);
    }

    {
        AllocationData allocData;
        AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::IMAGE, false, mockDeviceBitfield);
        properties.flags.crossRootDeviceAccess = false;

        MockMemoryManager mockMemoryManager;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

        EXPECT_FALSE(allocData.flags.useSystemMemory);
        EXPECT_FALSE(allocData.flags.crossRootDeviceAccess);
    }
}

HWTEST_F(MemoryManagerGetAlloctionDataTests, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::COMMAND_BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenAllocateMemoryFlagTrueWhenHostPtrIsNotNullThenAllocationDataHasHostPtrNulled) {
    AllocationData allocData;
    char memory = 0;
    AllocationProperties properties(mockRootDeviceIndex, true, sizeof(memory), AllocationType::BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_EQ(sizeof(memory), allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferHostMemoryTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER_HOST_MEMORY, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenBufferCompressedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::BUFFER, false, mockDeviceBitfield);
    properties.flags.preferCompressed = true;

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenWriteCombinedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::WRITE_COMBINED, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenDefaultAllocationFlagsWhenAllocationDataIsQueriedThenAllocateMemoryIsFalse) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, false, 0, AllocationType::BUFFER, false, mockDeviceBitfield);
    properties.flags.preferCompressed = true;
    char memory;
    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.allocateMemory);
}

TEST_F(MemoryManagerGetAlloctionDataTests, givenDebugModeWhenCertainAllocationTypesAreSelectedThenSystemPlacementIsChoosen) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::BUFFER;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    DebugManager.flags.ForceSystemMemoryPlacement.set(mask);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    allocData.flags.useSystemMemory = false;

    MockMemoryManager::overrideAllocationData(allocData, properties);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;
    allocationType = AllocationType::WRITE_COMBINED;
    mask |= 1llu << (static_cast<int64_t>(allocationType) - 1);
    DebugManager.flags.ForceSystemMemoryPlacement.set(mask);

    AllocationProperties properties2(mockRootDeviceIndex, 0, allocationType, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocData, properties2);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;

    MockMemoryManager::overrideAllocationData(allocData, properties);
    EXPECT_TRUE(allocData.flags.useSystemMemory);

    allocData.flags.useSystemMemory = false;
    allocationType = AllocationType::IMAGE;
    mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    DebugManager.flags.ForceSystemMemoryPlacement.set(mask);

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

    bool bufferCompressedType = (allocType == AllocationType::BUFFER);

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

static const AllocationType allocationTypesWith32BitAnd64KbPagesAllowed[] = {AllocationType::BUFFER,
                                                                             AllocationType::BUFFER_HOST_MEMORY,
                                                                             AllocationType::PIPE,
                                                                             AllocationType::SCRATCH_SURFACE,
                                                                             AllocationType::WORK_PARTITION_SURFACE,
                                                                             AllocationType::PRIVATE_SURFACE,
                                                                             AllocationType::PRINTF_SURFACE,
                                                                             AllocationType::CONSTANT_SURFACE,
                                                                             AllocationType::GLOBAL_SURFACE,
                                                                             AllocationType::WRITE_COMBINED};

INSTANTIATE_TEST_CASE_P(Allow32BitAnd64kbPagesTypes,
                        MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest,
                        ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesAllowed));

static const AllocationType allocationTypesWith32BitAnd64KbPagesNotAllowed[] = {AllocationType::COMMAND_BUFFER,
                                                                                AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
                                                                                AllocationType::PROFILING_TAG_BUFFER,
                                                                                AllocationType::IMAGE,
                                                                                AllocationType::INSTRUCTION_HEAP,
                                                                                AllocationType::SHARED_RESOURCE_COPY};

INSTANTIATE_TEST_CASE_P(Disallow32BitAnd64kbPagesTypes,
                        MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest,
                        ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesNotAllowed));

TEST(MemoryManagerTest, givenForced32BitSetWhenGraphicsMemoryFor32BitAllowedTypeIsAllocatedThen32BitAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    if constexpr (is64bit) {
        EXPECT_TRUE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    } else {
        EXPECT_FALSE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    }

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledShareableWhenGraphicsAllocationIsAllocatedThenAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);
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
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);
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
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);

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
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);

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
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & MemoryConstants::page64kMask);
    EXPECT_EQ(0u, allocation->getGpuAddress() & MemoryConstants::page64kMask);
    EXPECT_EQ(0u, allocation->getUnderlyingBufferSize() & MemoryConstants::page64kMask);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabled64kbPagesWhenGraphicsMemoryWithoutAllow64kbPagesFlagsIsAllocatedThenNon64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    allocData.flags.allow64kbPages = false;

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocation64kbPageCreated);
    EXPECT_TRUE(memoryManager.allocationCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenDisabled64kbPagesWhenGraphicsMemoryMustBeHostMemoryAndIsAllocatedWithNullptrForBufferThenNon64kbAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield);

    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocation64kbPageCreated);
    EXPECT_TRUE(memoryManager.allocationCreated);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenForced32BitAndEnabled64kbPagesWhenGraphicsMemoryMustBeHostMemoryAndIsAllocatedWithNullptrForBufferThen32BitAllocationOver64kbIsChosen) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 10, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield);

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
    AllocationProperties properties(mockRootDeviceIndex, false, 1, AllocationType::BUFFER_HOST_MEMORY, false, mockDeviceBitfield);

    char memory[1];
    memoryManager.getAllocationData(allocData, properties, &memory, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ((executionEnvironment.rootDeviceEnvironments[0u]->getHardwareInfo()->capabilityTable.hostPtrTrackingEnabled || is32bit), allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGraphicsMemoryAllocationInDevicePoolFailsThenFallbackAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocationCreated);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedThenAllocateGraphicsMemoryInPreferredPoolCanAllocateInDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedAndAllocateInDevicePoolFailsWithErrorThenAllocateGraphicsMemoryInPreferredPoolReturnsNullptr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePoolWithError = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield});
    ASSERT_EQ(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThenAllocatingMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allocateMemory);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThen64kbPagesAreAllowedAnd32BitAllocationIsDisallowed) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    EXPECT_FALSE(allocData.flags.allow32Bit);
}

TEST(MemoryManagerTest, givenTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::TAG_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalFenceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::GLOBAL_FENCE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PREEMPTION, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThen64kbPagesAllowed) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PREEMPTION, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThen48BitResourceIsTrue) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PREEMPTION, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenSharedContextImageTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::SHARED_CONTEXT_IMAGE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenMCSTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::MCS, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPipeTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PIPE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::GLOBAL_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenWriteCombinedTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::WRITE_COMBINED, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenInternalHostMemoryTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::INTERNAL_HOST_MEMORY, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenFillPatternTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::FILL_PATTERN, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

using GetAllocationDataTestHw = ::testing::Test;

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenTimestampPacketTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequestedAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(UnitTestHelper<FamilyType>::requiresTimestampPacketsInSystemMemory(*defaultHwInfo), allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenProfilingTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PROFILING_TAG_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagEnabledWhenAllocateMemoryThenAllocationDataIsMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield};
    properties.flags.multiOsContextCapable = true;

    AllocationData allocData;
    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagDisabledWhenAllocateMemoryThenAllocationDataIsNotMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield};
    properties.flags.multiOsContextCapable = false;

    AllocationData allocData;
    memoryManager.getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenConstantSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::CONSTANT_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

HWTEST_F(GetAllocationDataTestHw, givenInternalHeapTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::INTERNAL_HEAP, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenGpuTimestampDeviceBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}
HWTEST_F(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenConstantSurfaceAllocationWhenGetAllocationDataIsCalledThenDontForceSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::CONSTANT_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}
HWTEST_F(GetAllocationDataTestHw, givenKernelIsaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::KERNEL_ISA, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_NE(defaultHwInfo->featureTable.flags.ftrLocalMemory, allocData.flags.useSystemMemory);

    AllocationProperties properties2{mockRootDeviceIndex, 1, AllocationType::KERNEL_ISA_INTERNAL, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_NE(defaultHwInfo->featureTable.flags.ftrLocalMemory, allocData.flags.useSystemMemory);
}

HWTEST_F(GetAllocationDataTestHw, givenLinearStreamWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::LINEAR_STREAM, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

HWTEST_F(GetAllocationDataTestHw, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontUseSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::PRINTF_SURFACE, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenExternalHostMemoryWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, false, 1, AllocationType::EXTERNAL_HOST_PTR, false, mockDeviceBitfield};
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
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, 0u);

    AllocationProperties properties2{rootDevicesCount - 1, 1, AllocationType::BUFFER, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, properties2.rootDeviceIndex);
}

TEST(MemoryManagerTest, givenMapAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, false, 1, AllocationType::MAP_ALLOCATION, false, mockDeviceBitfield};
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
    AllocationProperties properties{mockRootDeviceIndex, 0x10000u, AllocationType::RING_BUFFER, mockDeviceBitfield};
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
    AllocationProperties properties{mockRootDeviceIndex, 0x1000u, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield};
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
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferPlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferPlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenDefaultIsUsedThenExpectNoFlagsChanged) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphorePlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphorePlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideToNo48BitThenExpect48BitFlagFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferAddressing.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::RING_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);

    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingWhenOverrideToNo48BitThenExpect48BitFlagFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphoreAddressing.set(0);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphoreAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::SEMAPHORE_BUFFER, mockDeviceBitfield);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);

    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenForceNonSystemMaskWhenAllocationTypeMatchesMaskThenExpectSystemFlagFalse) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::BUFFER;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(mask);

    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::BUFFER, mockDeviceBitfield);
    allocationData.flags.useSystemMemory = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenForceNonSystemMaskWhenAllocationTypeNotMatchesMaskThenExpectSystemFlagTrue) {
    DebugManagerStateRestore restorer;
    auto allocationType = AllocationType::BUFFER;
    auto mask = 1llu << (static_cast<int64_t>(allocationType) - 1);
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(mask);

    AllocationData allocationData;
    AllocationProperties properties(mockRootDeviceIndex, 0x1000, AllocationType::COMMAND_BUFFER, mockDeviceBitfield);
    allocationData.flags.useSystemMemory = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDebugContextSaveAreaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::DEBUG_CONTEXT_SAVE_AREA, mockDeviceBitfield};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.zeroMemory);
}

TEST(MemoryManagerTest, givenPropertiesWithOsContextWhenGetAllocationDataIsCalledThenOsContextIsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, AllocationType::DEBUG_CONTEXT_SAVE_AREA, mockDeviceBitfield};

    MockOsContext osContext(0u, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0]));

    properties.osContext = &osContext;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(&osContext, allocData.osContext);
}

TEST(MemoryManagerTest, givenPropertiesWithGpuAddressWhenGetAllocationDataIsCalledThenGpuAddressIsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{mockRootDeviceIndex, 1, AllocationType::DEBUG_CONTEXT_SAVE_AREA, mockDeviceBitfield};

    properties.gpuAddress = 0x4000;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(properties.gpuAddress, allocData.gpuAddress);
}

TEST(MemoryManagerTest, givenEnableLocalMemoryAndMemoryManagerWhenBufferTypeIsPassedThenAllocateGraphicsMemoryInPreferredPool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield},
                                                                          nullptr);
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenEnabledLocalMemoryWhenAllocatingSharedResourceCopyThenLocalMemoryAllocationIsReturnedAndGpuAddresIsInStandard64kHeap) {
    UltDeviceFactory deviceFactory{1, 0};
    HardwareInfo localPlatformDevice = {};

    localPlatformDevice = *defaultHwInfo;
    localPlatformDevice.featureTable.flags.ftrLocalMemory = true;

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(&localPlatformDevice, 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(mockRootDeviceIndex, imgInfo, true, memoryProperties, localPlatformDevice, mockDeviceBitfield, true);
    allocProperties.allocationType = AllocationType::SHARED_RESOURCE_COPY;

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    auto gmmHelper = memoryManager.getGmmHelper(allocation->getRootDeviceIndex());
    EXPECT_LT(gmmHelper->canonize(memoryManager.getGfxPartition(allocation->getRootDeviceIndex())->getHeapBase(HeapIndex::HEAP_STANDARD64KB)), allocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager.getGfxPartition(allocation->getRootDeviceIndex())->getHeapLimit(HeapIndex::HEAP_STANDARD64KB)), allocation->getGpuAddress());
    EXPECT_EQ(0llu, allocation->getGpuBaseAddress());

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
    AllocationType::COMMAND_BUFFER,
    AllocationType::IMAGE,
    AllocationType::INDIRECT_OBJECT_HEAP,
    AllocationType::INSTRUCTION_HEAP,
    AllocationType::INTERNAL_HEAP,
    AllocationType::KERNEL_ISA,
    AllocationType::LINEAR_STREAM,
    AllocationType::MCS,
    AllocationType::SCRATCH_SURFACE,
    AllocationType::WORK_PARTITION_SURFACE,
    AllocationType::SHARED_CONTEXT_IMAGE,
    AllocationType::SHARED_IMAGE,
    AllocationType::SHARED_RESOURCE_COPY,
    AllocationType::SURFACE_STATE_HEAP,
    AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
    AllocationType::RING_BUFFER,
    AllocationType::SEMAPHORE_BUFFER,
};

static const AllocationType allocationHaveNotToBeForcedTo48Bit[] = {
    AllocationType::BUFFER,
    AllocationType::BUFFER_HOST_MEMORY,
    AllocationType::CONSTANT_SURFACE,
    AllocationType::EXTERNAL_HOST_PTR,
    AllocationType::FILL_PATTERN,
    AllocationType::GLOBAL_SURFACE,
    AllocationType::INTERNAL_HOST_MEMORY,
    AllocationType::MAP_ALLOCATION,
    AllocationType::PIPE,
    AllocationType::PRINTF_SURFACE,
    AllocationType::PRIVATE_SURFACE,
    AllocationType::PROFILING_TAG_BUFFER,
    AllocationType::SHARED_BUFFER,
    AllocationType::SVM_CPU,
    AllocationType::SVM_GPU,
    AllocationType::SVM_ZERO_COPY,
    AllocationType::TAG_BUFFER,
    AllocationType::GLOBAL_FENCE,
    AllocationType::WRITE_COMBINED,
    AllocationType::DEBUG_CONTEXT_SAVE_AREA,
};

INSTANTIATE_TEST_CASE_P(ForceTo48Bit,
                        MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(allocationHaveToBeForcedTo48Bit),
                            ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(NotForceTo48Bit,
                        MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(allocationHaveNotToBeForcedTo48Bit),
                            ::testing::Bool()));
