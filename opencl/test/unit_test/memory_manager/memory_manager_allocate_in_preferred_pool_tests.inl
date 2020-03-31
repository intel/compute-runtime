/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;
class MemoryManagerGetAlloctionDataTest : public testing::TestWithParam<GraphicsAllocation::AllocationType> {
  public:
    void SetUp() override {}
    void TearDown() override {}
};

TEST(MemoryManagerGetAlloctionDataTest, givenHostMemoryAllocationTypeAndAllocateMemoryFlagAndNullptrWhenAllocationDataIsQueriedThenCorrectFlagsAndSizeAreSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);
    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST(MemoryManagerGetAlloctionDataTest, givenNonHostMemoryAllocatoinTypeWhenAllocationDataIsQueriedThenUseSystemMemoryFlagsIsNotSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_EQ(10u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST(MemoryManagerGetAlloctionDataTest, givenAllocateMemoryFlagTrueWhenHostPtrIsNotNullThenAllocationDataHasHostPtrNulled) {
    AllocationData allocData;
    char memory = 0;
    AllocationProperties properties(0, true, sizeof(memory), GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_EQ(sizeof(memory), allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
}

TEST(MemoryManagerGetAlloctionDataTest, givenBufferTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST(MemoryManagerGetAlloctionDataTest, givenBufferHostMemoryTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST(MemoryManagerGetAlloctionDataTest, givenBufferCompressedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST(MemoryManagerGetAlloctionDataTest, givenWriteCombinedTypeWhenAllocationDataIsQueriedThenForcePinFlagIsSet) {
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::WRITE_COMBINED, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.forcePin);
}

TEST(MemoryManagerGetAlloctionDataTest, givenDefaultAllocationFlagsWhenAllocationDataIsQueriedThenAllocateMemoryIsFalse) {
    AllocationData allocData;
    AllocationProperties properties(0, false, 0, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, false);
    char memory;
    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, &memory, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.allocateMemory);
}

TEST(MemoryManagerGetAlloctionDataTest, givenDebugModeToForceBuffersToSystemMemoryWhenGetAllocationDataIsCalledThenSystemMemoryIsRequired) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceBuffersToSystemMemory.set(true);

    AllocationData allocData;
    AllocationProperties properties(0, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);

    //do not affect BUFFER COMPRESSED
    allocData.flags.useSystemMemory = false;
    AllocationProperties properties2(0, true, 0, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, false);
    MockMemoryManager::getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

typedef MemoryManagerGetAlloctionDataTest MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest;

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest, givenAllocationTypesWith32BitAnd64kbPagesAllowedWhenAllocationDataIsQueriedThenProperFlagsAreSet) {
    AllocationData allocData;
    auto allocType = GetParam();
    AllocationProperties properties(0, true, 0, allocType, false);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.mockExecutionEnvironment->initGmm();
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.allow32Bit);
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocType, allocData.type);
}

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest, given64kbAllowedAllocationTypeWhenAllocatingThenPreferRenderCompressionOnlyForSpecificTypes) {
    auto allocType = GetParam();
    AllocationData allocData;
    AllocationProperties properties(0, true, 10, allocType, false);

    MockMemoryManager mockMemoryManager(true, false);
    mockMemoryManager.mockExecutionEnvironment->initGmm();
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    bool bufferCompressedType = (allocType == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    auto allocation = mockMemoryManager.allocateGraphicsMemory(allocData);

    EXPECT_TRUE(mockMemoryManager.allocation64kbPageCreated);
    EXPECT_EQ(mockMemoryManager.preferRenderCompressedFlagPassed, bufferCompressedType);

    mockMemoryManager.freeGraphicsMemory(allocation);
}

typedef MemoryManagerGetAlloctionDataTest MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest;

TEST_P(MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest, givenAllocationTypesWith32BitAnd64kbPagesDisallowedWhenAllocationDataIsQueriedThenFlagsAreNotSet) {
    AllocationData allocData;
    auto allocType = GetParam();
    AllocationProperties properties(0, true, 0, allocType, false);

    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocType, allocData.type);
}

static const GraphicsAllocation::AllocationType allocationTypesWith32BitAnd64KbPagesAllowed[] = {GraphicsAllocation::AllocationType::BUFFER,
                                                                                                 GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
                                                                                                 GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
                                                                                                 GraphicsAllocation::AllocationType::PIPE,
                                                                                                 GraphicsAllocation::AllocationType::SCRATCH_SURFACE,
                                                                                                 GraphicsAllocation::AllocationType::PRIVATE_SURFACE,
                                                                                                 GraphicsAllocation::AllocationType::PRINTF_SURFACE,
                                                                                                 GraphicsAllocation::AllocationType::CONSTANT_SURFACE,
                                                                                                 GraphicsAllocation::AllocationType::GLOBAL_SURFACE,
                                                                                                 GraphicsAllocation::AllocationType::WRITE_COMBINED};

INSTANTIATE_TEST_CASE_P(Allow32BitAnd64kbPagesTypes,
                        MemoryManagerGetAlloctionData32BitAnd64kbPagesAllowedTest,
                        ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesAllowed));

