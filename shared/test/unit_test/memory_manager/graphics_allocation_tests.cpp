/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenAllInspectionIdsAreSetToZero) {
    MockGraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0u, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
    for (auto i = 0u; i < MemoryManager::maxOsContextCount; i++) {
        EXPECT_EQ(0u, graphicsAllocation.getInspectionId(i));
    }
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenIsCreatedThenTaskCountsAreInitializedProperly) {
    GraphicsAllocation graphicsAllocation1(0, AllocationType::UNKNOWN, nullptr, 0u, 0u, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
    GraphicsAllocation graphicsAllocation2(0, AllocationType::UNKNOWN, nullptr, 0u, 0u, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
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

TEST(GraphicsAllocationTest, whenAllocationTypeIsCommandBufferThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::COMMAND_BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsConstantSurfaceThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::CONSTANT_SURFACE));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsGlobalSurfaceThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::GLOBAL_SURFACE));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsInternalHeapThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::INTERNAL_HEAP));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsKernelIsaThenCpuAccessIsNotRequired) {
    EXPECT_FALSE(GraphicsAllocation::isCpuAccessRequired(AllocationType::KERNEL_ISA));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsKernelIsaInternalThenCpuAccessIsNotRequired) {
    EXPECT_FALSE(GraphicsAllocation::isCpuAccessRequired(AllocationType::KERNEL_ISA_INTERNAL));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsLinearStreamThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::LINEAR_STREAM));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsPipeThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::PIPE));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsTimestampPacketThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::TIMESTAMP_PACKET_TAG_BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsGpuTimestampDeviceBufferThenCpuAccessIsRequired) {
    EXPECT_TRUE(GraphicsAllocation::isCpuAccessRequired(AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationRequiresCpuAccessThenAllocationIsLockable) {
    auto firstAllocationIdx = static_cast<int>(AllocationType::UNKNOWN);
    auto lastAllocationIdx = static_cast<int>(AllocationType::COUNT);

    for (int allocationIdx = firstAllocationIdx; allocationIdx != lastAllocationIdx; allocationIdx++) {
        auto allocationType = static_cast<AllocationType>(allocationIdx);
        if (GraphicsAllocation::isCpuAccessRequired(allocationType)) {
            EXPECT_TRUE(GraphicsAllocation::isLockable(allocationType));
        }
    }
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsISAThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::KERNEL_ISA));
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::KERNEL_ISA_INTERNAL));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsBufferThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsBufferHostMemoryThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::BUFFER_HOST_MEMORY));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsGpuTimestampDeviceBufferThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsSvmGpuThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::SVM_GPU));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsSharedResourceCopyThenAllocationIsLockable) {
    EXPECT_TRUE(GraphicsAllocation::isLockable(AllocationType::SHARED_RESOURCE_COPY));
}

TEST(GraphicsAllocationTest, whenAllocationTypeIsImageThenAllocationIsNotLockable) {
    EXPECT_FALSE(GraphicsAllocation::isLockable(AllocationType::IMAGE));
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
    EXPECT_EQ(0llu, graphicsAllocation.peekInternalHandle(nullptr));
}

TEST(GraphicsAllocationTest, givenGraphicsAllocationWhenQueryingUsedPageSizeThenCorrectSizeForMemoryPoolUsedIsReturned) {

    MemoryPool::Type page4kPools[] = {MemoryPool::MemoryNull,
                                      MemoryPool::System4KBPages,
                                      MemoryPool::System4KBPagesWith32BitGpuAddressing,
                                      MemoryPool::SystemCpuInaccessible};

    for (auto pool : page4kPools) {
        MockGraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0u, static_cast<osHandle>(1), pool, MemoryManager::maxOsContextCount);

        EXPECT_EQ(MemoryConstants::pageSize, graphicsAllocation.getUsedPageSize());
    }

    MemoryPool::Type page64kPools[] = {MemoryPool::System64KBPages,
                                       MemoryPool::System64KBPagesWith32BitGpuAddressing,
                                       MemoryPool::LocalMemory};

    for (auto pool : page64kPools) {
        MockGraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0u, 0, pool, MemoryManager::maxOsContextCount);

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
        graphicsAllocation.storageInfo.readOnlyMultiStorage = false;
        graphicsAllocation.storageInfo.memoryBanks = 0;
        graphicsAllocation.overrideMemoryPool(MemoryPool::MemoryNull);
    }

    void gfxAllocationEnableReadOnlyMultiStorage(uint32_t banks) {
        graphicsAllocation.storageInfo.cloningOfPageTables = false;
        graphicsAllocation.storageInfo.readOnlyMultiStorage = true;
        graphicsAllocation.storageInfo.memoryBanks = banks;
        graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<CommandStreamReceiver> aubCsr;
    MockGraphicsAllocation graphicsAllocation;
};

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationWhenIsAubWritableIsCalledThenTrueIsReturned) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    EXPECT_TRUE(aubCsr.isAubWritable(graphicsAllocation));

    gfxAllocationEnableReadOnlyMultiStorage(0b1111);
    EXPECT_TRUE(aubCsr.isAubWritable(graphicsAllocation));
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationThatHasPageTablesCloningWhenWriteableFlagsAreUsedThenDefaultBankIsUsed) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    graphicsAllocation.storageInfo.memoryBanks = 0x2;
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);
    graphicsAllocation.storageInfo.cloningOfPageTables = true;

    EXPECT_TRUE(aubCsr.isAubWritable(graphicsAllocation));

    //modify non default bank
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

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationWhenAubWritableIsSetToFalseThenAubWritableIsFalse) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    aubCsr.setAubWritable(false, graphicsAllocation);
    EXPECT_FALSE(aubCsr.isAubWritable(graphicsAllocation));

    gfxAllocationEnableReadOnlyMultiStorage(0b1111);
    aubCsr.setAubWritable(false, graphicsAllocation);
    EXPECT_FALSE(aubCsr.isAubWritable(graphicsAllocation));
}

