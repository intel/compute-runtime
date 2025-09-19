/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/properties_helper.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {
class Context;
class GmmHelper;
} // namespace NEO

using namespace NEO;

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
            return memObj->getMultiGraphicsAllocation().getDefaultGraphicsAllocation();
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
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  sizeof(buffer), buffer, buffer, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAllocation), true, false, false);

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
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  size, hostPtr, hostPtr, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAllocation), true, false, false);

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
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_PIPE, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  size, hostPtr, hostPtr, GraphicsAllocationHelper::toMultiGraphicsAllocation(mockAllocation), true, false, false);

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
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, &hostPtr, 0, true, false, false);

    EXPECT_EQ(&hostPtr, memObj.getBasePtrForMap(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenHostPtrWithoutUseHostPtrFlagWhenAskingForBaseMapPtrThenReturnAllocatedPtr) {
    uint8_t hostPtr = 0;
    MockContext context;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  1, nullptr, &hostPtr, 0, true, false, false);

    EXPECT_NE(&hostPtr, memObj.getBasePtrForMap(context.getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(memObj.getAllocatedMapPtr(), memObj.getBasePtrForMap(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenMemObjWhenReleaseAllocatedPtrIsCalledTwiceThenItDoesntCrash) {
    void *allocatedPtr = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    MockContext context;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, 0, true, false, false);

    memObj.setAllocatedMapPtr(allocatedPtr);
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
    memObj.releaseAllocatedMapPtr();
    EXPECT_EQ(nullptr, memObj.getAllocatedMapPtr());
}

TEST(MemObj, givenNotReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsAddedToMemoryManagerAllocationList) {
    MockContext context;

    auto memoryManager = context.getDevice(0)->getExecutionEnvironment()->memoryManager.get();

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto defaultEngine = context.getDevice(0)->getDefaultEngine();
    allocation->updateTaskCount(2, defaultEngine.osContext->getContextId());
    *(defaultEngine.commandStreamReceiver->getTagAddress()) = 1;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);
    auto &allocationList = defaultEngine.commandStreamReceiver->getDeferredAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_FALSE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenReadyGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));
    MockContext context(device.get());
    auto memoryManager = executionEnvironment->memoryManager.get();

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation->updateTaskCount(1, device->getDefaultEngine().osContext->getContextId());
    *device->getDefaultEngine().commandStreamReceiver->getTagAddress() = 1;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);

    auto &allocationList = device->getDefaultEngine().commandStreamReceiver->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenNotUsedGraphicsAllocationWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);

    auto &allocationList = context.getDevice(0)->getDefaultEngine().commandStreamReceiver->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenMemoryManagerWithoutDeviceWhenMemObjDestroysAllocationAsyncThenAllocationIsNotAddedToMemoryManagerAllocationList) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);

    auto &allocationList = context.getDevice(0)->getDefaultEngine().commandStreamReceiver->getTemporaryAllocations();
    EXPECT_TRUE(allocationList.peekIsEmpty());
    memObj.destroyGraphicsAllocation(allocation, true);

    EXPECT_TRUE(allocationList.peekIsEmpty());
}

TEST(MemObj, givenMemObjAndPointerToObjStorageWithProperCommandWhenCheckIfMemTransferRequiredThenReturnFalse) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);
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
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);
    void *ptr = memObj.getCpuAddressForMemoryTransfer();
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_FILL_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}

TEST(MemObj, givenMemObjAndPointerToDifferentStorageAndProperCommandWhenCheckIfMemTransferRequiredThenReturnTrue) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);
    void *ptr = (void *)0x1234;
    bool isMemTransferNeeded = memObj.checkIfMemoryTransferIsRequired(0, 0, ptr, CL_COMMAND_WRITE_BUFFER);
    EXPECT_TRUE(isMemTransferNeeded);
}

TEST(MemObj, givenSharingHandlerWhenAskedForCpuMappingThenReturnFalse) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  MemoryConstants::pageSize, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false);
    memObj.setSharingHandler(new SharingHandler());
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenTiledObjectWhenAskedForCpuMappingThenReturnFalse) {
    struct MyMemObj : public MemObj {
        using MemObj::MemObj;
        bool isTiledAllocation() const override { return true; }
    };
    MockContext context;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MyMemObj memObj(nullptr, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                    MemoryConstants::pageSize, nullptr, nullptr, 0, true, false, false);

    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenCompressedGmmWhenAskingForMappingOnCpuThenDisallow) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCpuCacheForResources.set(true);

    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation->setDefaultGmm(new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                  1, allocation->getUnderlyingBuffer(), nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), false, false, false);

    allocation->getDefaultGmm()->setCompressionEnabled(false);
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
    allocation->getDefaultGmm()->setCompressionEnabled(true);
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenDefaultWhenAskedForCpuMappingThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCpuCacheForResources.set(true);

    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  64, allocation->getUnderlyingBuffer(), nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false);

    EXPECT_FALSE(memObj.isTiledAllocation());
    EXPECT_FALSE(memObj.peekSharingHandler());
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
}

