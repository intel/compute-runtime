/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_library.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/wddm_residency_controller.h"
#include "runtime/platform/platform.h"
#include "runtime/utilities/tag_allocator.h"

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/os_interface/windows/wddm_memory_manager_tests.h"

using namespace OCLRT;
using namespace ::testing;

void WddmMemoryManagerFixture::SetUp() {
    GmmEnvironmentFixture::SetUp();
    GdiDllFixture::SetUp();

    wddm = static_cast<WddmMock *>(Wddm::createWddm());
    if (platformDevices[0]->capabilityTable.ftrRenderCompressedBuffers || platformDevices[0]->capabilityTable.ftrRenderCompressedImages) {
        GMM_DEVICE_CALLBACKS_INT dummyDeviceCallbacks = {};
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};
        wddm->resetPageTableManager(GmmPageTableMngr::create(&dummyDeviceCallbacks, 0, &dummyTTCallbacks));
    }
    EXPECT_TRUE(wddm->init());
    constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
    wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);

    osInterface = std::make_unique<OSInterface>();
    osInterface->get()->setWddm(wddm);

    memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, executionEnvironment);
}

TEST(WddmMemoryManager, NonCopyable) {
    EXPECT_FALSE(std::is_move_constructible<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_constructible<WddmMemoryManager>::value);
}

TEST(WddmMemoryManager, NonAssignable) {
    EXPECT_FALSE(std::is_move_assignable<WddmMemoryManager>::value);
    EXPECT_FALSE(std::is_copy_assignable<WddmMemoryManager>::value);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenRegisteringOsContextThenResidencyControllersIsResizedAccordinglyToContextId) {
    memoryManager->registerOsContext(new OsContext(osInterface.get(), 0u));
    EXPECT_EQ(1u, memoryManager->residencyControllers.size());
    EXPECT_EQ(0u, memoryManager->residencyControllers[0]->getLastTrimFenceValue());

    memoryManager->registerOsContext(new OsContext(osInterface.get(), 2u));
    EXPECT_EQ(3u, memoryManager->residencyControllers.size());
    EXPECT_EQ(0u, memoryManager->residencyControllers[0]->getLastTrimFenceValue());
    EXPECT_EQ(nullptr, memoryManager->residencyControllers[1]);
    EXPECT_EQ(0u, memoryManager->residencyControllers[2]->getLastTrimFenceValue());

    memoryManager->registerOsContext(new OsContext(osInterface.get(), 1u));
    EXPECT_EQ(3u, memoryManager->residencyControllers.size());
    EXPECT_EQ(0u, memoryManager->residencyControllers[0]->getLastTrimFenceValue());
    EXPECT_EQ(0u, memoryManager->residencyControllers[1]->getLastTrimFenceValue());
    EXPECT_EQ(0u, memoryManager->residencyControllers[2]->getLastTrimFenceValue());
}

TEST(WddmMemoryManagerAllocator32BitTest, allocator32BitIsCreatedWithCorrectBase) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<WddmMock> wddm(static_cast<WddmMock *>(Wddm::createWddm()));
    uint64_t base = 0x56000;
    uint64_t size = 0x9000;
    wddm->setHeap32(base, size);

    std::unique_ptr<WddmMemoryManager> memoryManager = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(false, false, wddm.get(), executionEnvironment));

    ASSERT_NE(nullptr, memoryManager->allocator32Bit.get());

    EXPECT_EQ(base, memoryManager->allocator32Bit->getBase());
}

TEST(WddmMemoryManagerWithDeferredDeleterTest, givenWMMWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    ExecutionEnvironment executionEnvironment;
    auto wddm = std::make_unique<WddmMock>();
    bool actualDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockWddmMemoryManager memoryManager(wddm.get(), executionEnvironment);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    auto size = 4096u;

    auto allocation = memoryManager->allocateGraphicsMemory(size);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->gmm->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(allocation);

    allocation = memoryManager->allocateGraphicsMemory(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->gmm->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->gmm->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    for (size_t i = 0; i < allocation->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->useSystemMemoryPool);
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->gmm->useSystemMemoryPool);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMIsCalledThen4KBGraphicsAllocationIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryForSVM(size, false);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    EXPECT_TRUE(svmAllocation->gmm->useSystemMemoryPool);
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMIsCalledThenMemoryPoolIsSystem64KBPages) {
    memoryManager.reset(new MockWddmMemoryManager(true, false, wddm, executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryForSVM(size, false);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(WddmMemoryManagerSimpleTest, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    auto osHandle = 1u;
    gdi->getQueryResourceInfoArgOut().NumAllocations = 1;
    std::unique_ptr<Gmm> gmm(new Gmm(nullptr, 0, false));

    D3DDDI_OPENALLOCATIONINFO allocationInfo;
    allocationInfo.pPrivateDriverData = gmm->gmmResourceInfo->peekHandle();
    allocationInfo.hAllocation = static_cast<D3DKMT_HANDLE>(0x4000);
    allocationInfo.PrivateDriverDataSize = sizeof(GMM_RESOURCE_INFO);

    gdi->getOpenResourceArgOut().pOpenAllocationInfo = &allocationInfo;

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerSimpleTest,
       givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhenNotAlignedPtrIsPassedThenAlignedGraphicsAllocationIsCreated) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm, executionEnvironment));
    void *hostPtr = reinterpret_cast<void *>(0x5001);

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(13, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(reinterpret_cast<void *>(0x5001), allocation->getUnderlyingBuffer());
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->allocationOffset);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest,
       givenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWhencreateWddmAllocationFailsThenGraphicsAllocationIsNotCreated) {
    char hostPtr[64];
    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(64, hostPtr);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, AllocateAndFree) {
    auto *ptr = memoryManager->allocateGraphicsMemory(0x1000);
    EXPECT_NE(nullptr, ptr);
    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForVirtualPaddingSupportThenFalseIsReturned) {
    EXPECT_FALSE(memoryManager->peekVirtualPaddingSupport());
}