static const GraphicsAllocation::AllocationType allocationTypesWith32BitAnd64KbPagesNotAllowed[] = {GraphicsAllocation::AllocationType::COMMAND_BUFFER,
                                                                                                    GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
                                                                                                    GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER,
                                                                                                    GraphicsAllocation::AllocationType::IMAGE,
                                                                                                    GraphicsAllocation::AllocationType::INSTRUCTION_HEAP,
                                                                                                    GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY};

INSTANTIATE_TEST_CASE_P(Disallow32BitAnd64kbPagesTypes,
                        MemoryManagerGetAlloctionData32BitAnd64kbPagesNotAllowedTest,
                        ::testing::ValuesIn(allocationTypesWith32BitAnd64KbPagesNotAllowed));

TEST(MemoryManagerTest, givenForced32BitSetWhenGraphicsMemoryFor32BitAllowedTypeIsAllocatedThen32BitAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    AllocationData allocData;
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    if (is64bit) {
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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);
    properties.flags.shareable = true;

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);
    properties.flags.shareable = true;

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

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
    AllocationProperties properties(0, true, 10, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);

    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    if (is64bit) {
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
    AllocationProperties properties(0, false, 1, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false);

    char memory[1];
    MockMemoryManager::getAllocationData(allocData, properties, &memory, memoryManager.createStorageInfoFromProperties(properties));

    auto allocation = memoryManager.allocateGraphicsMemory(allocData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGraphicsMemoryAllocationInDevicePoolFailsThenFallbackAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePool = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    ASSERT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocationCreated);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedThenAllocateGraphicsMemoryInPreferredPoolCanAllocateInDevicePool) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenBufferTypeIsPassedAndAllocateInDevicePoolFailsWithErrorThenAllocateGraphicsMemoryInPreferredPoolReturnsNullptr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    memoryManager.failInDevicePoolWithError = true;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    ASSERT_EQ(nullptr, allocation);
    EXPECT_FALSE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThenAllocatingMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::SVM_ZERO_COPY};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allocateMemory);
}

TEST(MemoryManagerTest, givenSvmAllocationTypeWhenGetAllocationDataIsCalledThen64kbPagesAreAllowedAnd32BitAllocationIsDisallowed) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::SVM_ZERO_COPY};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.allow64kbPages);
    EXPECT_FALSE(allocData.flags.allow32Bit);
}

TEST(MemoryManagerTest, givenTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::TAG_BUFFER};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalFenceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::GLOBAL_FENCE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPreemptionTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::PREEMPTION};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenSharedContextImageTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenMCSTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::MCS};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenPipeTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::PIPE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenGlobalSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::GLOBAL_SURFACE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenWriteCombinedTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::WRITE_COMBINED};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDeviceQueueBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenInternalHostMemoryTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenFillPatternTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::FILL_PATTERN};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenLinearStreamTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::LINEAR_STREAM};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenTimestampPacketTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequestedAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER};
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenProfilingTagBufferTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagEnabledWhenAllocateMemoryThenAllocationDataIsMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER};
    properties.flags.multiOsContextCapable = true;

    AllocationData allocData;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagDisabledWhenAllocateMemoryThenAllocationDataIsNotMultiOsContextCapable) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    AllocationProperties properties{0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER};
    properties.flags.multiOsContextCapable = false;

    AllocationData allocData;
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, memoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.multiOsContextCapable);
}

