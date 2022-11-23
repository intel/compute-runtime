/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/test/unit_test/os_interface/windows/wddm_memory_manager_tests.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"
using namespace NEO;
using namespace ::testing;

TEST_F(WddmMemoryManagerSimpleTest, givenUseSystemMemorySetToTrueWhenAllocateInDevicePoolIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationIsReturned) {
    const bool localMemoryEnabled = true;

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenShareableAllocationWhenAllocateInDevicePoolThenMemoryIsNotLocableAndLocalOnlyIsSet) {
    const bool localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.type = AllocationType::SVM_GPU;
    allocData.storageInfo.localOnlyRequired = true;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.flags.shareable = true;
    allocData.storageInfo.memoryBanks = 2;
    allocData.storageInfo.systemMemoryPlacement = false;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_NE(allocation->peekInternalHandle(memoryManager.get()), 0u);

    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenShareableAllocationWhenAllocateGraphicsMemoryInPreferredPoolThenMemoryIsNotLocableAndLocalOnlyIsSet) {
    const bool localMemoryEnabled = true;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);
    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SVM_GPU, mockDeviceBitfield};
    properties.allFlags = 0;
    properties.size = MemoryConstants::pageSize;
    properties.flags.allocateMemory = true;
    properties.flags.shareable = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_NE(allocation->peekInternalHandle(memoryManager.get()), 0u);

    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);

    memoryManager->freeGraphicsMemory(allocation);
}

struct WddmMemoryManagerDevicePoolAlignmentTests : WddmMemoryManagerSimpleTest {
    void testAlignment(uint32_t allocationSize, uint32_t expectedAlignment) {
        const bool enable64kbPages = false;
        const bool localMemoryEnabled = true;
        memoryManager = std::make_unique<MockWddmMemoryManager>(enable64kbPages, localMemoryEnabled, *executionEnvironment);

        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = allocationSize;
        allocData.flags.allocateMemory = true;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);

        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
        EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_EQ(alignUp(allocationSize, expectedAlignment), allocation->getUnderlyingBufferSize());
        EXPECT_EQ(expectedAlignment, allocation->getDefaultGmm()->resourceParams.BaseAlignment);

        memoryManager->freeGraphicsMemory(allocation);
    }

    DebugManagerStateRestore restore{};
};

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenCustomAlignmentAndAllocationAsBigAsTheAlignmentWhenAllocationInDevicePoolIsCreatedThenUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::pageSize64k;
    const uint32_t expectedAlignment = customAlignment;
    const uint32_t size = 4 * MemoryConstants::pageSize64k;
    DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
    testAlignment(size + 1, expectedAlignment);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenCustomAlignmentAndAllocationNotAsBigAsTheAlignmentWhenAllocationInDevicePoolIsCreatedThenDoNotUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::pageSize64k;
    const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
    const uint32_t size = 3 * MemoryConstants::pageSize64k;
    DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenCustomAlignmentBiggerThan2MbAndAllocationBiggerThanCustomAlignmentWhenAllocationInDevicePoolIsCreatedThenUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::megaByte;
    const uint32_t expectedAlignment = customAlignment;
    const uint32_t size = 4 * MemoryConstants::megaByte;
    DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
    testAlignment(size + 1, expectedAlignment);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenCustomAlignmentBiggerThan2MbAndAllocationLessThanCustomAlignmentWhenAllocationInDevicePoolIsCreatedThenDoNotUseCustomAlignment) {
    const uint32_t customAlignment = 4 * MemoryConstants::megaByte;
    const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
    const uint32_t size = 4 * MemoryConstants::megaByte - 1;
    DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(customAlignment);
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenAllocationLessThen2MbWhenAllocationInDevicePoolIsCreatedThenUse64KbAlignment) {
    const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
    const uint32_t size = 2 * MemoryConstants::megaByte - 1;
    testAlignment(size, expectedAlignment);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenTooMuchMemoryWastedOn2MbAlignmentWhenAllocationInDevicePoolIsCreatedThenUse64kbAlignment) {
    const float threshold = 0.1f;

    {
        const uint32_t alignedSize = 4 * MemoryConstants::megaByte;
        const uint32_t maxAmountOfWastedMemory = static_cast<uint32_t>(alignedSize * threshold);
        testAlignment(alignedSize, MemoryConstants::pageSize2Mb);
        testAlignment(alignedSize - maxAmountOfWastedMemory + 1, MemoryConstants::pageSize2Mb);
        testAlignment(alignedSize - maxAmountOfWastedMemory - 1, MemoryConstants::pageSize64k);
    }

    {
        const uint32_t alignedSize = 8 * MemoryConstants::megaByte;
        const uint32_t maxAmountOfWastedMemory = static_cast<uint32_t>(alignedSize * threshold);
        testAlignment(alignedSize, MemoryConstants::pageSize2Mb);
        testAlignment(alignedSize - maxAmountOfWastedMemory + 1, MemoryConstants::pageSize2Mb);
        testAlignment(alignedSize - maxAmountOfWastedMemory - 1, MemoryConstants::pageSize64k);
    }
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenBigAllocationWastingMaximumPossibleAmountOfMemorytWhenAllocationInDevicePoolIsCreatedThenStillUse2MbAlignment) {
    const uint32_t size = 200 * MemoryConstants::megaByte + 1; // almost entire 2MB page will be wasted
    testAlignment(size, MemoryConstants::pageSize2Mb);
}

