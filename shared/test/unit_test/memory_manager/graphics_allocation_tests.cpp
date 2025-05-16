/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenAllInspectionIdsAreSetToZero) {
    MockGraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    for (auto i = 0u; i < MemoryManager::maxOsContextCount; i++) {
        EXPECT_EQ(0u, graphicsAllocation.getInspectionId(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenTaskCountsAreInitializedProperly) {
    GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    for (auto i = 0u; i < MemoryManager::maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation1.getTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation2.getTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation1.getResidencyTaskCount(i));
        EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation2.getResidencyTaskCount(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenAllocationWasUsed) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationGetDefaultHandleAddressAndHandleSize) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_EQ(0lu, graphicsAllocation.getHandleAddressBase(0));
    EXPECT_EQ(0lu, graphicsAllocation.getHandleSize(0));
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountThenOnlyOneTaskCountIsUpdated) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.updateTaskCount(1u, 0u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    for (auto i = 1u; i < MemoryManager::maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation.getTaskCount(i));
    }
    graphicsAllocation.updateTaskCount(2u, 1u);
    EXPECT_EQ(1u, graphicsAllocation.getTaskCount(0u));
    EXPECT_EQ(2u, graphicsAllocation.getTaskCount(1u));
    for (auto i = 2u; i < MemoryManager::maxOsContextCount; i++) {
        EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation.getTaskCount(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenGettingTaskCountForInvalidContextIdThenObjectNotUsedIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    for (auto i = 0u; i < MemoryManager::maxOsContextCount; i++) {
        graphicsAllocation.updateTaskCount(1u, i);
    }
    EXPECT_EQ(MockGraphicsAllocation::objectNotUsed, graphicsAllocation.getTaskCount(MemoryManager::maxOsContextCount));
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatingTaskCountForInvalidContextIdThenAbortExecution) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_THROW(graphicsAllocation.updateTaskCount(1u, MemoryManager::maxOsContextCount), std::runtime_error);
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedTaskCountToobjectNotUsedValueThenUnregisterContext) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_FALSE(graphicsAllocation.isUsed());
}
TEST(GraphicsAllocationTest, whenTwoContextsUpdatedTaskCountAndOneOfThemUnregisteredThenOneContextUsageRemains) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(0u, 0u);
    graphicsAllocation.updateTaskCount(0u, 1u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 0u);
    EXPECT_TRUE(graphicsAllocation.isUsed());
    graphicsAllocation.updateTaskCount(MockGraphicsAllocation::objectNotUsed, 1u);
    EXPECT_FALSE(graphicsAllocation.isUsed());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenUpdatedResidencyTaskCountToNonDefaultValueThenAllocationIsResident) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
    uint32_t residencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(residencyTaskCount, 0u);
    EXPECT_EQ(residencyTaskCount, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    graphicsAllocation.updateResidencyTaskCount(MockGraphicsAllocation::objectNotResident, 0u);
    EXPECT_EQ(MockGraphicsAllocation::objectNotResident, graphicsAllocation.getResidencyTaskCount(0u));
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenResetResidencyTaskCountThenAllocationIsNotResident) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.updateResidencyTaskCount(1u, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));

    graphicsAllocation.releaseResidencyInOsContext(0u);
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
}

TEST(GraphicsAllocationTest, givenNonResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowAnyValueThenReturnTrue) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation.isResident(0u));
    EXPECT_TRUE(graphicsAllocation.isResidencyTaskCountBelow(0u, 0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowCurrentResidencyTaskCountThenReturnFalse) {
    MockGraphicsAllocation graphicsAllocation;
    auto currentResidencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(currentResidencyTaskCount, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    EXPECT_FALSE(graphicsAllocation.isResidencyTaskCountBelow(currentResidencyTaskCount, 0u));
}

TEST(GraphicsAllocationTest, givenResidentGraphicsAllocationWhenCheckIfResidencyTaskCountIsBelowHigherThanCurrentResidencyTaskCountThenReturnTrue) {
    MockGraphicsAllocation graphicsAllocation;
    auto currentResidencyTaskCount = 1u;
    graphicsAllocation.updateResidencyTaskCount(currentResidencyTaskCount, 0u);
    EXPECT_TRUE(graphicsAllocation.isResident(0u));
    EXPECT_TRUE(graphicsAllocation.isResidencyTaskCountBelow(currentResidencyTaskCount + 1u, 0u));
}

TEST(GraphicsAllocationTest, givenAllocationTypeWhenCheckingCpuAccessRequiredThenReturnTrue) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocType = static_cast<AllocationType>(i);

        switch (allocType) {
        case AllocationType::commandBuffer:
        case AllocationType::constantSurface:
        case AllocationType::globalSurface:
        case AllocationType::internalHeap:
        case AllocationType::linearStream:
        case AllocationType::pipe:
        case AllocationType::printfSurface:
        case AllocationType::timestampPacketTagBuffer:
        case AllocationType::ringBuffer:
        case AllocationType::semaphoreBuffer:
        case AllocationType::debugContextSaveArea:
        case AllocationType::debugSbaTrackingBuffer:
        case AllocationType::gpuTimestampDeviceBuffer:
        case AllocationType::debugModuleArea:
        case AllocationType::assertBuffer:
        case AllocationType::syncDispatchToken:
        case AllocationType::syncBuffer:
            EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(allocType));
            break;
        default:
            EXPECT_FALSE(GraphicsAllocation::isCpuAccessRequired(allocType));
            break;
        }
    }
}