TEST_F(WddmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    WddmAllocation gfxAllocation(cpuPtr, size, nullptr, MemoryPool::MemoryNull);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->hostPtrManager.getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_EQ(fragment->osInternalStorage->gmm, gfxAllocation.gmm);
    EXPECT_NE(fragment->osInternalStorage->gpuPtr, 0ULL);
    EXPECT_EQ(fragment->osInternalStorage->handle, gfxAllocation.handle);
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->hostPtrManager.storeFragment(fragmentStorage);
    fragment = memoryManager->hostPtrManager.getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->hostPtrManager.getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->hostPtrManager.getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->hostPtrManager.getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtr) {
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(0x1000, ptr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, givenDefaultMemoryManagerWhenAllocateWithSizeIsCalledThenResourceHandleIsZero) {
    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(0x1000);

    auto wddmAllocation = static_cast<WddmAllocation *>(gpuAllocation);

    EXPECT_EQ(0u, wddmAllocation->resourceHandle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->handle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1));
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->handle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    auto alloc = memoryManager->allocateGraphicsMemory(1);

    auto ptr = memoryManager->lockResource(alloc);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(1u, wddm->lockResult.called);
    EXPECT_TRUE(wddm->lockResult.success);

    memoryManager->unlockResource(alloc);
    EXPECT_EQ(1u, wddm->unlockResult.called);
    EXPECT_TRUE(wddm->unlockResult.success);

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleReturns32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsTrue) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, true);
    ASSERT_NE(nullptr, gpuAllocation);
    if (is64bit) {
        EXPECT_TRUE(gpuAllocation->is32BitAllocation);

        uint64_t base = memoryManager->allocator32Bit->getBase();
        EXPECT_EQ(GmmHelper::canonize(base), gpuAllocation->gpuBaseAddress);
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleDoesNotReturn32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsFalse) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    memoryManager->setForce32BitAllocations(true);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_FALSE(gpuAllocation->is32BitAllocation);
    if (is64bit) {
        uint64_t base = 0;
        EXPECT_EQ(base, gpuAllocation->gpuBaseAddress);
    }

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto gpuAllocation = (WddmAllocation *)memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_NE(nullptr, gpuAllocation);
    auto expectedDestroyHandle = gpuAllocation->resourceHandle;
    EXPECT_NE(0u, expectedDestroyHandle);

    auto lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(0u, lastDestroyed);

    memoryManager->freeGraphicsMemory(gpuAllocation);
    lastDestroyed = getMockLastDestroyedResHandleFcn();
    EXPECT_EQ(lastDestroyed, expectedDestroyHandle);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, size, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, size, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    wddm->failOpenSharedHandle = true;

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_EQ(nullptr, gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.setMemoryManager(memoryManager.get());

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->gmm->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.setMemoryManager(memoryManager.get());

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<int>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->gmm->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    executionEnvironment.incRefInternal(); // to prevent destrorying execution environment by destructor of device
    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, &executionEnvironment, 0u));
    MockContext context(device.get());
    context.setMemoryManager(memoryManager.get());

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->gmm->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    MockContext context;
    context.setMemoryManager(memoryManager.get());

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    char data[64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_BUFFER, imageGraphicsAllocation->gmm->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.setMemoryManager(memoryManager.get());

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<int>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->gmm->resourceParams.Usage);
}

TEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtrOffseted) {
    MockWddmAllocation alloc, allocOffseted;
    bool success = false;
    // three pages
    void *ptr = alignedMalloc(4 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t baseOffset = 1024;
    // misalligned buffer spanning accross 3 pages
    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(2 * 4096, (char *)ptr + baseOffset);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ((char *)ptr + baseOffset, gpuAllocation->getUnderlyingBuffer());

    auto &hostPtrManager = memoryManager->hostPtrManager;

    auto fragment = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage, nullptr);

    // offseted by 3 pages, not in boundary
    auto fragment2 = hostPtrManager.getFragment((char *)ptr + 3 * 4096);

    EXPECT_EQ(nullptr, fragment2);

    // offseted by one page, still in boundary
    void *offsetedPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 4096);
    auto *gpuAllocation2 = memoryManager->allocateGraphicsMemory(0x1000, offsetedPtr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(offsetedPtr, gpuAllocation2->getUnderlyingBuffer());

    auto fragment3 = hostPtrManager.getFragment(offsetedPtr);
    ASSERT_NE(nullptr, fragment3);

    EXPECT_TRUE(fragment3->refCount == 2);
    EXPECT_EQ(alloc.handle, allocOffseted.handle);
    EXPECT_EQ(alloc.getUnderlyingBufferSize(), allocOffseted.getUnderlyingBufferSize());
    EXPECT_EQ(alloc.getAlignedCpuPtr(), allocOffseted.getAlignedCpuPtr());

    memoryManager->freeGraphicsMemory(gpuAllocation2);

    auto fragment4 = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment4);

    EXPECT_TRUE(fragment4->refCount == 1);

    memoryManager->freeGraphicsMemory(gpuAllocation);

    fragment4 = hostPtrManager.getFragment(ptr);
    EXPECT_EQ(nullptr, fragment4);

    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, AllocateGpuMemCheckGmm) {
    MockWddmAllocation allocation;
    bool success = false;
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(3 * 4096, ptr);
    // Should be same cpu ptr and gpu ptr
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    auto &hostPtrManager = memoryManager->hostPtrManager;

    auto fragment = hostPtrManager.getFragment(ptr);
    ASSERT_NE(nullptr, fragment);
    EXPECT_TRUE(fragment->refCount == 1);
    EXPECT_NE(fragment->osInternalStorage->handle, 0);
    EXPECT_NE(fragment->osInternalStorage->gmm, nullptr);
    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

TEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {

    MockWddmAllocation allocation;
    bool success = false;
    uint32_t size = 4096;
    void *ptr = reinterpret_cast<void *>(4096);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(gpuAllocation->gmm->resourceParams.pExistingSysMem));
    EXPECT_EQ(size, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    MockWddmAllocation allocation;
    bool success = false;
    uint32_t size = 0x1001;
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(gpuAllocation->gmm->resourceParams.pExistingSysMem));
    EXPECT_EQ(0x2000, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, getSystemSharedMemory) {
    int64_t mem = memoryManager->getSystemSharedMemory();
    EXPECT_EQ(mem, 4249540608);
}

TEST_F(WddmMemoryManagerTest, getMaxApplicationAddress) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

TEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithNullptr) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_LE(GmmHelper::canonize(wddm->getHeap32Base()), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(wddm->getHeap32Base()) + wddm->getHeap32Size() - 1, gpuAllocation->getGpuAddress());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithMisalignedHostPtrDoesNotDoTripleAlloc) {
    size_t misalignedSize = 0x2500;
    void *misalignedPtr = reinterpret_cast<void *>(0x12500);

    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(misalignedSize, misalignedPtr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, gpuAllocation);

    EXPECT_EQ(alignSizeWholePage(misalignedPtr, misalignedSize), gpuAllocation->getUnderlyingBufferSize());

    EXPECT_LE(GmmHelper::canonize(wddm->getHeap32Base()), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(wddm->getHeap32Base()) + wddm->getHeap32Size() - 1, gpuAllocation->getGpuAddress());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);

    void *alignedPtr = alignDown(misalignedPtr, MemoryConstants::allocationAlignment);
    uint64_t offset = ptrDiff(misalignedPtr, alignedPtr);

    EXPECT_EQ(offset, gpuAllocation->allocationOffset);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, Allocate32BitMemorySetsCannonizedGpuBaseAddress) {
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, gpuAllocation);

    uint64_t cannonizedAddress = GmmHelper::canonize(wddm->getHeap32Base());
    EXPECT_EQ(cannonizedAddress, gpuAllocation->gpuBaseAddress);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

TEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    OsHandleStorage storage;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    storage.fragmentStorageData[0].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[0].residency = new ResidencyData;

    storage.fragmentStorageData[0].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    storage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(pSysMem, 4096u, false);

    storage.fragmentStorageData[1].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[1].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData;

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[2].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    storage.fragmentStorageData[2].osHandleStorage->gmm = new Gmm(pSysMem, 4096u, false);
    storage.fragmentStorageData[2].residency = new ResidencyData;

    memoryManager->cleanOsHandles(storage);

    auto destroyWithResourceHandleCalled = 0u;
    D3DKMT_DESTROYALLOCATION2 *ptrToDestroyAlloc2 = nullptr;

    getSizesFcn(destroyWithResourceHandleCalled, ptrToDestroyAlloc2);

    EXPECT_EQ(0u, ptrToDestroyAlloc2->Flags.SynchronousDestroy);
    EXPECT_EQ(1u, ptrToDestroyAlloc2->Flags.AssumeNotInUse);

    EXPECT_EQ(ALLOCATION_HANDLE, storage.fragmentStorageData[1].osHandleStorage->handle);

    delete storage.fragmentStorageData[1].osHandleStorage;
    delete storage.fragmentStorageData[1].residency;
}

TEST_F(WddmMemoryManagerTest, freeNullAllocationNoCrash) {
    EXPECT_NO_THROW(memoryManager->freeGraphicsMemory(nullptr));
}

TEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenValueIsReturned) {
    AlignedMallocRestrictions *mallocRestrictions = memoryManager->getAlignedMallocRestrictions();
    ASSERT_NE(nullptr, mallocRestrictions);
    EXPECT_EQ(OCLRT::windowsMinAddress, mallocRestrictions->minAddress);
}

TEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCpuMemNotMeetRestrictionsThenReserveMemRangeForMap) {
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    WddmAllocation *allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(size, cpuPtr));

    void *expectReserve = reinterpret_cast<void *>(wddm->virtualAllocAddress);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectReserve, allocation->getReservedAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithDisabledDeferredDeleterWhenMapGpuVaFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(ptr, size, false));

    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(ptr, size, nullptr, MemoryPool::MemoryNull);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstMapGpuVaFailSecondAfterDrainSuccessThenCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(ptr, size, nullptr, MemoryPool::MemoryNull);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_TRUE(ret);
}

TEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstAndMapGpuVaFailSecondAfterDrainFailThenFailToCreateAllocation) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(new Gmm(ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 2);

    WddmAllocation allocation(ptr, size, nullptr, MemoryPool::MemoryNull);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_FALSE(ret);
}

TEST_F(WddmMemoryManagerTest, givenNullPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeap1) {
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(MemoryConstants::pageSize, nullptr, AllocationOrigin::INTERNAL_ALLOCATION));
    ASSERT_NE(nullptr, wddmAllocation);
    EXPECT_EQ(wddmAllocation->gpuBaseAddress, GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Base));
    EXPECT_NE(nullptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Base);
    auto cannonizedHeapEnd = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Limit);

    EXPECT_GE(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LE(wddmAllocation->getGpuAddress(), cannonizedHeapEnd);

    EXPECT_TRUE(wddmAllocation->cpuPtrAllocated);
    EXPECT_TRUE(wddmAllocation->is32BitAllocation);
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerTest, givenPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeap1) {
    auto ptr = reinterpret_cast<void *>(0x1000000);
    auto wddmAllocation = static_cast<WddmAllocation *>(memoryManager->allocate32BitGraphicsMemory(MemoryConstants::pageSize, ptr, AllocationOrigin::INTERNAL_ALLOCATION));
    ASSERT_NE(nullptr, wddmAllocation);
    EXPECT_EQ(wddmAllocation->gpuBaseAddress, GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Base));
    EXPECT_EQ(ptr, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(4096u, wddmAllocation->getUnderlyingBufferSize());
    EXPECT_NE((uint64_t)wddmAllocation->getUnderlyingBuffer(), wddmAllocation->getGpuAddress());
    auto cannonizedHeapBase = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Base);
    auto cannonizedHeapEnd = GmmHelper::canonize(this->wddm->getGfxPartition().Heap32[1].Limit);

    EXPECT_GE(wddmAllocation->getGpuAddress(), cannonizedHeapBase);
    EXPECT_LE(wddmAllocation->getGpuAddress(), cannonizedHeapEnd);

    EXPECT_FALSE(wddmAllocation->cpuPtrAllocated);
    EXPECT_TRUE(wddmAllocation->is32BitAllocation);
    memoryManager->freeGraphicsMemory(wddmAllocation);
}

TEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksAllocationsResident) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
    EXPECT_TRUE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
    EXPECT_TRUE(allocation4.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsUpdatesLastFence) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;
    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};

    osContext->get()->getMonitoredFence().currentFenceValue = 20;

    memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_EQ(20u, allocation1.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation2.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation3.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
    EXPECT_EQ(20u, allocation4.getResidencyData().getFenceValueForContextId(osContext->getContextId()));
}

TEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksTripleAllocationsResident) {
    MockWddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, ptr);
    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};

    memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    EXPECT_EQ(5u, gdi->getMakeResidentArg().NumAllocations);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsSetsLastFencePLusOneForTripleAllocations) {
    MockWddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, reinterpret_cast<void *>(0x1500));

    osContext->get()->getMonitoredFence().currentFenceValue = 20;

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->getFenceValueForContextId(0));
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmMemoryManagerResidencyTest, trimCallbackIsRegisteredInWddmMemoryManagerCtor) {
    EXPECT_EQ((PFND3DKMT_TRIMNOTIFICATIONCALLBACK)memoryManager->trimCallback, gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(memoryManager.get()), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDevice(), gdi->getRegisterTrimNotificationArg().hDevice);
}

TEST_F(WddmMemoryManagerResidencyTest, givenWddmMemoryManagerWhenCallingDestructorThenUnregisterTrimCallback) {
    auto trimCallbackHandle = wddm->trimCallbackHandle;
    auto trimCallbackAddress = reinterpret_cast<PFND3DKMT_TRIMNOTIFICATIONCALLBACK>(memoryManager->trimCallback);
    memoryManager.reset();

    auto &unregisterNotification = gdi->getUnregisterTrimNotificationArg();
    EXPECT_EQ(trimCallbackAddress, unregisterNotification.Callback);
    EXPECT_EQ(trimCallbackHandle, unregisterNotification.Handle);
}

TEST(WddmDebugModesTests, givenDebugModeWhenItIsActiveThenTrimCallbackIsNotRegistred) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DoNotRegisterTrimCallback.set(true);
    WddmMock wddm;
    wddm.init();
    EXPECT_EQ(nullptr, wddm.trimCallbackHandle);
}