TEST_F(WddmMemoryManagerDevicePoolAlignmentTests, givenAtLeast2MbAllocationWhenAllocationInDevicePoolIsCreatedThenUse2MbAlignment) {
    const uint32_t size = 2 * MemoryConstants::megaByte;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        const uint32_t expectedAlignment = MemoryConstants::pageSize64k;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        const uint32_t expectedAlignment = 2 * MemoryConstants::megaByte;
        testAlignment(size, expectedAlignment);
        testAlignment(2 * size, expectedAlignment);
    }
}

HWTEST_F(WddmMemoryManagerSimpleTest, givenLinearStreamWhenItIsAllocatedThenItIsInLocalMemoryHasCpuPointerAndHasStandardHeap64kbAsGpuAddress) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::LINEAR_STREAM, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(MemoryPool::LocalMemory, graphicsAllocation->getMemoryPool());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_TRUE(graphicsAllocation->isLocked());
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto &partition = wddm->getGfxPartition();

    if (is64bit) {
        auto gmmHelper = memoryManager->getGmmHelper(graphicsAllocation->getRootDeviceIndex());
        if (executionEnvironment->rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()]->isFullRangeSvm()) {
            EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard64KB.Base));
            EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard64KB.Limit));
        } else {
            EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard.Base));
            EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard.Limit));
        }
    } else {
        if (executionEnvironment->rootDeviceEnvironments[graphicsAllocation->getRootDeviceIndex()]->isFullRangeSvm()) {
            EXPECT_GE(gpuAddress, 0ull);
            EXPECT_LE(gpuAddress, UINT32_MAX);

            EXPECT_GE(gpuAddressEnd, 0ull);
            EXPECT_LE(gpuAddressEnd, UINT32_MAX);
        }
    }

    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::LINEAR_STREAM);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenLocalMemoryAllocationHasCorrectStorageInfoAndFlushL3IsSet) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = 0x1;
    allocData.storageInfo.pageTablesVisibility = 0x2;
    allocData.storageInfo.cloningOfPageTables = false;
    allocData.flags.flushL3 = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_FALSE(allocation->storageInfo.cloningOfPageTables);
    EXPECT_TRUE(allocation->isFlushL3Required());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndUseSytemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);
    memoryManager->setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryAndAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    const bool localMemoryEnabled = true;

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);
    memoryManager->setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenEnabledLocalMemoryWhenAllocateFailsThenGraphicsAllocationInDevicePoolReturnsError) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseDestroyAllocations = false;
    wddm->createAllocationStatus = STATUS_NO_MEMORY;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenLocalMemoryAllocationWhenCpuPointerNotMeetRestrictionsThenDontReserveMemRangeForMap) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, size, AllocationType::BUFFER, mockDeviceBitfield}, cpuPtr));

    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));
    if (is32bit && this->executionEnvironment->rootDeviceEnvironments[allocation->getRootDeviceIndex()]->isFullRangeSvm()) {
        EXPECT_NE(nullptr, allocation->getReservedAddressPtr());
        EXPECT_EQ(alignUp(size, MemoryConstants::pageSize64k) + 2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        EXPECT_EQ(allocation->getGpuAddress(), castToUint64(allocation->getReservedAddressPtr()));
    } else {
        EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, whenMemoryIsAllocatedInLocalMemoryThenTheAllocationNeedsMakeResidentBeforeLock) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->needsMakeResidentBeforeLock);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithHighPriorityWhenMemoryIsAllocatedInLocalMemoryThenSetAllocationPriorityIsCalledWithHighPriority) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);

    AllocationType highPriorityTypes[] = {
        AllocationType::KERNEL_ISA,
        AllocationType::KERNEL_ISA_INTERNAL,
        AllocationType::COMMAND_BUFFER,
        AllocationType::INTERNAL_HEAP,
        AllocationType::LINEAR_STREAM

    };
    for (auto &allocationType : highPriorityTypes) {

        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = MemoryConstants::pageSize;
        allocData.flags.allocateMemory = true;
        allocData.type = allocationType;

        auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(1u, wddm->setAllocationPriorityResult.called);
        EXPECT_EQ(DXGI_RESOURCE_PRIORITY_HIGH, wddm->setAllocationPriorityResult.uint64ParamPassed);

        wddm->setAllocationPriorityResult.called = 0u;
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(WddmMemoryManagerSimpleTest, givenAllocationWithoutHighPriorityWhenMemoryIsAllocatedInLocalMemoryThenSetAllocationPriorityIsCalledWithNormalPriority) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(1u, wddm->setAllocationPriorityResult.called);
    EXPECT_EQ(static_cast<uint64_t>(DXGI_RESOURCE_PRIORITY_NORMAL), wddm->setAllocationPriorityResult.uint64ParamPassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenSetAllocationPriorityFailureWhenMemoryIsAllocatedInLocalMemoryThenNullptrIsReturned) {
    const bool localMemoryEnabled = true;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, localMemoryEnabled, *executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    wddm->callBaseSetAllocationPriority = false;
    wddm->setAllocationPriorityResult.success = false;

    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

class WddmMemoryManagerSimpleTestWithLocalMemory : public MockWddmMemoryManagerFixture, public ::testing::Test {
  public:
    void SetUp() override {
        HardwareInfo localPlatformDevice = *defaultHwInfo;
        localPlatformDevice.featureTable.flags.ftrLocalMemory = true;

        platformsImpl->clear();
        auto executionEnvironment = constructPlatform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&localPlatformDevice);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        MockWddmMemoryManagerFixture::SetUp();
        wddm->init();
    }
    void TearDown() override {
        MockWddmMemoryManagerFixture::TearDown();
    }
    HardwareInfo localPlatformDevice = {};
    FeatureTable ftrTable = {};
};

TEST_F(WddmMemoryManagerSimpleTestWithLocalMemory, givenLocalMemoryAndImageOrSharedResourceWhenAllocateInDevicePoolIsCalledThenLocalMemoryAllocationAndAndStatusSuccessIsReturned) {
    memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 1;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationType types[] = {AllocationType::IMAGE,
                              AllocationType::SHARED_RESOURCE_COPY};

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.preferCompressed = true;

    allocData.imgInfo = &imgInfo;

    for (uint32_t i = 0; i < arrayCount(types); i++) {
        allocData.type = types[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
        EXPECT_EQ(0u, allocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
        memoryManager->freeGraphicsMemory(allocation);
    }
}
using WddmMemoryManagerMultiHandleAllocationTest = WddmMemoryManagerSimpleTest;

TEST_F(WddmMemoryManagerSimpleTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{mockRootDeviceIndex, false, size, AllocationType::SVM_GPU, false, mockDeviceBitfield};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);
    memoryManager->localMemorySupported[properties.rootDeviceIndex] = true;
    auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());
    // limited platforms will not use heap HeapIndex::HEAP_SVM
    if (executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm()) {
        EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));
    }
    EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(WddmMemoryManager, givenWddmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(4u);

    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment.rootDeviceEnvironments[i]->osInterface.reset();
        auto wddmMock = Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i]);
        wddmMock->init();

        static_cast<WddmMock *>(wddmMock)->dedicatedVideoMemory = 32 * MemoryConstants::gigaByte;
    }

    MockWddmMemoryManager memoryManager(executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto wddmMock = executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Wddm>();

        auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
        auto deviceMask = std::max(static_cast<uint32_t>(maxNBitValue(hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount)), 1u);

        EXPECT_EQ(wddmMock->getDedicatedVideoMemory(), memoryManager.getLocalMemorySize(i, deviceMask));
    }
}