TEST(MemoryManagerTest, givenConstantSurfaceTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::CONSTANT_SURFACE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenInternalHeapTypeThenUseInternal32BitAllocator) {
    EXPECT_TRUE(MockMemoryManager::useInternal32BitAllocator(GraphicsAllocation::AllocationType::INTERNAL_HEAP));
}

TEST(MemoryManagerTest, givenInternalHeapTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::INTERNAL_HEAP};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenKernelIsaTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::KERNEL_ISA};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenLinearStreamWhenGetAllocationDataIsCalledThenSystemMemoryIsNotRequested) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::LINEAR_STREAM};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenPrintfAllocationWhenGetAllocationDataIsCalledThenDontUseSystemMemoryAndRequireCpuAccess) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::PRINTF_SURFACE};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenKernelIsaTypeThenUseInternal32BitAllocator) {
    EXPECT_TRUE(MockMemoryManager::useInternal32BitAllocator(GraphicsAllocation::AllocationType::KERNEL_ISA));
}

TEST(MemoryManagerTest, givenExternalHostMemoryWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, false, 1, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, false};
    MockMemoryManager::getAllocationData(allocData, properties, hostPtr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocData.hostPtr, hostPtr);
}

TEST(MemoryManagerTest, getAllocationDataProperlyHandlesRootDeviceIndexFromAllcationProperties) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, 1, GraphicsAllocation::AllocationType::BUFFER};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, 0u);

    AllocationProperties properties2{100, 1, GraphicsAllocation::AllocationType::BUFFER};
    MockMemoryManager::getAllocationData(allocData, properties2, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocData.rootDeviceIndex, properties2.rootDeviceIndex);
}

TEST(MemoryManagerTest, givenMapAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    auto hostPtr = reinterpret_cast<void *>(0x1234);
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, false, 1, GraphicsAllocation::AllocationType::MAP_ALLOCATION, false};
    MockMemoryManager::getAllocationData(allocData, properties, hostPtr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(allocData.hostPtr, hostPtr);
}

TEST(MemoryManagerTest, givenRingBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, true, 0x10000u, GraphicsAllocation::AllocationType::RING_BUFFER, false};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x10000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenSemaphoreBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet) {
    AllocationData allocData;
    MockMemoryManager mockMemoryManager;
    AllocationProperties properties{0, true, 0x1000u, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER, false};
    MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_FALSE(allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.flags.allocateMemory);
    EXPECT_FALSE(allocData.flags.allow32Bit);
    EXPECT_FALSE(allocData.flags.allow64kbPages);
    EXPECT_EQ(0x1000u, allocData.size);
    EXPECT_EQ(nullptr, allocData.hostPtr);
    EXPECT_TRUE(allocData.flags.requiresCpuAccess);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenDefaultIsUsedThenExpectNoFlagsChanged) {
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferPlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferPlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferPlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenDefaultIsUsedThenExpectNoFlagsChanged) {
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToNonSystemThenExpectNonSystemFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphorePlacement.set(0);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(0u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectSemaphorePlacementSetWhenOverrideToSystemThenExpectNonFlags) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphorePlacement.set(1);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.requiresCpuAccess);
    EXPECT_EQ(1u, allocationData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideToNo48BitThenExpect48BitFlagFalse) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferAddressing.set(0);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionBufferAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectBufferAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::RING_BUFFER);
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
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingWhenOverrideTo48BitThenExpect48BitFlagTrue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DirectSubmissionSemaphoreAddressing.set(1);
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

TEST(MemoryManagerTest, givenDirectSemaphoreAddressingDefaultWhenNoOverrideThenExpect48BitFlagSame) {
    AllocationData allocationData;
    AllocationProperties properties(0, 0x1000, GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER);
    allocationData.flags.resource48Bit = 0;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(0u, allocationData.flags.resource48Bit);

    allocationData.flags.resource48Bit = 1;
    MockMemoryManager::overrideAllocationData(allocationData, properties);

    EXPECT_EQ(1u, allocationData.flags.resource48Bit);
}

using MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest = testing::TestWithParam<std::tuple<GraphicsAllocation::AllocationType, bool>>;

TEST_P(MemoryManagerGetAlloctionDataHaveToBeForcedTo48BitTest, givenAllocationTypesHaveToBeForcedTo48BitThenAllocationDataResource48BitIsSet) {
    GraphicsAllocation::AllocationType allocationType;
    bool propertiesFlag48Bit;

    std::tie(allocationType, propertiesFlag48Bit) = GetParam();

    AllocationProperties properties(0, true, 0, allocationType, false);
    properties.flags.resource48Bit = propertiesFlag48Bit;

    AllocationData allocationData;
    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocationData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_TRUE(allocationData.flags.resource48Bit);
}

using MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest = testing::TestWithParam<std::tuple<GraphicsAllocation::AllocationType, bool>>;

TEST_P(MemoryManagerGetAlloctionDataHaveNotToBeForcedTo48BitTest, givenAllocationTypesHaveNotToBeForcedTo48BitThenAllocationDataResource48BitIsSetProperly) {
    GraphicsAllocation::AllocationType allocationType;
    bool propertiesFlag48Bit;

    std::tie(allocationType, propertiesFlag48Bit) = GetParam();

    AllocationProperties properties(0, true, 0, allocationType, false);
    properties.flags.resource48Bit = propertiesFlag48Bit;

    AllocationData allocationData;
    MockMemoryManager mockMemoryManager;
    MockMemoryManager::getAllocationData(allocationData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(allocationData.flags.resource48Bit, propertiesFlag48Bit);
}

static const GraphicsAllocation::AllocationType allocationHaveToBeForcedTo48Bit[] = {
    GraphicsAllocation::AllocationType::COMMAND_BUFFER,
    GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER,
    GraphicsAllocation::AllocationType::IMAGE,
    GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP,
    GraphicsAllocation::AllocationType::INSTRUCTION_HEAP,
    GraphicsAllocation::AllocationType::INTERNAL_HEAP,
    GraphicsAllocation::AllocationType::KERNEL_ISA,
    GraphicsAllocation::AllocationType::LINEAR_STREAM,
    GraphicsAllocation::AllocationType::MCS,
    GraphicsAllocation::AllocationType::SCRATCH_SURFACE,
    GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE,
    GraphicsAllocation::AllocationType::SHARED_IMAGE,
    GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY,
    GraphicsAllocation::AllocationType::SURFACE_STATE_HEAP,
    GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
};

static const GraphicsAllocation::AllocationType allocationHaveNotToBeForcedTo48Bit[] = {
    GraphicsAllocation::AllocationType::BUFFER,
    GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
    GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY,
    GraphicsAllocation::AllocationType::CONSTANT_SURFACE,
    GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR,
    GraphicsAllocation::AllocationType::FILL_PATTERN,
    GraphicsAllocation::AllocationType::GLOBAL_SURFACE,
    GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
    GraphicsAllocation::AllocationType::MAP_ALLOCATION,
    GraphicsAllocation::AllocationType::PIPE,
    GraphicsAllocation::AllocationType::PREEMPTION,
    GraphicsAllocation::AllocationType::PRINTF_SURFACE,
    GraphicsAllocation::AllocationType::PRIVATE_SURFACE,
    GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER,
    GraphicsAllocation::AllocationType::SHARED_BUFFER,
    GraphicsAllocation::AllocationType::SVM_CPU,
    GraphicsAllocation::AllocationType::SVM_GPU,
    GraphicsAllocation::AllocationType::SVM_ZERO_COPY,
    GraphicsAllocation::AllocationType::TAG_BUFFER,
    GraphicsAllocation::AllocationType::GLOBAL_FENCE,
    GraphicsAllocation::AllocationType::WRITE_COMBINED,
    GraphicsAllocation::AllocationType::RING_BUFFER,
    GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER,
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
