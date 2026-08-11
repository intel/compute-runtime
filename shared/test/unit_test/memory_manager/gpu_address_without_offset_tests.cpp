/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(GpuAddressWithoutOffsetTest, givenAllocationOffsetWhenGettingGpuAddressWithoutOffsetThenOffsetIsNotApplied) {
    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0xFFFF800600EE0000ull, 0x1000);

    const auto addressWithoutOffset = allocation.getGpuAddressWithoutOffset();
    EXPECT_EQ(0xFFFF800600EE0000ull, addressWithoutOffset);
    EXPECT_EQ(allocation.getGpuAddress(), addressWithoutOffset);

    allocation.setAllocationOffset(0x4080);

    EXPECT_EQ(addressWithoutOffset, allocation.getGpuAddressWithoutOffset());
    EXPECT_EQ(addressWithoutOffset + 0x4080, allocation.getGpuAddress());
    EXPECT_NE(allocation.getGpuAddress(), allocation.getGpuAddressWithoutOffset());
}

TEST(GpuAddressWithoutOffsetTest, givenAllocationWhenGettingGpuBaseAddressThenHeapBaseIsReturned) {
    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0xFFFF800600EE0000ull, 0x1000);
    allocation.setGpuBaseAddress(0xFFFF800600000000ull);
    allocation.setAllocationOffset(0x4080);

    EXPECT_EQ(0xFFFF800600000000ull, allocation.getGpuBaseAddress());
    EXPECT_EQ(0xFFFF800600EE0000ull, allocation.getGpuAddressWithoutOffset());
    EXPECT_NE(allocation.getGpuBaseAddress(), allocation.getGpuAddressWithoutOffset());
}

using SvmAllocsTrackingTest = Test<SVMMemoryAllocatorFixture<false, 1u>>;

TEST_F(SvmAllocsTrackingTest, givenAllocationOffsetChangedAfterInsertWhenRemovingSvmAllocThenEntryIsErased) {
    MockGraphicsAllocation allocation(nullptr, 0xFFFF800600EE0000ull, 0x1000);
    SvmAllocationData svmData(mockRootDeviceIndex);
    svmData.gpuAllocations.addAllocation(&allocation);
    svmData.size = 0x1000;
    svmData.setAllocId(1u);

    svmManager->insertSVMAlloc(svmData);
    ASSERT_EQ(1u, svmManager->getNumAllocs());

    allocation.setAllocationOffset(0x480);

    svmManager->removeSVMAlloc(svmData);
    EXPECT_EQ(0u, svmManager->getNumAllocs());
    EXPECT_EQ(0u, svmManager->internalAllocationsMap.count(svmData.getAllocId()));
}

TEST_F(SvmAllocsTrackingTest, givenAllocationOffsetChangedAfterInsertWhenFreeingSvmDataThenEntryIsErased) {
    MockGraphicsAllocation allocation(nullptr, 0xFFFF800600EE0000ull, 0x1000);
    SvmAllocationData svmData(mockRootDeviceIndex);
    svmData.gpuAllocations.addAllocation(&allocation);
    svmData.size = 0x1000;
    svmData.setAllocId(1u);

    svmManager->insertSVMAlloc(svmData);
    ASSERT_EQ(1u, svmManager->getNumAllocs());

    allocation.setAllocationOffset(0x480);

    auto insertedData = svmManager->getSVMAlloc(reinterpret_cast<void *>(allocation.getGpuAddress()));
    ASSERT_NE(nullptr, insertedData);

    svmManager->freeSVMData(insertedData);
    EXPECT_EQ(0u, svmManager->getNumAllocs());
    EXPECT_EQ(0u, svmManager->internalAllocationsMap.count(svmData.getAllocId()));
}

TEST_F(SvmAllocsTrackingTest, givenAllocationOffsetWhenGettingSvmAllocThenAllocationIsFoundByUserPointer) {
    MockGraphicsAllocation allocation(nullptr, 0xFFFF800600EE0000ull, 0x1000);
    allocation.setAllocationOffset(0x800);
    SvmAllocationData svmData(mockRootDeviceIndex);
    svmData.gpuAllocations.addAllocation(&allocation);
    svmData.size = 0x1000;
    svmData.setAllocId(1u);

    svmManager->insertSVMAlloc(svmData);
    ASSERT_EQ(1u, svmManager->getNumAllocs());

    auto userPtr = reinterpret_cast<void *>(allocation.getGpuAddress());
    EXPECT_NE(nullptr, svmManager->getSVMAlloc(userPtr));
    EXPECT_NE(nullptr, svmManager->getSVMAlloc(reinterpret_cast<void *>(allocation.getGpuAddressWithoutOffset())));

    svmManager->removeSVMAlloc(svmData);
    EXPECT_EQ(0u, svmManager->getNumAllocs());
}

TEST_F(SvmAllocsTrackingTest, givenAllocationOffsetSetBeforeInsertWhenFreeingSvmDataThenOnlyMatchingEntryIsErased) {
    MockGraphicsAllocation offsetAllocation(nullptr, 0xFFFF800600EE0000ull, 0x1000);
    offsetAllocation.setAllocationOffset(0x480);
    SvmAllocationData offsetSvmData(mockRootDeviceIndex);
    offsetSvmData.gpuAllocations.addAllocation(&offsetAllocation);
    offsetSvmData.size = 0x1000;
    offsetSvmData.setAllocId(1u);
    svmManager->insertSVMAlloc(reinterpret_cast<void *>(offsetAllocation.getGpuAddress()), offsetSvmData);

    MockGraphicsAllocation higherAllocation(nullptr, 0xFFFF800600EF0000ull, 0x1000);
    SvmAllocationData higherSvmData(mockRootDeviceIndex);
    higherSvmData.gpuAllocations.addAllocation(&higherAllocation);
    higherSvmData.size = 0x1000;
    higherSvmData.setAllocId(2u);
    svmManager->insertSVMAlloc(reinterpret_cast<void *>(higherAllocation.getGpuAddress()), higherSvmData);
    ASSERT_EQ(2u, svmManager->getNumAllocs());

    auto insertedData = svmManager->getSVMAlloc(reinterpret_cast<void *>(offsetAllocation.getGpuAddress()));
    ASSERT_NE(nullptr, insertedData);

    svmManager->freeSVMData(insertedData);

    EXPECT_EQ(1u, svmManager->getNumAllocs());
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(reinterpret_cast<void *>(offsetAllocation.getGpuAddress())));
    EXPECT_NE(nullptr, svmManager->getSVMAlloc(reinterpret_cast<void *>(higherAllocation.getGpuAddress())));
}

using SvmDeferFreeTrackingTest = Test<SVMMemoryAllocatorFixture<true, 1u>>;

TEST_F(SvmDeferFreeTrackingTest, givenAllocationOffsetWhenDeferredFreeIsProcessedThenDeferFreeEntryIsErased) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 2));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device;

    auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
    ASSERT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);
    svmData->gpuAllocations.getDefaultGraphicsAllocation()->setAllocationOffset(0x480);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
    mockMemoryManager->deferAllocInUse = true;
    svmManager->freeSVMAllocDefer(ptr);
    ASSERT_EQ(1ul, svmManager->getNumDeferFreeAllocs());

    mockMemoryManager->deferAllocInUse = false;
    svmManager->freeSVMAllocDeferImpl();

    EXPECT_EQ(0ul, svmManager->getNumDeferFreeAllocs());
    EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
    EXPECT_EQ(0u, svmManager->getNumAllocs());
}
