/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> KernelSubstituteTest;

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithGreaterSizeThenAllocatesNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    auto pHeader = const_cast<SKernelBinaryHeaderCommon *>(kernel.kernelInfo.heapInfo.pKernelHeader);
    const size_t initialHeapSize = 0x40;
    pHeader->KernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(pDevice->getRootDeviceIndex(), pDevice->getMemoryManager());
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(initialHeapSize, firstAllocationSize);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize + 1;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_NE(initialHeapSize, secondAllocationSize);
    EXPECT_EQ(newHeapSize, secondAllocationSize);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_NE(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithSameSizeThenDoesNotAllocateNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    auto pHeader = const_cast<SKernelBinaryHeaderCommon *>(kernel.kernelInfo.heapInfo.pKernelHeader);
    const size_t initialHeapSize = 0x40;
    pHeader->KernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(pDevice->getRootDeviceIndex(), pDevice->getMemoryManager());
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(initialHeapSize, firstAllocationSize);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(initialHeapSize, secondAllocationSize);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_EQ(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWhenSubstituteKernelHeapWithSmallerSizeThenDoesNotAllocateNewKernelAllocation) {
    MockKernelWithInternals kernel(*pClDevice);
    auto pHeader = const_cast<SKernelBinaryHeaderCommon *>(kernel.kernelInfo.heapInfo.pKernelHeader);
    const size_t initialHeapSize = 0x40;
    pHeader->KernelHeapSize = initialHeapSize;

    EXPECT_EQ(nullptr, kernel.kernelInfo.kernelAllocation);
    kernel.kernelInfo.createKernelAllocation(pDevice->getRootDeviceIndex(), pDevice->getMemoryManager());
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, firstAllocation);
    auto firstAllocationSize = firstAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(initialHeapSize, firstAllocationSize);

    auto firstAllocationId = static_cast<MemoryAllocation *>(firstAllocation)->id;

    const size_t newHeapSize = initialHeapSize - 1;
    char newHeap[newHeapSize];

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;
    EXPECT_NE(nullptr, secondAllocation);
    auto secondAllocationSize = secondAllocation->getUnderlyingBufferSize();
    EXPECT_EQ(initialHeapSize, secondAllocationSize);
    auto secondAllocationId = static_cast<MemoryAllocation *>(secondAllocation)->id;

    EXPECT_EQ(firstAllocationId, secondAllocationId);

    pDevice->getMemoryManager()->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
}

TEST_F(KernelSubstituteTest, givenKernelWithUsedKernelAllocationWhenSubstituteKernelHeapAndAllocateNewMemoryThenStoreOldAllocationOnTemporaryList) {
    MockKernelWithInternals kernel(*pClDevice);
    auto pHeader = const_cast<SKernelBinaryHeaderCommon *>(kernel.kernelInfo.heapInfo.pKernelHeader);
    auto memoryManager = pDevice->getMemoryManager();
    auto &commandStreamReceiver = pDevice->getGpgpuCommandStreamReceiver();

    const size_t initialHeapSize = 0x40;
    pHeader->KernelHeapSize = initialHeapSize;

    kernel.kernelInfo.createKernelAllocation(pDevice->getRootDeviceIndex(), memoryManager);
    auto firstAllocation = kernel.kernelInfo.kernelAllocation;

    uint32_t notReadyTaskCount = *commandStreamReceiver.getTagAddress() + 1u;

    firstAllocation->updateTaskCount(notReadyTaskCount, commandStreamReceiver.getOsContext().getContextId());

    const size_t newHeapSize = initialHeapSize + 1;
    char newHeap[newHeapSize];

    EXPECT_TRUE(commandStreamReceiver.getTemporaryAllocations().peekIsEmpty());

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);
    auto secondAllocation = kernel.kernelInfo.kernelAllocation;

    EXPECT_FALSE(commandStreamReceiver.getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(commandStreamReceiver.getTemporaryAllocations().peekHead(), firstAllocation);
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(secondAllocation);
    commandStreamReceiver.getInternalAllocationStorage()->cleanAllocationList(notReadyTaskCount, TEMPORARY_ALLOCATION);
}