TEST_F(WddmMemoryManagerResidencyTest, givenNotUsedAllocationsFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenAllocationsAreEvictedMarkedAndRemovedFromTrimCandidateList) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1, allocation2;

    allocation1.getResidencyData().updateCompletionData(0, osContext);
    allocation2.getResidencyData().updateCompletionData(0, osContext);

    allocation1.getResidencyData().resident = true;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    memoryManager->residencyControllers[0]->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    osContext->get()->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 allocations evicted
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(0u, memoryManager->residencyControllers[0]->peekTrimCandidateList().size());
    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerResidencyTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    MockWddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    // mark allocation used from last periodic trim
    allocation1.getResidencyData().updateCompletionData(0, osContext);
    allocation2.getResidencyData().updateCompletionData(11, osContext);
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    memoryManager->residencyControllers[0]->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    osContext->get()->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 1 allocation evicted
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());

    //marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    // second stays resident
    EXPECT_TRUE(allocation2.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerResidencyTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    // 3-fragment Allocation
    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, ptr);
    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().updateCompletionData(0, osContext);

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->updateCompletionData(0, osContext);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(11, osContext);
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;

    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->updateCompletionData(0, osContext);
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident = true;

    // Set last periodic fence value
    memoryManager->residencyControllers[0]->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    osContext->get()->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(allocationTriple);

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmMemoryManagerResidencyTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    platform()->peekExecutionEnvironment()->osInterface = std::move(osInterface);
    platform()->initialize();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    memoryManager->residencyControllers[0]->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    *osContext->get()->getMonitoredFence().cpuAddress = 20;

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, memoryManager->residencyControllers[0]->getLastTrimFenceValue());
}

TEST_F(WddmMemoryManagerResidencyTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    platform()->peekExecutionEnvironment()->osInterface = std::move(osInterface);
    platform()->initialize();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    memoryManager->residencyControllers[0]->setLastTrimFenceValue(10);
    // Set current fence value to greater value
    *osContext->get()->getMonitoredFence().cpuAddress = 20;

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, memoryManager->residencyControllers[0]->getLastTrimFenceValue());
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWithZeroSizeReturnsTrue) {
    bool status = memoryManager->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetAllDoneAllocations) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1, allocation2, allocation3;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    allocation2.getResidencyData().updateCompletionData(1, osContext);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContext);
    allocation3.getResidencyData().resident = true;

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;
    osContext->get()->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation3);

    memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_EQ(1u, memoryManager->residencyControllers[0]->peekTrimCandidatesCount());
    memoryManager->residencyControllers[0]->compactTrimCandidateList();
    EXPECT_EQ(1u, memoryManager->residencyControllers[0]->peekTrimCandidateList().size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetReturnsFalseWhenNumBytesToTrimIsNotZero) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(0u, memoryManager->residencyControllers[0]->peekTrimCandidateList().size());

    EXPECT_FALSE(status);
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetStopsEvictingWhenNumBytesToTrimIsZero) {
    WddmAllocation allocation1(reinterpret_cast<void *>(0x1000), 0x1000, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull),
        allocation2(reinterpret_cast<void *>(0x1000), 0x3000, reinterpret_cast<void *>(0x1000), 0x3000, nullptr, MemoryPool::MemoryNull),
        allocation3(reinterpret_cast<void *>(0x1000), 0x1000, reinterpret_cast<void *>(0x1000), 0x1000, nullptr, MemoryPool::MemoryNull);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    allocation2.getResidencyData().updateCompletionData(1, osContext);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContext);
    allocation3.getResidencyData().resident = true;

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;
    osContext->get()->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(status);
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(1u, memoryManager->residencyControllers[0]->peekTrimCandidateList().size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetMarksEvictedAllocationNonResident) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1, allocation2, allocation3;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    allocation2.getResidencyData().updateCompletionData(1, osContext);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(2, osContext);
    allocation3.getResidencyData().resident = true;

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;
    osContext->get()->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWaitsFromCpuWhenLastFenceIsGreaterThanMonitored) {
    gdi->setNonZeroNumBytesToTrimInEvict();

    MockWddmAllocation allocation1;

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(2, osContext);

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 2;
    osContext->get()->getMonitoredFence().currentFenceValue = 3;

    wddm->makeNonResidentResult.called = 0;
    wddm->waitFromCpuResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);

    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident);

    EXPECT_EQ(wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
}