HWTEST_F(GraphicsAllocationTests, givenMultiStorageGraphicsAllocationWhenAubWritableIsSetOnSpecificBanksThenCorrectValuesAreSet) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();
    gfxAllocationEnableReadOnlyMultiStorage(0b1010);

    aubCsr.setAubWritable(false, graphicsAllocation);
    EXPECT_EQ(graphicsAllocation.aubInfo.aubWritable, maxNBitValue(32) & ~(0b1010));

    EXPECT_FALSE(graphicsAllocation.isAubWritable(0b10));
    EXPECT_FALSE(graphicsAllocation.isAubWritable(0b1000));
    EXPECT_FALSE(graphicsAllocation.isAubWritable(0b1010));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b1));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b100));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b101));

    aubCsr.setAubWritable(true, graphicsAllocation);
    EXPECT_EQ(graphicsAllocation.aubInfo.aubWritable, maxNBitValue(32));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b1));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b10));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b100));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b1000));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b101));
    EXPECT_TRUE(graphicsAllocation.isAubWritable(0b1010));
}

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationWhenIsTbxWritableIsCalledThenTrueIsReturned) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    EXPECT_TRUE(aubCsr.isTbxWritable(graphicsAllocation));

    gfxAllocationEnableReadOnlyMultiStorage(0b1111);
    EXPECT_TRUE(aubCsr.isTbxWritable(graphicsAllocation));
};

HWTEST_F(GraphicsAllocationTests, givenGraphicsAllocationWhenTbxWritableIsSetToFalseThenTbxWritableIsFalse) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();

    gfxAllocationSetToDefault();
    aubCsr.setTbxWritable(false, graphicsAllocation);
    EXPECT_FALSE(aubCsr.isTbxWritable(graphicsAllocation));

    gfxAllocationEnableReadOnlyMultiStorage(0b1111);
    aubCsr.setTbxWritable(false, graphicsAllocation);
    EXPECT_FALSE(aubCsr.isTbxWritable(graphicsAllocation));
}

HWTEST_F(GraphicsAllocationTests, givenMultiStorageGraphicsAllocationWhenTbxWritableIsSetOnSpecificBanksThenCorrectValuesAreSet) {
    initializeCsr<FamilyType>();
    auto &aubCsr = getAubCsr<FamilyType>();
    gfxAllocationEnableReadOnlyMultiStorage(0b1010);

    aubCsr.setTbxWritable(false, graphicsAllocation);
    EXPECT_EQ(graphicsAllocation.aubInfo.tbxWritable, maxNBitValue(32) & ~(0b1010));

    EXPECT_FALSE(graphicsAllocation.isTbxWritable(0b10));
    EXPECT_FALSE(graphicsAllocation.isTbxWritable(0b1000));
    EXPECT_FALSE(graphicsAllocation.isTbxWritable(0b1010));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b1));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b100));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b101));

    aubCsr.setTbxWritable(true, graphicsAllocation);
    EXPECT_EQ(graphicsAllocation.aubInfo.tbxWritable, maxNBitValue(32));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b1));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b10));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b100));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b1000));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b101));
    EXPECT_TRUE(graphicsAllocation.isTbxWritable(0b1010));
}