TEST(WddmMemoryManager, givenMultipleTilesWhenGetLocalMemorySizeIsCalledThenReturnCorrectValue) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.prepareRootDeviceEnvironments(1u);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();

    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset();
    auto wddmMock = Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    wddmMock->init();

    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    static_cast<WddmMock *>(wddmMock)->dedicatedVideoMemory = 32 * MemoryConstants::gigaByte;

    MockWddmMemoryManager memoryManager(executionEnvironment);

    auto singleRegionSize = wddmMock->getDedicatedVideoMemory() / hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount;

    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0001));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0010));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b0100));
    EXPECT_EQ(singleRegionSize, memoryManager.getLocalMemorySize(0, 0b1000));

    EXPECT_EQ(singleRegionSize * 2, memoryManager.getLocalMemorySize(0, 0b0011));

    EXPECT_EQ(wddmMock->getDedicatedVideoMemory(), memoryManager.getLocalMemorySize(0, 0b1111));
}

TEST_F(WddmMemoryManagerSimpleTest, given32BitAllocationOfBufferWhenItIsAllocatedThenItHas32BitGpuPointer) {
    if constexpr (is64bit) {
        GTEST_SKIP();
    }
    REQUIRE_SVM_OR_SKIP(defaultHwInfo);
    AllocationType allocationTypes[] = {AllocationType::BUFFER,
                                        AllocationType::SHARED_BUFFER,
                                        AllocationType::SCRATCH_SURFACE,
                                        AllocationType::PRIVATE_SURFACE};

    for (auto &allocationType : allocationTypes) {
        size_t size = 2 * MemoryConstants::kiloByte;
        auto alignedSize = alignUp(size, MemoryConstants::pageSize64k);
        AllocationProperties properties{mockRootDeviceIndex, size, allocationType, mockDeviceBitfield};
        memoryManager->localMemorySupported[properties.rootDeviceIndex] = true;
        auto allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, nullptr));
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(alignedSize, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
        EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

        EXPECT_NE(nullptr, allocation->getReservedAddressPtr());
        EXPECT_EQ(alignedSize + 2 * MemoryConstants::megaByte, allocation->getReservedAddressSize());
        EXPECT_EQ(castToUint64(allocation->getReservedAddressPtr()), allocation->getGpuAddress());
        EXPECT_EQ(0u, allocation->getGpuAddress() % 2 * MemoryConstants::megaByte);

        EXPECT_GE(allocation->getGpuAddress(), 0u);
        EXPECT_LE(allocation->getGpuAddress(), MemoryConstants::max32BitAddress);

        memoryManager->freeGraphicsMemory(allocation);
    }
}
struct WddmMemoryManagerSimple64BitTest : public WddmMemoryManagerSimpleTest {
    using WddmMemoryManagerSimpleTest::SetUp;
    using WddmMemoryManagerSimpleTest::TearDown;