TEST(GraphicsAllocationTest, whenAllocationRequiresCpuAccessThenAllocationIsLockable) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::unknown);
    auto lastAllocationIdx = static_cast<int>(AllocationType::count);

    for (int allocationIdx = firstAllocationIdx; allocationIdx != lastAllocationIdx; allocationIdx++) {
        auto allocationType = static_cast<AllocationType>(allocationIdx);
        if (GraphicsAllocation::isCpuAccessRequired(allocationType)) {
            EXPECT_TRUE(GraphicsAllocation::isLockable(allocationType));
        }
    }
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsISAThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::kernelIsa));
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::kernelIsaInternal));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsBufferThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::buffer));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsBufferHostMemoryThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::bufferHostMemory));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsGpuTimestampDeviceBufferThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::gpuTimestampDeviceBuffer));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsSvmGpuThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::svmGpu));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsSharedResourceCopyThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::sharedResourceCopy));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsImageThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::image));
}

TEST(GraphicsAllocationTest, givenNumMemoryBanksWhenGettingNumHandlesForKmdSharedAllocationThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    EXPECT_EQ(1u, GraphicsAllocation::getNumHandlesForKmdSharedAllocation(1));
    EXPECT_EQ(2u, GraphicsAllocation::getNumHandlesForKmdSharedAllocation(2));

    debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.set(0);
    EXPECT_EQ(1u, GraphicsAllocation::getNumHandlesForKmdSharedAllocation(2));

    debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.set(1);
    EXPECT_EQ(2u, GraphicsAllocation::getNumHandlesForKmdSharedAllocation(2));
}

TEST(GraphicsAllocationTest, givenDefaultAllocationWhenGettingNumHandlesThenOneIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_EQ(1u, graphicsAllocation.getNumGmms());
}

TEST(GraphicsAllocationTest, givenAlwaysResidentAllocationWhenUpdateTaskCountThenItIsNotUpdated) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, 0u);

    graphicsAllocation.updateResidencyTaskCount(10u, 0u);

    EXPECT_EQ(graphicsAllocation.getResidencyTaskCount(0u), GraphicsAllocation::objectAlwaysResident);
}

TEST(GraphicsAllocationTest, givenDefaultGraphicsAllocationWhenInternalHandleIsBeingObtainedThenZeroIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    uint64_t handle = 0;
    graphicsAllocation.peekInternalHandle(nullptr, handle);
    EXPECT_EQ(0ull, handle);
}

TEST(GraphicsAllocationTest, givenDefaultGraphicsAllocationWhenInternalHandleIsBeingObtainedOrCreatedThenZeroIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    uint64_t handle = 0;
    graphicsAllocation.createInternalHandle(nullptr, 0u, handle);
    EXPECT_EQ(0ull, handle);
    graphicsAllocation.clearInternalHandle(0u);
}

TEST(GraphicsAllocationTest, givenDefaultGraphicsAllocationWhenGettingNumHandlesThenZeroIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    EXPECT_EQ(0u, graphicsAllocation.getNumHandles());
}

TEST(GraphicsAllocationTest, givenDefaultGraphicsAllocationWhenGettingNumHandlesAfterSettingNonZeroNumberThenZeroIsReturned) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setNumHandles(64u);
    EXPECT_EQ(0u, graphicsAllocation.getNumHandles());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenQueryingUsedPageSizeThenCorrectSizeForMemoryPoolUsedIsReturned) {

    MemoryPool page4kPools[] = {MemoryPool::memoryNull,
                                MemoryPool::system4KBPages,
                                MemoryPool::system4KBPagesWith32BitGpuAddressing,
                                MemoryPool::systemCpuInaccessible};

    for (auto pool : page4kPools) {
        MockGraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0u, static_cast<osHandle>(1), pool, MemoryManager::maxOsContextCount);

        EXPECT_EQ(MemoryConstants::pageSize, graphicsAllocation.getUsedPageSize());
    }

    MemoryPool page64kPools[] = {MemoryPool::system64KBPages,
                                 MemoryPool::system64KBPagesWith32BitGpuAddressing,
                                 MemoryPool::localMemory};

    for (auto pool : page64kPools) {
        MockGraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0u, 0, pool, MemoryManager::maxOsContextCount);

        EXPECT_EQ(MemoryConstants::pageSize64k, graphicsAllocation.getUsedPageSize());
    }
}