struct MyMockGmm : Gmm {
    using Gmm::Gmm;
    using Gmm::preferNoCpuAccess;
};
TEST(MemObj, givenCpuAccessNotAllowedWhenAskedForCpuMappingThenReturnFalse) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableCpuCacheForResources.set(true);

    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_COPY_HOST_PTR, 0,
                  64, allocation->getUnderlyingBuffer(), nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation->setDefaultGmm(new MyMockGmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    EXPECT_TRUE(memObj.mappingOnCpuAllowed());

    static_cast<MyMockGmm *>(memObj.getGraphicsAllocation(0)->getDefaultGmm())->preferNoCpuAccess = true;
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenNonCpuAccessibleMemoryWhenAskingForMappingOnCpuThenDisallow) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCpuCacheForResources.set(true);

    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());

    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation->setDefaultGmm(new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                  1, allocation->getUnderlyingBuffer(), nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), false, false, false);

    EXPECT_TRUE(memObj.mappingOnCpuAllowed());
    reinterpret_cast<MemoryAllocation *>(allocation)->overrideMemoryPool(MemoryPool::systemCpuInaccessible);
    EXPECT_FALSE(memObj.mappingOnCpuAllowed());
}

TEST(MemObj, givenMultipleMemObjectsWithReusedGraphicsAllocationWhenDestroyedThenFreeAllocationOnce) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});

    std::unique_ptr<MemObj> memObj1(new MemObj(&context, CL_MEM_OBJECT_BUFFER, {}, 0, 0, 1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false));
    memObj1->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj2(new MemObj(&context, CL_MEM_OBJECT_BUFFER, {}, 0, 0, 1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false));
    memObj2->setSharingHandler(new MySharingHandler(allocation));

    std::unique_ptr<MemObj> memObj3(new MemObj(&context, CL_MEM_OBJECT_BUFFER, {}, 0, 0, 1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false));
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
        MemObj memObj(&context, 0, {}, 0, 0, 0, nullptr, nullptr, 0, false, false, false);
        EXPECT_EQ(2, context.getRefInternalCount());
    }
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST(MemObj, givenSharedMemObjectWithNullGfxAllocationWhenSettingGfxAllocationThenSucceed) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;
    MockGraphicsAllocation *gfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, 0, true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    memObj.resetGraphicsAllocation(gfxAllocation);
    gfxAllocation->incReuseCount();

    ASSERT_EQ(1u, gfxAllocation->peekReuseCount());
    EXPECT_EQ(gfxAllocation, memObj.getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenSharedMemObjectAndGfxAllocationWhenGraphicsAllocationIsRemovedThenTheAllocationIsNotAvailable) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;
    MockGraphicsAllocation *graphicsAllocation = new MockGraphicsAllocation(nullptr, 0);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAllocation), true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    graphicsAllocation->decReuseCount();
    memObj.removeGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(nullptr, memObj.getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenSharedMemObjectAndZeroReuseCountWhenChangingGfxAllocationThenOldAllocationIsDestroyed) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(oldGfxAllocation), true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    oldGfxAllocation->decReuseCount();
    memObj.resetGraphicsAllocation(newGfxAllocation);
    newGfxAllocation->incReuseCount();

    ASSERT_EQ(1u, newGfxAllocation->peekReuseCount());
    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenSharedMemObjectAndNonZeroReuseCountWhenChangingGfxAllocationThenOldAllocationIsNotDestroyed) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(oldGfxAllocation), true, false, false);
    memObj.setSharingHandler(new MySharingHandler(&memObj));

    memObj.resetGraphicsAllocation(newGfxAllocation);
    newGfxAllocation->incReuseCount();

    ASSERT_EQ(1u, newGfxAllocation->peekReuseCount());
    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
    memoryManager.checkGpuUsageAndDestroyGraphicsAllocations(oldGfxAllocation);
}

TEST(MemObj, givenNotSharedMemObjectWhenChangingGfxAllocationThenOldAllocationIsDestroyed) {
    MockContext context;
    MockMemoryManager memoryManager(*context.getDevice(0)->getExecutionEnvironment());
    context.memoryManager = &memoryManager;
    MockGraphicsAllocation *oldGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    MockGraphicsAllocation *newGfxAllocation = new MockGraphicsAllocation(nullptr, 0);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_USE_HOST_PTR, 0,
                  1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(oldGfxAllocation), true, false, false);

    memObj.resetGraphicsAllocation(newGfxAllocation);

    EXPECT_EQ(newGfxAllocation, memObj.getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
}

TEST(MemObj, givenGraphicsAllocationWhenCallingIsAllocDumpableThenItReturnsTheCorrectValue) {
    MockGraphicsAllocation gfxAllocation(nullptr, 0);
    EXPECT_FALSE(gfxAllocation.isAllocDumpable());
    gfxAllocation.setAllocDumpable(true, false);
    EXPECT_TRUE(gfxAllocation.isAllocDumpable());
}

