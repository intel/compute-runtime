/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"

#include "gtest/gtest.h"

#include <memory>
#include <type_traits>

using namespace NEO;

TEST(KernelInfo, WhenKernelInfoIsCreatedThenItIsNotMoveableAndNotCopyable) {
    static_assert(false == std::is_move_constructible<KernelInfo>::value, "");
    static_assert(false == std::is_copy_constructible<KernelInfo>::value, "");
    static_assert(false == std::is_move_assignable<KernelInfo>::value, "");
    static_assert(false == std::is_copy_assignable<KernelInfo>::value, "");
}

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationThenCopyWholeKernelHeapToKernelAllocation) {
    KernelInfo kernelInfo;
    auto factory = UltDeviceFactory{1, 0};
    auto device = factory.rootDevices[0];
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelInfo.heapInfo.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    for (size_t i = 0; i < heapSize; i++) {
        heap[i] = static_cast<char>(i);
    }

    auto retVal = kernelInfo.createKernelAllocation(*device, false);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), heap, heapSize));
    size_t isaPadding = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getPaddingForISAAllocation();
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), heapSize + isaPadding);
    device->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

TEST(KernelInfoTest, givenKernelInfoWhenCreatingKernelAllocationWithInternalIsaFalseTypeThenCorrectAllocationTypeIsUsed) {
    KernelInfo kernelInfo;
    auto factory = UltDeviceFactory{1, 0};
    auto device = factory.rootDevices[0];
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelInfo.heapInfo.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    auto retVal = kernelInfo.createKernelAllocation(*device, false);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    EXPECT_EQ(GraphicsAllocation::AllocationType::KERNEL_ISA, allocation->getAllocationType());
    device->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

TEST(KernelInfoTest, givenKernelInfoWhenCreatingKernelAllocationWithInternalIsaTrueTypeThenCorrectAllocationTypeIsUsed) {
    KernelInfo kernelInfo;
    auto factory = UltDeviceFactory{1, 0};
    auto device = factory.rootDevices[0];
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelInfo.heapInfo.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    auto retVal = kernelInfo.createKernelAllocation(*device, true);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    EXPECT_EQ(GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL, allocation->getAllocationType());
    device->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

class MyMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    GraphicsAllocation *allocate32BitGraphicsMemoryImpl(const AllocationData &allocationData, bool useLocalMemory) override { return nullptr; }
};

TEST(KernelInfoTest, givenKernelInfoWhenCreateKernelAllocationAndCannotAllocateMemoryThenReturnsFalse) {
    KernelInfo kernelInfo;
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get());
    executionEnvironment->memoryManager.reset(new MyMemoryManager(*executionEnvironment));
    if (executionEnvironment->memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto device = std::unique_ptr<Device>(Device::create<RootDevice>(executionEnvironment, mockRootDeviceIndex));
    auto retVal = kernelInfo.createKernelAllocation(*device, false);
    EXPECT_FALSE(retVal);
}

using KernelInfoMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(KernelInfoMultiRootDeviceTests, WhenCreatingKernelAllocationThenItHasCorrectRootDeviceIndex) {
    KernelInfo kernelInfo;
    const size_t heapSize = 0x40;
    char heap[heapSize];
    kernelInfo.heapInfo.KernelHeapSize = heapSize;
    kernelInfo.heapInfo.pKernelHeap = &heap;

    auto retVal = kernelInfo.createKernelAllocation(device1->getDevice(), false);
    EXPECT_TRUE(retVal);
    auto allocation = kernelInfo.kernelAllocation;
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectedRootDeviceIndex, allocation->getRootDeviceIndex());
    mockMemoryManager->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
}

TEST(KernelInfo, whenGetKernelNamesStringIsCalledThenNamesAreProperlyConcatenated) {
    ExecutionEnvironment execEnv;
    KernelInfo kernel1 = {};
    kernel1.kernelDescriptor.kernelMetadata.kernelName = "kern1";
    KernelInfo kernel2 = {};
    kernel2.kernelDescriptor.kernelMetadata.kernelName = "kern2";
    std::vector<KernelInfo *> kernelInfoArray;
    kernelInfoArray.push_back(&kernel1);
    kernelInfoArray.push_back(&kernel2);
    EXPECT_STREQ("kern1;kern2", concatenateKernelNames(kernelInfoArray).c_str());
}
