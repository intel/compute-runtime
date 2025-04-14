/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

typedef Test<ClDeviceFixture> KernelSubstituteTest;

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithGreaterSizeThenAllocatesNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.kernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(*pDevice, false);
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    auto &helper = pClDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(firstAllocationSize, initialHeapSize + isaPadding);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize + 1;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_NE(secondAllocationSize, initialHeapSize + isaPadding);
    EXPECT_EQ(secondAllocationSize, newHeapSize + isaPadding);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_NE(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithSameSizeThenDoesNotAllocateNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.kernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(*pDevice, false);
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    auto &helper = pClDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(firstAllocationSize, initialHeapSize + isaPadding);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(secondAllocationSize, initialHeapSize + isaPadding);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_EQ(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithSmallerSizeThenDoesNotAllocateNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.kernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(*pDevice, false);
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    auto &helper = pClDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(firstAllocationSize, initialHeapSize + isaPadding);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize - 1;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(secondAllocationSize, initialHeapSize + isaPadding);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_EQ(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWithUsedKernelAllocationWhenSubstituteKernelHeapAndAllocateNewMemoryThenStoreOldAllocationOnTemporaryList) {
    MockKernelWithInternals kernel(*pClDevice);
    auto memoryManager = pDevice->getMemoryManager();
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();

    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.kernelHeapSize = initialHeapSize;

    kernel.kernelInfo.createKernelAllocation(*pDevice, false);
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;

    TaskCountType notReadyTaskCount = *commandStreamReceiver.getTagAddress() + 1u;

    firstAllocation->updateTaskCount(notReadyTaskCount, commandStreamReceiver.getOsContext().getContextId());

    const size_t newHeapSize = initialHeapSize + 1;
    char newHeap[newHeapSize];

    EXPECT_TRUE(commandStreamReceiver.getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(commandStreamReceiver.getDeferredAllocations().peekIsEmpty());

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;

    EXPECT_TRUE(commandStreamReceiver.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(commandStreamReceiver.getDeferredAllocations().peekIsEmpty());
    EXPECT_EQ(commandStreamReceiver.getDeferredAllocations().peekHead(), firstAllocation);
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
    commandStreamReceiver.getInternalAllocationStorage()->cleanAllocationList(notReadyTaskCount, TEMPORARY_ALLOCATION);
}
