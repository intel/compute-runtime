/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/mem_obj/mem_obj.h"
#include "runtime/device/device.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(MemObj, useCount) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  sizeof(buffer), buffer, buffer, mockAllocation, true, false, false);

    EXPECT_EQ(ObjectNotResident, memObj.getGraphicsAllocation()->residencyTaskCount);
    memObj.getGraphicsAllocation()->residencyTaskCount = 1;
    EXPECT_EQ(1, memObj.getGraphicsAllocation()->residencyTaskCount);
    memObj.getGraphicsAllocation()->residencyTaskCount--;
    EXPECT_EQ(0, memObj.getGraphicsAllocation()->residencyTaskCount);
}

TEST(MemObj, GivenMemObjWhenInititalizedFromHostPtrThenInitializeFields) {
    const size_t size = 64;
    char buffer[size];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  sizeof(buffer), buffer, buffer, mockAllocation, true, false, false);

    EXPECT_EQ(&buffer, memObj.getCpuAddress());
    EXPECT_EQ(&buffer, memObj.getHostPtr());
    EXPECT_EQ(size, memObj.getSize());
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), memObj.getFlags());
    EXPECT_EQ(nullptr, memObj.getMappedPtr());
    EXPECT_EQ(0u, memObj.getMappedSize());
    EXPECT_EQ(0u, memObj.getMappedOffset());
}

TEST(MemObj, GivenMemObjectWhenAskedForTransferDataThenNullPtrIsReturned) {
    char buffer[64];
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(buffer, sizeof(buffer));
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  sizeof(buffer), buffer, buffer, mockAllocation, true, false, false);

    auto ptr = memObj.transferDataToHostPtr();
    EXPECT_EQ(nullptr, ptr);
}

TEST(MemObj, givenMemObjWhenAllocatedMappedPtrIsSetThenGetMappedPtrIsDifferentThanAllocatedMappedPtr) {
    void *mockPtr = (void *)0x01234;
    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_EQ(nullptr, memObj.getAllocatedMappedPtr());
    EXPECT_EQ(nullptr, memObj.getMappedPtr());
    memObj.setAllocatedMappedPtr(mockPtr);
    EXPECT_EQ(mockPtr, memObj.getAllocatedMappedPtr());
    EXPECT_NE(mockPtr, memObj.getMappedPtr());
    memObj.setAllocatedMappedPtr(nullptr);
}

TEST(MemObj, givenMemObjWhenReleaseAllocatedPtrIsCalledTwiceThenItDoesntCrash) {
    void *allocatedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, nullptr, true, false, false);

    memObj.setAllocatedMappedPtr(allocatedPtr);
    memObj.releaseAllocatedMappedPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMappedPtr());
    memObj.releaseAllocatedMappedPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMappedPtr());
}

TEST(MemObj, givenNotReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = 2;
    *memoryManager.device->getTagAddress() = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_FALSE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = 1;
    *memoryManager.device->getTagAddress() = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenNotUsedGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);
    memoryManager.setDevice(context.getDevice(0));

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocation->taskCount = ObjectNotUsed;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenMemoryManagerWithoutDeviceWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(memoryManager.isAllocationListEmpty());
}

TEST(MemObj, givenMemObjWhenItDoesntHaveGraphicsAllocationThenWaitForCsrCompletionDoesntCrash) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_EQ(nullptr, memoryManager.device);
    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation());
    memObj.waitForCsrCompletion();

    memoryManager.setDevice(context.getDevice(0));

    EXPECT_NE(nullptr, memoryManager.device);
    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation());
    memObj.waitForCsrCompletion();
}
TEST(MemObj, givenMemObjAndPointerToObjStorageWithProperCommandWhenCheckIfMemTransferRequiredThenReturnFalse) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = memObj.getCpuAddressForMemoryTransfer();
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_BUFFER);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER_RECT);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_BUFFER_RECT);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_IMAGE);
    EXPECT_FALSE(isMemTransferNeeded);

    isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_READ_IMAGE);
    EXPECT_FALSE(isMemTransferNeeded);
}
TEST(MemObj, givenMemObjAndPointerToObjStorageBadCommandWhenCheckIfMemTransferRequiredThenReturnTrue) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = memObj.getCpuAddressForMemoryTransfer();
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_FILL_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}
TEST(MemObj, givenMemObjAndPointerToDiffrentStorageAndProperCommandWhenCheckIfMemTransferRequiredThenReturnTrue) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    void *ptr = (void *)0x1234;
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}

TEST(MemObj, givenSharingHandlerWhenAskedForCpuMappingThenReturnFalse) {
    MemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    memObj.setSharingHandler(new SharingHandler());
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenTiledObjectWhenAskedForCpuMappingThenReturnFalse) {
    struct MyMemObj : public MemObj {
        using MemObj::MemObj;
        bool allowTiling() const override { return true; }
    };
    MyMemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                    MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenDefaultWhenAskedForCpuMappingThenReturnTrue) {
    MemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    EXPECT_FALSE(memObj.allowTiling());
    EXPECT_FALSE(memObj.peekSharingHandler());
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
}