TEST(MemObj, givenMemObjNotUsingHostPtrWhenGettingBasePtrTwiceThenReturnSameMapPtr) {
    MockContext context;
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice());
    MemObj memObj(&context, CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                  1, nullptr, nullptr, 0, true, false, false);

    void *mapPtr = memObj.getBasePtrForMap(context.getDevice(0)->getRootDeviceIndex());
    EXPECT_NE(nullptr, mapPtr);
    auto mapAllocation = memObj.getMapAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, mapAllocation);
    EXPECT_EQ(mapPtr, mapAllocation->getUnderlyingBuffer());
    EXPECT_EQ(mapPtr, memObj.getAllocatedMapPtr());
}

using MemObjMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(MemObjMultiRootDeviceTests, WhenMemObjMapIsCreatedThenAllocationHasCorrectRootDeviceIndex) {
    auto allocation = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device1->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(0)->getDevice());
    std::unique_ptr<MemObj> memObj(
        new MemObj(context.get(), CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                   1, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation), true, false, false));

    void *mapPtr = memObj->getBasePtrForMap(device1->getRootDeviceIndex());
    EXPECT_NE(nullptr, mapPtr);

    auto mapAllocation = memObj->getMapAllocation(device1->getRootDeviceIndex());
    ASSERT_NE(nullptr, mapAllocation);
    EXPECT_EQ(expectedRootDeviceIndex, mapAllocation->getRootDeviceIndex());

    memObj.reset(nullptr);
}

TEST_F(MemObjMultiRootDeviceTests, WhenMemObjIsCreatedWithMultiGraphicsAllocationThenAllAllocationAreDestroyedProperly) {
    auto allocation0 = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    auto allocation1 = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{1, MemoryConstants::pageSize});

    auto multiGraphicsAllocation = MultiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(allocation0);
    multiGraphicsAllocation.addAllocation(allocation1);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(0)->getDevice());
    std::unique_ptr<MemObj> memObj(
        new MemObj(context.get(), CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                   1, nullptr, nullptr, std::move(multiGraphicsAllocation), true, false, false));

    EXPECT_NE(nullptr, memObj->getMultiGraphicsAllocation().getGraphicsAllocation(0));
    EXPECT_NE(nullptr, memObj->getMultiGraphicsAllocation().getGraphicsAllocation(1));

    EXPECT_NE(memObj->getMultiGraphicsAllocation().getGraphicsAllocation(0), memObj->getMultiGraphicsAllocation().getGraphicsAllocation(1));

    memObj.reset(nullptr);
}

TEST_F(MemObjMultiRootDeviceTests, WhenMemObjIsCreatedWithMcsAllocationForNonDefaultRootDeviceThenAllAllocationAreDestroyedProperly) {
    auto allocation = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{1, MemoryConstants::pageSize});

    auto multiGraphicsAllocation = MultiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(allocation);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(0)->getDevice());
    std::unique_ptr<MemObj> memObj(
        new MemObj(context.get(), CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                   1, nullptr, nullptr, std::move(multiGraphicsAllocation), true, false, false));

    EXPECT_EQ(nullptr, memObj->getMultiGraphicsAllocation().getGraphicsAllocation(0));
    EXPECT_NE(nullptr, memObj->getMultiGraphicsAllocation().getGraphicsAllocation(1));

    auto mcsAllocation = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    memObj->setMcsAllocation(mcsAllocation);

    memObj.reset(nullptr);
}

TEST_F(MemObjMultiRootDeviceTests, WhenMemObjMapAreCreatedThenAllAllocationAreDestroyedProperly) {
    auto allocation0 = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{2, MemoryConstants::pageSize});
    auto allocation1 = mockMemoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{1, MemoryConstants::pageSize});

    auto multiGraphicsAllocation = MultiGraphicsAllocation(2);
    multiGraphicsAllocation.addAllocation(allocation0);
    multiGraphicsAllocation.addAllocation(allocation1);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(1)->getDevice());
    std::unique_ptr<MemObj> memObj(
        new MemObj(context.get(), CL_MEM_OBJECT_BUFFER, memoryProperties, CL_MEM_READ_WRITE, 0,
                   1, nullptr, nullptr, std::move(multiGraphicsAllocation), true, false, false));

    auto mapAllocation0 = memObj->getMapAllocation(2);
    auto mapAllocation1 = memObj->getMapAllocation(1);

    EXPECT_EQ(nullptr, mapAllocation0);
    EXPECT_EQ(nullptr, mapAllocation1);

    EXPECT_NE(nullptr, memObj->getBasePtrForMap(2));
    EXPECT_EQ(memObj->getBasePtrForMap(2), memObj->getBasePtrForMap(1));

    mapAllocation0 = memObj->getMapAllocation(2);
    mapAllocation1 = memObj->getMapAllocation(1);

    ASSERT_NE(nullptr, mapAllocation0);
    ASSERT_NE(nullptr, mapAllocation1);
    ASSERT_NE(mapAllocation0, mapAllocation1);

    memObj.reset(nullptr);
}