struct GraphicsAllocationTests : public ::testing::Test {
    template <typename GfxFamily>
    void initializeCsr() {
        executionEnvironment.initializeMemoryManager();
        DeviceBitfield deviceBitfield(3);
        auto csr = new MockAubCsr<GfxFamily>("", true, executionEnvironment, 0, deviceBitfield);
        csr->multiOsContextCapable = true;
        aubCsr.reset(csr);
    }

    template <typename GfxFamily>
    MockAubCsr<GfxFamily> &getAubCsr() {
        return *(static_cast<MockAubCsr<GfxFamily> *>(aubCsr.get()));
    }

    void gfxAllocationSetToDefault() {
        graphicsAllocation.storageInfo.memoryBanks = 0;
        graphicsAllocation.overrideMemoryPool(MemoryPool::memoryNull);
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<CommandStreamReceiver> aubCsr;
    MockGraphicsAllocation graphicsAllocation;
};

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationThatHasPageTablesCloningWhenWriteableFlagsAreUsedThenDefaultBankIsUsed) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    graphicsAllocation.storageInfo.memoryBanks = 0x2;
    graphicsAllocation.overrideMemoryPool(MemoryPool::localMemory);
    graphicsAllocation.storageInfo.cloningOfPageTables = true;

    EXPECT_TRUE(aubCsr.isAubWritable(graphicsAllocation));

    // modify non default bank
    graphicsAllocation.setAubWritable(false, 0x2);

    EXPECT_TRUE(aubCsr.isAubWritable(graphicsAllocation));

    aubCsr.setAubWritable(false, graphicsAllocation);

    EXPECT_FALSE(aubCsr.isAubWritable(graphicsAllocation));

    EXPECT_TRUE(aubCsr.isTbxWritable(graphicsAllocation));

    graphicsAllocation.setTbxWritable(false, 0x2);
    EXPECT_TRUE(aubCsr.isTbxWritable(graphicsAllocation));

    aubCsr.setTbxWritable(false, graphicsAllocation);

    EXPECT_FALSE(aubCsr.isTbxWritable(graphicsAllocation));
}