TEST_F(WddmMemoryManagerResidencyTest, trimToBudgetEvictsDoneFragmentsOnly) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    WddmAllocation allocation1(ptr, 0x1000, ptr, 0x1000, nullptr, MemoryPool::MemoryNull);
    WddmAllocation allocation2(ptr, 0x1000, ptr, 0x1000, nullptr, MemoryPool::MemoryNull);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    allocation2.getResidencyData().updateCompletionData(1, osContext);
    allocation2.getResidencyData().resident = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptrTriple));

    allocationTriple->getResidencyData().updateCompletionData(1, osContext);
    allocationTriple->getResidencyData().resident = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->updateCompletionData(1, osContext);
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident = true;
    }

    // This should not be evicted
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->updateCompletionData(2, osContext);

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(allocationTriple);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;
    osContext->get()->getMonitoredFence().currentFenceValue = 2;

    wddm->makeNonResidentResult.called = 0;

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmMemoryManagerResidencyTest, givenThreeAllocationsAlignedSizeBiggerThanAllocSizeWhenBudgetEqualTwoAlignedAllocationThenEvictOnlyTwo) {
    gdi->setNonZeroNumBytesToTrimInEvict();
    size_t underlyingSize = 0xF00;
    size_t alignedSize = 0x1000;
    size_t budget = 2 * alignedSize;

    //trim budget should consider aligned size, not underlying, so if function considers underlying, it should evict three, not two
    EXPECT_GT((3 * underlyingSize), budget);
    EXPECT_LT((2 * underlyingSize), budget);
    void *ptr1 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    void *ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x3000);
    void *ptr3 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x5000);

    WddmAllocation allocation1(ptr1, underlyingSize, ptr1, alignedSize, nullptr, MemoryPool::MemoryNull);
    WddmAllocation allocation2(ptr2, underlyingSize, ptr2, alignedSize, nullptr, MemoryPool::MemoryNull);
    WddmAllocation allocation3(ptr3, underlyingSize, ptr3, alignedSize, nullptr, MemoryPool::MemoryNull);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().updateCompletionData(0, osContext);

    allocation2.getResidencyData().updateCompletionData(1, osContext);
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().updateCompletionData(1, osContext);
    allocation3.getResidencyData().resident = true;

    *osContext->get()->getMonitoredFence().cpuAddress = 1;
    osContext->get()->getMonitoredFence().lastSubmittedFence = 1;
    osContext->get()->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation1);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation2);
    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(budget);
    EXPECT_TRUE(status);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

TEST_F(BufferWithWddmMemory, ValidHostPtr) {
    flags = CL_MEM_USE_HOST_PTR;

    auto ptr = alignedMalloc(MemoryConstants::preferredAlignment, MemoryConstants::preferredAlignment);

    auto buffer = Buffer::create(
        &context,
        flags,
        MemoryConstants::preferredAlignment,
        ptr,
        retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    EXPECT_EQ(ptr, address);
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation());
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation()->getUnderlyingBuffer());

    delete buffer;
    alignedFree(ptr);
}

TEST_F(BufferWithWddmMemory, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    memoryManager->populateOsHandles(storage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage->gmm);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage);
}

TEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = memoryManager->hostPtrManager;

    ASSERT_EQ(3u, hostPtrManager.getFragmentCount());

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < max_fragments_count; i++) {

        uintptr_t GpuPtr = (uintptr_t)(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gpuPtr);
        uintptr_t CpuPtr = (uintptr_t)(reqs.AllocationFragments[i].allocationPtr);
        EXPECT_NE((D3DKMT_HANDLE) nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->handle);

        EXPECT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr,
                  reinterpret_cast<void *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.pExistingSysMem));
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize,
                  graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gmm->resourceParams.BaseWidth);
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(BufferWithWddmMemory, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;

    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    auto ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, size, ptr);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedThenGraphicsAddressIsPopulatedFromProperFragment) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAdress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData();
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(ptr, size, false);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    fragment.osInternalStorage->gpuPtr = gpuAdress;
    memoryManager->hostPtrManager.storeFragment(fragment);

    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, size, ptr);
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAdress, allocation->getGpuAddress());
    EXPECT_EQ(0ULL, allocation->allocationOffset);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedNotAllignedToPageThenGraphicsAddressIsPopulatedFromProperFragmentAndOffsetisAssigned) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAdress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData();
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[0].osHandleStorage->gmm = new Gmm(ptr, size, false);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    fragment.osInternalStorage->gpuPtr = gpuAdress;
    memoryManager->hostPtrManager.storeFragment(fragment);

    auto offset = 80;
    auto allocationPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + offset);
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, size, allocationPtr);

    EXPECT_EQ(allocationPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAdress + offset, allocation->getGpuAddress()); // getGpuAddress returns gpuAddress + allocationOffset
    EXPECT_EQ(offset, allocation->allocationOffset);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkAllocationsResidentWhenMakeResidentFails) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkTripleAllocationsResidentWhenMakeResidentFails) {
    MockWddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptr));
    ASSERT_NE(nullptr, allocationTriple);

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, allocationTriple, &allocation2};
    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

TEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsFailsWhenMakeResidentFailsAndCantTrimFurther) {
    MockWddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    ResidencyContainer residencyPack{&allocation1, &allocation2, &allocation3, &allocation4};
    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsCallsMakeResidentWithCantTrimFurtherSetToTrueWhenTrimToBudgetReturnsFalse) {
    MockWddmAllocation allocation1;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, false, ::testing::_)).Times(1);
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, true, ::testing::_)).Times(1);

    ResidencyContainer residencyPack{&allocation1};
    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_FALSE(result);
}

TEST_F(WddmMemoryManagerTest2, givenAllocationPackPassedWhenCallingMakeResidentResidencyAllocationsThenItIsUsed) {
    MockWddmAllocation allocation1;
    MockWddmAllocation allocation2;
    allocation1.handle = 1;
    allocation2.handle = 2;
    ResidencyContainer residencyPack{&allocation1, &allocation2};

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool {
        EXPECT_EQ(1, handles[0]);
        EXPECT_EQ(2, handles[1]);
        return true;
    };
    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, 2, false, ::testing::_)).Times(1);

    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);
    EXPECT_TRUE(result);
}

TEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsSucceedsWhenMakeResidentFailsAndTrimToBudgetSucceeds) {
    MockWddmAllocation allocation1;
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    WddmAllocation allocationToTrim(cpuPtr, allocationSize, cpuPtr, allocationSize, nullptr, MemoryPool::MemoryNull);

    allocationToTrim.getResidencyData().updateCompletionData(osContext->get()->getMonitoredFence().lastSubmittedFence, osContext);

    auto makeResidentWithOutBytesToTrim = [allocationSize](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = allocationSize;  return false; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentWithOutBytesToTrim)).WillOnce(::testing::Return(true));

    memoryManager->residencyControllers[0]->addToTrimCandidateList(&allocationToTrim);

    ResidencyContainer residencyPack{&allocation1};
    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
}

TEST_F(WddmMemoryManagerTest2, givenMemoryManagerWhenMakeResidentFailsThenMemoryBudgetExhaustedIsReturnedAsTrue) {
    MockWddmAllocation allocation1;
    ResidencyContainer residencyPack{&allocation1};

    auto makeResidentThatFails = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return false; };
    auto makeResidentThatSucceds = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return true; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentThatFails)).WillOnce(::testing::Invoke(makeResidentThatSucceds));

    bool result = memoryManager->makeResidentResidencyAllocations(residencyPack, *osContext);
    EXPECT_TRUE(memoryManager->isMemoryBudgetExhausted());
}

struct WddmMemoryManagerWithAsyncDeleterTest : ::testing::Test {
    void SetUp() {
        wddm = std::make_unique<WddmMock>();
        wddm->gdi.reset(new MockGdi());
        wddm->callBaseDestroyAllocations = false;
        deleter = new MockDeferredDeleter;
        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm.get(), executionEnvironment);
        memoryManager->setDeferredDeleter(deleter);
    }
    MockDeferredDeleter *deleter = nullptr;
    std::unique_ptr<WddmMock> wddm;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    ExecutionEnvironment executionEnvironment;
};

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsEnabledThenCanDeferDeletions) {
    EXPECT_EQ(0, deleter->deferDeletionCalled);
    memoryManager->tryDeferDeletions(nullptr, 0, 0);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
    EXPECT_EQ(1u, wddm.get()->destroyAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsDisabledThenCannotDeferDeletions) {
    memoryManager->setDeferredDeleter(nullptr);
    memoryManager->tryDeferDeletions(nullptr, 0, 0);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenDrainIsCalledAndCreateAllocationIsCalledTwice) {
    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    memoryManager->allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_SUCCESS;
    wddm->mapGpuVaStatus = true;
    wddm->callBaseMapGpuVa = false;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, wddm->mapGpuVirtualAddressResult.called);
    auto allocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    memoryManager->setDeferredDeleter(nullptr);
    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    memoryManager->allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerDefaults, givenDefaultWddmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenHeap1BaseIsReturned) {
    ExecutionEnvironment executionEnvironment;
    auto wddm = std::make_unique<WddmMock>();
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm.get(), executionEnvironment);
    auto heapBase = wddm->getGfxPartition().Heap32[1].Base;
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress());
}

TEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    ExecutionEnvironment executionEnvironment;
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    MockWddmMemoryManager memoryManager(wddm.get(), executionEnvironment);

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, reinterpret_cast<void *>(0x1000));

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenCreatingGraphicsMemoryForBufferWithoutHostPtrThen64kbAdressIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager64k(true, false, wddm.get(), executionEnvironment);
    EXPECT_EQ(0, wddm->createAllocationResult.called);

    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemoryInPreferredPool(AllocationFlags(true), 0, nullptr, static_cast<size_t>(MemoryConstants::pageSize64k), GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_EQ(1, wddm->createAllocationResult.called);
    EXPECT_NE(nullptr, galloc);
    EXPECT_EQ(true, galloc->isLocked());
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % MemoryConstants::pageSize64k);
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % MemoryConstants::pageSize64k);
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(OsAgnosticMemoryManagerUsingWddmTest, givenEnabled64kbPagesWhenAllocationIsCreatedWithSizeSmallerThan64kbThenGraphicsAllocationsHas64kbAlignedUnderlyingSize) {
    DebugManagerStateRestore dbgRestore;
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager(true, false, wddm.get(), executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemory64kb(1, MemoryConstants::pageSize64k, false, false);

    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(MemoryConstants::pageSize64k, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_NE(0llu, graphicsAllocation->getGpuAddress());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(1u, graphicsAllocation->gmm->resourceParams.Flags.Info.Cacheable);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MockWddmMemoryManagerTest, givenWddmWhenallocateGraphicsMemory64kbThenLockResultAndmapGpuVirtualAddressIsCalled) {
    ExecutionEnvironment executionEnvironment;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    MockWddmMemoryManager memoryManager64k(wddm.get(), executionEnvironment);
    uint32_t lockCount = wddm->lockResult.called;
    uint32_t mapGpuVirtualAddressResult = wddm->mapGpuVirtualAddressResult.called;
    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemory64kb(65536, 65536, true, false);
    EXPECT_EQ(lockCount + 1, wddm->lockResult.called);
    EXPECT_EQ(mapGpuVirtualAddressResult + 1, wddm->mapGpuVirtualAddressResult.called);
    EXPECT_NE(wddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    auto wddm = std::make_unique<WddmMock>();
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenItIsCreatedThenMemoryBudgetIsNotExhausted) {
    auto wddm = std::make_unique<WddmMock>();
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST_F(MockWddmMemoryManagerTest, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsTrueAndDeleterIsNotNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    auto wddm = std::make_unique<WddmMock>();
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    auto wddm = std::make_unique<WddmMock>();
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenPageTableManagerWhenMapAuxGpuVaCalledThenUseWddmToMap) {
    auto myWddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(myWddm->init());
    WddmMemoryManager memoryManager(false, false, myWddm.get(), executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    myWddm->resetPageTableManager(mockMngr);

    auto allocation = memoryManager.allocateGraphicsMemory(4096);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation->getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation->gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = memoryManager.mapAuxGpuVA(allocation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenMappedGpuVaThenMapAuxVa) {
    std::unique_ptr<Gmm> gmm(new Gmm(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = GmmHelper::canonize(wddm.getGfxPartition().Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, gpuVa, false, false, false);
    ASSERT_TRUE(result);

    auto productFamily = wddm.getGfxPlatform()->eProductFamily;
    UNRECOVERABLE_IF(!hardwareInfoTable[productFamily]);
    auto gpuAddressRange = hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace;
    if (gpuAddressRange == MemoryConstants::max48BitAddress) {
        EXPECT_EQ(GmmHelper::canonize(wddm.getGfxPartition().Standard.Base), gpuVa);
    } else {
        EXPECT_GE(gpuAddressRange, gpuVa);
        EXPECT_LE(0u, gpuVa);
        expectedDdiUpdateAuxTable.BaseGpuVA = MemoryConstants::pageSize64k;
    }
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenReleaseingThenUnmapAuxVa) {
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u));
    wddmAlloc->gpuPtr = gpuVa;
    wddmAlloc->gmm->isRenderCompressed = true;

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = gpuVa;
    expectedDdiUpdateAuxTable.BaseResInfo = wddmAlloc->gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));
    memoryManager.freeGraphicsMemory(wddmAlloc);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

TEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u));
    wddmAlloc->gmm->isRenderCompressed = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    std::unique_ptr<Gmm> gmm(new Gmm(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, gpuVa, false, false, false);
    ASSERT_TRUE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    std::unique_ptr<Gmm> gmm(new Gmm(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init());

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), 0, nullptr, gpuVa, false, false, false);
    ASSERT_FALSE(result);
}

TEST_F(MockWddmMemoryManagerTest, givenRenderCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    WddmMemoryManager memoryManager(false, false, wddm.get(), executionEnvironment);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto myGmm = new Gmm(reinterpret_cast<void *>(123), 4096u, false);
    myGmm->isRenderCompressed = false;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u));
    delete wddmAlloc->gmm;
    wddmAlloc->gmm = myGmm;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm->mapGpuVirtualAddressImpl(myGmm, ALLOCATION_HANDLE, nullptr, gpuVa, false, false, false);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryWhenCreateAllocationFailsThenPopulateOsHandlesReturnsInvalidPointer) {
    OsHandleStorage handleStorage;
    handleStorage.fragmentCount = 1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;
    handleStorage.fragmentStorageData[0].freeTheFragment = false;

    EXPECT_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillOnce(::testing::Return(STATUS_GRAPHICS_NO_VIDEO_MEMORY));

    auto result = memoryManager->populateOsHandles(handleStorage);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage);
}

TEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryPassedToPopulateOsHandlesWhenCreateAllocationFailsThenAllocatedFragmentsAreNotStored) {
    OsHandleStorage handleStorage;
    OsHandle handle;
    handleStorage.fragmentCount = 2;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 0x1000;

    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 0x6000;

    EXPECT_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillOnce(::testing::Return(STATUS_GRAPHICS_NO_VIDEO_MEMORY));

    auto result = memoryManager->populateOsHandles(handleStorage);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    auto numberOfStoredFragments = memoryManager->hostPtrManager.getFragmentCount();
    EXPECT_EQ(0u, numberOfStoredFragments);
    EXPECT_EQ(nullptr, memoryManager->hostPtrManager.getFragment(handleStorage.fragmentStorageData[1].cpuPtr));

    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage);
}

TEST(WddmMemoryManagerCleanupTest, givenUsedTagAllocationInWddmMemoryManagerWhenCleanupMemoryManagerThenDontAccessCsr) {
    auto wddm = std::make_unique<WddmMock>();
    EXPECT_TRUE(wddm->init());
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(createCommandStream(*platformDevices, executionEnvironment)));
    executionEnvironment.memoryManager = std::make_unique<WddmMemoryManager>(false, false, wddm.get(), executionEnvironment);
    EXPECT_EQ(executionEnvironment.commandStreamReceivers[0].get(), executionEnvironment.memoryManager->getCommandStreamReceiver(0));
    auto tagAllocator = executionEnvironment.memoryManager->getEventPerfCountAllocator();
    auto allocation = tagAllocator->getTag()->getGraphicsAllocation();
    allocation->taskCount = 1;
    executionEnvironment.commandStreamReceivers.clear();
    EXPECT_THROW(executionEnvironment.memoryManager->getCommandStreamReceiver(0), std::exception);
    EXPECT_NO_THROW(executionEnvironment.memoryManager.reset());
}
