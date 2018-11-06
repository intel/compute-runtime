/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/memory_manager/allocations_list.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "gtest/gtest.h"

using namespace OCLRT;

struct MySharingHandler : public SharingHandler {
    MySharingHandler(MemObj *memObj) : memObj(memObj) {
        auto alloc = getAllocation();
        if (alloc) {
            alloc->incReuseCount();
        }
    }
    MySharingHandler(GraphicsAllocation *allocation) : allocation(allocation) {
        auto alloc = getAllocation();
        if (alloc) {
            alloc->incReuseCount();
        }
    }

    void releaseReusedGraphicsAllocation() override {
        auto alloc = getAllocation();
        if (alloc) {
            alloc->decReuseCount();
        }
    }

    GraphicsAllocation *getAllocation() {
        if (memObj) {
            return memObj->getGraphicsAllocation();
        }
        return allocation;
    }

    MemObj *memObj = nullptr;
    GraphicsAllocation *allocation = nullptr;
};

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
}

TEST(MemObj, givenMemObjectWhenAskedForTransferToHostPtrThenDoNothing) {
    const size_t size = 64;
    uint8_t hostPtr[size] = {};
    uint8_t expectedHostPtr[size] = {};
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(hostPtr, sizeof(hostPtr));
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  size, hostPtr, hostPtr, mockAllocation, true, false, false);

    memset(memObj.getCpuAddress(), 123, size);
    memset(hostPtr, 0, size);

    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{size, 0, 0}};
    EXPECT_THROW(memObj.transferDataToHostPtr(copySize, copyOffset), std::exception);

    EXPECT_TRUE(memcmp(hostPtr, expectedHostPtr, size) == 0);
}

TEST(MemObj, givenMemObjectWhenAskedForTransferFromHostPtrThenDoNothing) {
    const size_t size = 64;
    uint8_t hostPtr[size] = {};
    uint8_t expectedBufferPtr[size] = {};
    MockContext context;
    MockGraphicsAllocation *mockAllocation = new MockGraphicsAllocation(hostPtr, sizeof(hostPtr));
    MemObj memObj(&context, CL_MEM_OBJECT_PIPE, CL_MEM_USE_HOST_PTR,
                  size, hostPtr, hostPtr, mockAllocation, true, false, false);

    memset(memObj.getCpuAddress(), 123, size);
    memset(expectedBufferPtr, 123, size);

    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{size, 0, 0}};
    EXPECT_THROW(memObj.transferDataFromHostPtr(copySize, copyOffset), std::exception);

    EXPECT_TRUE(memcmp(memObj.getCpuAddress(), expectedBufferPtr, size) == 0);
}

TEST(MemObj, givenHostPtrAndUseHostPtrFlagWhenAskingForBaseMapPtrThenReturnHostPtr) {
    uint8_t hostPtr = 0;
    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, &hostPtr, nullptr, true, false, false);

    EXPECT_EQ(&hostPtr, memObj.getBasePtrForMap());
}

TEST(MemObj, givenHostPtrWithoutUseHostPtrFlagWhenAskingForBaseMapPtrThenReturnAllocatedPtr) {
    uint8_t hostPtr = 0;
    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  1, nullptr, &hostPtr, nullptr, true, false, false);

    EXPECT_NE(&hostPtr, memObj.getBasePtrForMap());
    EXPECT_EQ(memObj.getAllocatedMapPtr(), memObj.getBasePtrForMap());
}

TEST(MemObj, givenMemObjWhenReleaseAllocatedPtrIsCalledTwiceThenItDoesntCrash) {
    void *allocatedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    MockContext context;

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, nullptr, true, false, false);

    memObj.setAllocatedMapPtr(allocatedPtr);
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
}

TEST(MemObj, givenNotReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize);
    allocation->updateTaskCount(2, 0);
    *(memoryManager.getCommandStreamReceiver(0)->getTagAddress()) = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);
    auto &allocationList = memoryManager.getCommandStreamReceiver(0)->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_FALSE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize);
    allocation->updateTaskCount(1, 0);
    *context.getDevice(0)->getTagAddress() = 1;
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    auto &allocationList = memoryManager.getCommandStreamReceiver(0)->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenNotUsedGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize);
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    auto &allocationList = memoryManager.getCommandStreamReceiver(0)->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenMemoryManagerWithoutDeviceWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  MemoryConstants::pageSize, nullptr, nullptr, nullptr, true, false, false);

    auto &allocationList = memoryManager.getCommandStreamReceiver(0)->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
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