    template <bool using32Bit>
    void givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted() {
        if constexpr (using32Bit) {
            GTEST_SKIP();
        } else {

            DebugManagerStateRestore dbgRestore;
            wddm->init();
            wddm->mapGpuVaStatus = true;
            VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

            memoryManager = std::make_unique<MockWddmMemoryManager>(false, true, *executionEnvironment);
            AllocationData allocData;
            allocData.allFlags = 0;
            allocData.size = static_cast<size_t>(MemoryConstants::gigaByte * 13);
            allocData.flags.allocateMemory = true;

            MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
            auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
            EXPECT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
            EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
            EXPECT_EQ(4, allocation->getNumGmms());
            EXPECT_EQ(4, wddm->createAllocationResult.called);

            uint64_t totalSizeFromGmms = 0u;
            for (uint32_t gmmId = 0u; gmmId < allocation->getNumGmms(); ++gmmId) {
                Gmm *gmm = allocation->getGmm(gmmId);
                EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
                EXPECT_EQ(2 * MemoryConstants::megaByte, gmm->resourceParams.BaseAlignment);
                EXPECT_TRUE(isAligned(gmm->resourceParams.BaseWidth64, gmm->resourceParams.BaseAlignment));

                totalSizeFromGmms += gmm->resourceParams.BaseWidth64;
            }
            EXPECT_EQ(static_cast<uint64_t>(allocData.size), totalSizeFromGmms);

            memoryManager->freeGraphicsMemory(allocation);
        }
    }
};
TEST_F(WddmMemoryManagerSimple64BitTest, givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted) {
    givenLocalMemoryAllocationAndRequestedSizeIsHugeThenResultAllocationIsSplitted<is32bit>();
}