uint32_t MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes = 0;

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenAssignedTaskCountEqualZeroThenPrepareForResidencyDoeNotCallGetTaskCount) {
    MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes = 0;
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.hostPtrTaskCountAssignment = 0;
    graphicsAllocation.prepareHostPtrForResidency(nullptr);
    EXPECT_EQ(MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes, 0u);
    MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes = 0;
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationWhenAssignedTaskCountAbovelZeroThenPrepareForResidencyGetTaskCountWasCalled) {
    executionEnvironment.initializeMemoryManager();
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));
    MockCommandStreamReceiver csr(executionEnvironment, 0, 1);
    csr.osContext = osContext.get();
    MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes = 0;
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.hostPtrTaskCountAssignment = 1;
    graphicsAllocation.prepareHostPtrForResidency(&csr);
    EXPECT_EQ(MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes, 1u);
    MockGraphicsAllocationTaskCount::getTaskCountCalleedTimes = 0;
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationAllocTaskCountHigherThanInCsrThenUpdateTaskCountWasNotCalled) {
    executionEnvironment.initializeMemoryManager();
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));
    MockCommandStreamReceiver csr(executionEnvironment, 0, 1);
    csr.osContext = osContext.get();
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.updateTaskCount(10u, 0u);
    *csr.getTagAddress() = 5;
    graphicsAllocation.hostPtrTaskCountAssignment = 1;
    auto calledTimesBefore = graphicsAllocation.updateTaskCountCalleedTimes;
    graphicsAllocation.prepareHostPtrForResidency(&csr);
    EXPECT_EQ(graphicsAllocation.updateTaskCountCalleedTimes, calledTimesBefore);
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationAllocTaskCountLowerThanInCsrThenUpdateTaskCountWasCalled) {
    executionEnvironment.initializeMemoryManager();
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));
    MockCommandStreamReceiver csr(executionEnvironment, 0, 1);
    csr.osContext = osContext.get();
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.updateTaskCount(5u, 0u);
    csr.taskCount = 10;
    graphicsAllocation.hostPtrTaskCountAssignment = 1;
    auto calledTimesBefore = graphicsAllocation.updateTaskCountCalleedTimes;
    graphicsAllocation.prepareHostPtrForResidency(&csr);
    EXPECT_EQ(graphicsAllocation.updateTaskCountCalleedTimes, calledTimesBefore + 1u);
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationAllocTaskCountNotUsedLowerThanInCsrThenUpdateTaskCountWasCalled) {
    executionEnvironment.initializeMemoryManager();
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));
    MockCommandStreamReceiver csr(executionEnvironment, 0, 1);
    csr.osContext = osContext.get();
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.updateTaskCount(GraphicsAllocation::objectNotResident, 0u);
    csr.taskCount = 10;
    graphicsAllocation.hostPtrTaskCountAssignment = 1;
    auto calledTimesBefore = graphicsAllocation.updateTaskCountCalleedTimes;
    graphicsAllocation.prepareHostPtrForResidency(&csr);
    EXPECT_EQ(graphicsAllocation.updateTaskCountCalleedTimes, calledTimesBefore + 1u);
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationAllocTaskCountLowerThanInCsrThenAssignmentCountIsDecremented) {
    executionEnvironment.initializeMemoryManager();
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));
    MockCommandStreamReceiver csr(executionEnvironment, 0, 1);
    csr.osContext = osContext.get();
    MockGraphicsAllocationTaskCount graphicsAllocation;
    graphicsAllocation.updateTaskCount(5u, 0u);
    csr.taskCount = 10;
    graphicsAllocation.hostPtrTaskCountAssignment = 1;
    graphicsAllocation.prepareHostPtrForResidency(&csr);
    EXPECT_EQ(graphicsAllocation.hostPtrTaskCountAssignment, 0u);
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationsWithFragmentsWhenCallingForUpateFenceThenAllResidencyDataHasUpdatedValue) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.fragmentsStorage.fragmentCount = 3;

    auto residencyData1 = std::make_unique<ResidencyData>(3);
    auto residencyData2 = std::make_unique<ResidencyData>(3);
    auto residencyData3 = std::make_unique<ResidencyData>(3);

    graphicsAllocation.fragmentsStorage.fragmentStorageData[0].residency = residencyData1.get();
    graphicsAllocation.fragmentsStorage.fragmentStorageData[1].residency = residencyData2.get();
    graphicsAllocation.fragmentsStorage.fragmentStorageData[2].residency = residencyData3.get();

    uint64_t newFenceValue = 0x54321;
    auto contextId = 0u;

    graphicsAllocation.updateCompletionDataForAllocationAndFragments(newFenceValue, contextId);

    EXPECT_EQ(graphicsAllocation.getResidencyData().getFenceValueForContextId(contextId), newFenceValue);

    for (uint32_t allocationId = 0; allocationId < graphicsAllocation.fragmentsStorage.fragmentCount; allocationId++) {
        auto residencyData = graphicsAllocation.fragmentsStorage.fragmentStorageData[allocationId].residency;
        EXPECT_EQ(residencyData->getFenceValueForContextId(contextId), newFenceValue);
    }
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenHasAllocationReadOnlyTypeCalledThenFalseReturned) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::buffer;
    EXPECT_FALSE(graphicsAllocation.hasAllocationReadOnlyType());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsKernelIsaAndMaskSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::kernelIsa) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::kernelIsa;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsInternalIsaAndMaskSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::kernelIsaInternal) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::kernelIsaInternal;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsCommandBufferAndMaskSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::commandBuffer) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::commandBuffer;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsLinearStreamAndMaskSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::linearStream) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::linearStream;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsKernelIsaAndMaskDoesNotSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::linearStream) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::kernelIsa;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsInternalIsaAndMaskDoesNotSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::kernelIsa) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::kernelIsaInternal;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsCommandBufferAndMaskDoesNotSupportItThenAllocationHasReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::kernelIsaInternal) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::commandBuffer;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsLinearStreamAndMaskDoesNotSupportItThenAllocationHasNotReadonlyType) {
    DebugManagerStateRestore restorer;
    auto mask = 1llu << (static_cast<int64_t>(AllocationType::commandBuffer) - 1);
    debugManager.flags.ReadOnlyAllocationsTypeMask.set(mask);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::linearStream;
    EXPECT_FALSE(graphicsAllocation.hasAllocationReadOnlyType());
}
TEST(GraphicsAllocationTest, givenGraphicsAllocationsWhenAllocationTypeIsRingBufferThenAllocationHasReadonlyType) {
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.hasAllocationReadOnlyTypeCallBase = true;
    graphicsAllocation.allocationType = AllocationType::ringBuffer;
    EXPECT_TRUE(graphicsAllocation.hasAllocationReadOnlyType());
}