TEST(MemObj, givenRenderCompressedGmmWhenAskingForMappingOnCpuThenDisallow) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(1);
    allocation->gmm = new Gmm(nullptr, 1, false);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_READ_WRITE,
                  1, allocation->getUnderlyingBuffer(), nullptr, allocation, false, false, false);

    allocation->gmm->isRenderCompressed = false;
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
    allocation->gmm->isRenderCompressed = true;
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenDefaultWhenAskedForCpuMappingThenReturnTrue) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(64);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_COPY_HOST_PTR,
                  64, allocation->getUnderlyingBuffer(), nullptr, allocation, true, false, false);

    EXPECT_FALSE(memObj.allowTiling());
    EXPECT_FALSE(memObj.peekSharingHandler());
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenNonCpuAccessibleMemoryWhenAskingForMappingOnCpuThenDisallow) {
    MockMemoryManager memoryManager;
    MockContext context;

    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(1);
    allocation->gmm = new Gmm(nullptr, 1, false);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_READ_WRITE,
                  1, allocation->getUnderlyingBuffer(), nullptr, allocation, false, false, false);

    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
    reinterpret_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::SystemCpuInaccessible);
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenMultipleMemObjectsWithReusedGraphicsAllocationWhenDestroyedThenFreeAllocationOnce) {

    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);

    auto allocation = memoryManager.allocateGraphicsMemory(1);

    std::unique_ptr<MemObj> memObj1(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj1->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj2(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj2->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj3(new MemObj(&context, CL_MEM_OBJECT_BUFFER, 0, 1, nullptr, nullptr, allocation, true, false, false));
    memObj3->setSharingHandler(new MySharingHandler(allocation));

    EXPECT_EQ(3u, allocation->peekReuseCount());

    memObj3.reset(nullptr);
    EXPECT_EQ(2u, allocation->peekReuseCount());
    memObj1.reset(nullptr);
    EXPECT_EQ(1u, allocation->peekReuseCount());

    memObj2.reset(nullptr);
}

TEST(MemObj, givenMemObjectWhenContextIsNotNullThenContextOutlivesMemobjects) {
    MockContext context;
    EXPECT_EQ(1, context.getRefInternalCount());
    {
        MemObj memObj(&context, 0, 0, 0, nullptr, nullptr, nullptr, false, false, false);
        EXPECT_EQ(2, context.getRefInternalCount());
    }
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST(MemObj, givenSharedMemObjectWithNullGfxAllocationWhenSettingGfxAllocationThenSucceed) {
    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);
    MockGraphicsAllocation *gfxAllocation = new MockGraphicsAllocation(nullptr, 0);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, nullptr, true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    memObj.resetGraphicsAllocation(gfxAllocation);
    gfxAllocation->incReuseCount();

    ASSERT_EQ(1u, gfxAllocation->peekReuseCount());
    EXPECT_EQ(gfxAllocation, memObj.getGraphicsAllocation());
}

TEST(MemObj, givenSharedMemObjectAndNullGfxAllocationProvidedWhenSettingGfxAllocationThenSucceed) {
    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);
    MockGraphicsAllocation *graphicsAllocation = new MockGraphicsAllocation(nullptr, 0);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, graphicsAllocation, true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    graphicsAllocation->decReuseCount();
    memObj.resetGraphicsAllocation(nullptr);

    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation());
}

TEST(MemObj, givenSharedMemObjectAndZeroReuseCountWhenChangingGfxAllocationThenOldAllocationIsDestroyed) {
    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, oldGfxAllocation, true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    oldGfxAllocation->decReuseCount();
    memObj.resetGraphicsAllocation(newGfxAllocation);
    newGfxAllocation->incReuseCount();

    ASSERT_EQ(1u, newGfxAllocation->peekReuseCount());
    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation());
}

TEST(MemObj, givenSharedMemObjectAndNonZeroReuseCountWhenChangingGfxAllocationThenOldAllocationIsNotDestroyed) {
    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, oldGfxAllocation, true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    memObj.resetGraphicsAllocation(newGfxAllocation);
    newGfxAllocation->incReuseCount();

    ASSERT_EQ(1u, newGfxAllocation->peekReuseCount());
    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation());
    memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(oldGfxAllocation);
}

TEST(MemObj, givenNotSharedMemObjectWhenChangingGfxAllocationThenOldAllocationIsDestroyed) {
    MockMemoryManager memoryManager;
    MockContext context;
    context.setMemoryManager(&memoryManager);
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);

    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, CL_MEM_USE_HOST_PTR,
                  1, nullptr, nullptr, oldGfxAllocation, true, false, false);

    memObj.resetGraphicsAllocation(newGfxAllocation);

    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation());
}

TEST(MemObj, givenGraphicsAllocationWhenCallingIsAllocDumpableThenItReturnsTheCorrectValue) {
    MockGraphicsAllocation gfxAllocation(nullptr, 0);
    EXPECT_FALSE(gfxAllocation.isAllocDumpable());
    gfxAllocation.setAllocDumpable(true);
    EXPECT_TRUE(gfxAllocation.isAllocDumpable());
}
