/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_library.h"

#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/os_interface/windows/wddm_memory_manager_tests.h"

using namespace OCLRT;
using namespace ::testing;

void WddmMemoryManagerFixture::SetUp() {
    GdiDllFixture::SetUp();
    wddm = static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20));
    ASSERT_NE(nullptr, wddm);
    if (platformDevices[0]->capabilityTable.ftrCompression) {
        GMM_DEVICE_CALLBACKS_INT dummyDeviceCallbacks = {};
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};
        wddm->resetPageTableManager(GmmPageTableMngr::create(&dummyDeviceCallbacks, 0, &dummyTTCallbacks));
    }
}

TEST(WddmMemoryManagerAllocator32BitTest, allocator32BitIsCreatedWithCorrectBase) {
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm(WddmInterfaceVersion::Wddm20));
    uint64_t base = 0x56000;
    uint64_t size = 0x9000;
    wddm->setHeap32(base, size);

    std::unique_ptr<WddmMemoryManager> memoryManager = std::unique_ptr<WddmMemoryManager>(new WddmMemoryManager(false, wddm));

    ASSERT_NE(nullptr, memoryManager->allocator32Bit.get());

    EXPECT_EQ(base, memoryManager->allocator32Bit->getBase());
}

TEST(WddmMemoryManagerWithDeferredDeleterTest, givenWMMWhenAsyncDeleterIsEnabledAndWaitForDeletionsIsCalledThenDeleterInWddmIsSetToNullptr) {
    WddmMock *wddm = new WddmMock();
    bool actualDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockWddmMemoryManager memoryManager(wddm);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(actualDeleterFlag);
}

HWTEST_F(WddmMemoryManagerTest, AllocateAndFree) {
    SetUpMm<FamilyType>();
    auto *ptr = memoryManager->allocateGraphicsMemory(0x1000);
    EXPECT_NE(nullptr, ptr);
    memoryManager->freeGraphicsMemory(ptr);
}

HWTEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForVirtualPaddingSupportThenFalseIsReturned) {
    SetUpMm<FamilyType>();
    EXPECT_FALSE(memoryManager->peekVirtualPaddingSupport());
}

TEST_F(WddmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    memoryManager.reset(new (std::nothrow) MockWddmMemoryManager(wddm));
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    WddmAllocation gfxAllocation(cpuPtr, size, nullptr);
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

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtr) {
    SetUpMm<FamilyType>();
    // three pages
    void *ptr = alignedMalloc(3 * 4096, 4096);
    ASSERT_NE(nullptr, ptr);

    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(0x1000, ptr);
    // Should be same cpu ptr and gpu ptr
    EXPECT_EQ(ptr, gpuAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(gpuAllocation);
    alignedFree(ptr);
}

HWTEST_F(WddmMemoryManagerTest, givenDefaultMemoryManagerWhenAllocateWithSizeIsCalledThenResourceHandleIsZero) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = memoryManager->allocateGraphicsMemory(0x1000, MemoryConstants::pageSize);

    auto wddmAllocation = static_cast<WddmAllocation *>(gpuAllocation);

    EXPECT_EQ(0u, wddmAllocation->resourceHandle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmAlloc->handle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromNTHandleIsCalledThenNonNullGraphicsAllocationIsReturned) {
    SetUpMm<FamilyType>();
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, 4096u, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1));
    auto wddmAlloc = static_cast<WddmAllocation *>(gpuAllocation);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(NT_RESOURCE_HANDLE, wddmAlloc->resourceHandle);
    EXPECT_EQ(NT_ALLOCATION_HANDLE, wddmAlloc->handle);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleReturns32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsTrue) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, 4096u, false));
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

HWTEST_F(WddmMemoryManagerTest, createAllocationFromSharedHandleDoesNotReturn32BitAllocWhenForce32bitAddressingIsSetAndRequireSpecificBitnessIsFalse) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, 4096u, false));
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenFreeAllocFromSharedHandleIsCalledThenDestroyResourceHandle) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, 4096u, false));
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerSizeZeroWhenCreateFromSharedHandleIsCalledThenUpdateSize) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, size, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_EQ(size, gpuAllocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCreateFromSharedHandleFailsThenReturnNull) {
    SetUpMm<FamilyType>();
    auto osHandle = 1u;
    auto size = 4096u;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<Gmm> gmm(GmmHelper::create(pSysMem, size, false));
    auto status = setSizesFcn(gmm->gmmResourceInfo.get(), 1u, 1024u, 1u);

    wddm->failOpenSharedHandle = true;

    auto *gpuAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_EQ(nullptr, gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
    MockContext context;
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryForImageIsUsed) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemHostPtrOffseted) {
    SetUpMm<FamilyType>();
    WddmAllocation alloc, allocOffseted;
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

HWTEST_F(WddmMemoryManagerTest, AllocateGpuMemCheckGmm) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
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

HWTEST_F(WddmMemoryManagerTest, GivenAlignedPointerWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
    bool success = false;
    uint32_t size = 4096;
    void *ptr = reinterpret_cast<void *>(4096);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(gpuAllocation->gmm->resourceParams.pExistingSysMem));
    EXPECT_EQ(size, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, GivenUnAlignedPointerAndSizeWhenAllocate32BitMemoryThenGmmCalledWithCorrectPointerAndSize) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;
    bool success = false;
    uint32_t size = 0x1001;
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(gpuAllocation->gmm->resourceParams.pExistingSysMem));
    EXPECT_EQ(0x2000, gpuAllocation->gmm->resourceParams.ExistingSysMemSize);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, getSystemSharedMemory) {
    SetUpMm<FamilyType>();
    int64_t mem = memoryManager->getSystemSharedMemory();
    EXPECT_EQ(mem, 4249540608);
}

HWTEST_F(WddmMemoryManagerTest, getMaxApplicationAddress) {
    SetUpMm<FamilyType>();
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if (is32bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    }
}

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithNullptr) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_LE(GmmHelper::canonize(wddm->getHeap32Base()), gpuAllocation->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(wddm->getHeap32Base()) + wddm->getHeap32Size() - 1, gpuAllocation->getGpuAddress());

    EXPECT_EQ(0u, gpuAllocation->fragmentsStorage.fragmentCount);
    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemoryWithMisalignedHostPtrDoesNotDoTripleAlloc) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, Allocate32BitMemorySetsCannonizedGpuBaseAddress) {
    SetUpMm<FamilyType>();
    auto *gpuAllocation = memoryManager->allocate32BitGraphicsMemory(3 * MemoryConstants::pageSize, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, gpuAllocation);

    uint64_t cannonizedAddress = GmmHelper::canonize(wddm->getHeap32Base());
    EXPECT_EQ(cannonizedAddress, gpuAllocation->gpuBaseAddress);

    memoryManager->freeGraphicsMemory(gpuAllocation);
}

HWTEST_F(WddmMemoryManagerTest, GivenThreeOsHandlesWhenAskedForDestroyAllocationsThenAllMarkedAllocationsAreDestroyed) {
    SetUpMm<FamilyType>();
    OsHandleStorage storage;
    void *pSysMem = reinterpret_cast<void *>(0x1000);

    storage.fragmentStorageData[0].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[0].residency = new ResidencyData;

    storage.fragmentStorageData[0].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[0].freeTheFragment = true;
    storage.fragmentStorageData[0].osHandleStorage->gmm = GmmHelper::create(pSysMem, 4096u, false);

    storage.fragmentStorageData[1].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[1].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[1].residency = new ResidencyData;

    storage.fragmentStorageData[1].freeTheFragment = false;

    storage.fragmentStorageData[2].osHandleStorage = new OsHandle;
    storage.fragmentStorageData[2].osHandleStorage->handle = ALLOCATION_HANDLE;
    storage.fragmentStorageData[2].freeTheFragment = true;
    storage.fragmentStorageData[2].osHandleStorage->gmm = GmmHelper::create(pSysMem, 4096u, false);
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

HWTEST_F(WddmMemoryManagerTest, freeNullAllocationNoCrash) {
    SetUpMm<FamilyType>();
    memoryManager->freeGraphicsMemory(nullptr);
}

HWTEST_F(WddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenValueIsReturned) {
    SetUpMm<FamilyType>();
    AlignedMallocRestrictions *mallocRestrictions = memoryManager->getAlignedMallocRestrictions();
    ASSERT_NE(nullptr, mallocRestrictions);
    EXPECT_EQ(OCLRT::windowsMinAddress, mallocRestrictions->minAddress);
}

HWTEST_F(WddmMemoryManagerTest, givenWddmMemoryManagerWhenCpuMemNotMeetRestrictionsThenReserveMemRangeForMap) {
    SetUpMm<FamilyType>();
    void *cpuPtr = reinterpret_cast<void *>(memoryManager->getAlignedMallocRestrictions()->minAddress - 0x1000);
    size_t size = 0x1000;

    WddmAllocation *allocation = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(size, cpuPtr));

    void *expectReserve = reinterpret_cast<void *>(wddm->virtualAllocAddress);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectReserve, allocation->getReservedAddress());
    EXPECT_EQ(expectReserve, reinterpret_cast<void *>(allocation->gpuPtr));
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(WddmMemoryManagerTest, givenManagerWithDisabledDeferredDeleterWhenMapGpuVaFailThenFailToCreateAllocation) {
    SetUpMm<FamilyType>();
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(GmmHelper::create(ptr, size, false));

    memoryManager->setDeferredDeleter(nullptr);
    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(ptr, size, nullptr);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_FALSE(ret);
}

HWTEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstMapGpuVaFailSecondAfterDrainSuccessThenCreateAllocation) {
    SetUpMm<FamilyType>();
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(GmmHelper::create(ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 1);

    WddmAllocation allocation(ptr, size, nullptr);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_TRUE(ret);
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), allocation.getGpuAddress());
}

HWTEST_F(WddmMemoryManagerTest, givenManagerWithEnabledDeferredDeleterWhenFirstAndMapGpuVaFailSecondAfterDrainFailThenFailToCreateAllocation) {
    SetUpMm<FamilyType>();
    void *ptr = reinterpret_cast<void *>(0x1000);
    size_t size = 0x1000;
    std::unique_ptr<Gmm> gmm(GmmHelper::create(ptr, size, false));

    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    memoryManager->setDeferredDeleter(deleter);

    setMapGpuVaFailConfigFcn(0, 2);

    WddmAllocation allocation(ptr, size, nullptr);
    allocation.gmm = gmm.get();
    bool ret = memoryManager->createWddmAllocation(&allocation, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_FALSE(ret);
}

HWTEST_F(WddmMemoryManagerTest, givenNullPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeap1) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest, givenPtrAndSizePassedToCreateInternalAllocationWhenCallIsMadeThenAllocationIsCreatedIn32BitHeap1) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerResidencyTest, addToTrimCandidateListPlacesAllocationInContainerAndAssignsPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    memoryManager->addToTrimCandidateList(&allocation);

    EXPECT_NE(0u, memoryManager->trimCandidateList.size());
    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, memoryManager->trimCandidateList.size());

    EXPECT_EQ(&allocation, memoryManager->trimCandidateList[position]);
}

HWTEST_F(WddmMemoryManagerResidencyTest, addToTrimCandidateListDoesNotInsertAllocationAlreadyOnTheList) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation);

    EXPECT_NE(trimListUnusedPosition, allocation.getTrimCandidateListPosition());

    size_t position = allocation.getTrimCandidateListPosition();
    ASSERT_LT(position, memoryManager->trimCandidateList.size());

    EXPECT_EQ(&allocation, memoryManager->trimCandidateList[position]);

    size_t previousSize = memoryManager->trimCandidateList.size();
    memoryManager->addToTrimCandidateList(&allocation);

    EXPECT_EQ(previousSize, memoryManager->trimCandidateList.size());
    EXPECT_EQ(position, allocation.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListAssignsUnusedPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    memoryManager->addToTrimCandidateList(&allocation);
    memoryManager->removeFromTrimCandidateList(&allocation);

    EXPECT_EQ(trimListUnusedPosition, allocation.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesAllocationInAssignedPosition) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    memoryManager->addToTrimCandidateList(&allocation);
    size_t position = allocation.getTrimCandidateListPosition();

    memoryManager->removeFromTrimCandidateList(&allocation);

    if (memoryManager->trimCandidateList.size() > position) {
        EXPECT_NE(&allocation, memoryManager->trimCandidateList[position]);
    }
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesLastAllocation) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation);

    memoryManager->removeFromTrimCandidateList(&allocation);

    EXPECT_EQ(0u, memoryManager->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removeFromTrimCandidateListRemovesLastAllocationAndAllPreviousEmptyEntries) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);

    memoryManager->trimCandidateList.push_back(nullptr);
    memoryManager->trimCandidateList.push_back(nullptr);
    memoryManager->trimCandidateList.push_back(nullptr);

    memoryManager->addToTrimCandidateList(&allocation2);

    EXPECT_EQ(5u, memoryManager->trimCandidateList.size());

    memoryManager->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(1u, memoryManager->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, successiveAddingToTrimCandidateListAssignsNewPositions) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    EXPECT_EQ(3u, memoryManager->trimCandidateList.size());
    EXPECT_NE(allocation1.getTrimCandidateListPosition(), allocation2.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, DISABLED_removingNotLastAllocationFromTrimCandidateListSubstituesLastPositionAllocation) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    memoryManager->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(2u, memoryManager->trimCandidateList.size());

    EXPECT_EQ(2u, allocation3.getTrimCandidateListPosition());
    EXPECT_NE(allocation2.getTrimCandidateListPosition(), allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, removingNotLastAllocationFromTrimCandidateListPutsNullEntry) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3;

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);
    size_t position2 = allocation2.getTrimCandidateListPosition();
    size_t position3 = allocation3.getTrimCandidateListPosition();

    memoryManager->removeFromTrimCandidateList(&allocation2);

    EXPECT_EQ(3u, memoryManager->trimCandidateList.size());
    EXPECT_EQ(2u, position3);
    EXPECT_EQ(nullptr, memoryManager->trimCandidateList[position2]);
}

HWTEST_F(WddmMemoryManagerResidencyTest, compactTrimCandidateListRemovesInitialNullEntriesAndUpdatesPositions) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);
    memoryManager->addToTrimCandidateList(&allocation4);

    size_t position3 = allocation3.getTrimCandidateListPosition();
    size_t position4 = allocation4.getTrimCandidateListPosition();

    memoryManager->removeFromTrimCandidateList(&allocation2);
    memoryManager->removeFromTrimCandidateList(&allocation1);

    EXPECT_EQ(4u, memoryManager->trimCandidateList.size());

    memoryManager->compactTrimCandidateList();

    EXPECT_EQ(2u, memoryManager->trimCandidateList.size());

    EXPECT_EQ(memoryManager->trimCandidateList[0], &allocation3);
    EXPECT_EQ(0u, allocation3.getTrimCandidateListPosition());

    EXPECT_EQ(memoryManager->trimCandidateList[1], &allocation4);
    EXPECT_EQ(1u, allocation4.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, compactTrimCandidateListWithNonNullEntries) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);
    memoryManager->addToTrimCandidateList(&allocation4);

    EXPECT_EQ(4u, memoryManager->trimCandidateList.size());

    memoryManager->compactTrimCandidateList();

    EXPECT_EQ(4u, memoryManager->trimCandidateList.size());
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksAllocationsResident) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(&allocation2);
    memoryManager->pushAllocationForResidency(&allocation3);
    memoryManager->pushAllocationForResidency(&allocation4);

    memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
    EXPECT_TRUE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
    EXPECT_TRUE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsUpdatesLastFence) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(&allocation2);
    memoryManager->pushAllocationForResidency(&allocation3);
    memoryManager->pushAllocationForResidency(&allocation4);

    wddm->getMonitoredFence().currentFenceValue = 20;

    memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_EQ(20u, allocation1.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation2.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation3.getResidencyData().lastFence);
    EXPECT_EQ(20u, allocation4.getResidencyData().lastFence);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsMarksTripleAllocationsResident) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, ptr);

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(allocationTriple);
    memoryManager->pushAllocationForResidency(&allocation2);

    memoryManager->makeResidentResidencyAllocations(nullptr);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    EXPECT_EQ(5u, gdi->getMakeResidentArg().NumAllocations);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, makeResidentResidencyAllocationsSetsLastFencePLusOneForTripleAllocations) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;

    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, reinterpret_cast<void *>(0x1500));

    wddm->getMonitoredFence().currentFenceValue = 20;

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(allocationTriple);
    memoryManager->pushAllocationForResidency(&allocation2);

    memoryManager->makeResidentResidencyAllocations(nullptr);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_EQ(20u, allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->lastFence);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimCallbackIsRegisteredInWddmMemoryManagerCtor) {
    SetUpMm<FamilyType>();
    EXPECT_EQ((PFND3DKMT_TRIMNOTIFICATIONCALLBACK)memoryManager->trimCallback, gdi->getRegisterTrimNotificationArg().Callback);
    EXPECT_EQ(reinterpret_cast<void *>(memoryManager.get()), gdi->getRegisterTrimNotificationArg().Context);
    EXPECT_EQ(wddm->getDevice(), gdi->getRegisterTrimNotificationArg().hDevice);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenNotUsedAllocationsFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenAllocationsAreEvictedMarkedAndRemovedFromTrimCandidateList) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    WddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    memoryManager->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 allocations evicted
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    // removed from trim candidate list
    EXPECT_EQ(0u, memoryManager->trimCandidateList.size());
    // marked nonresident
    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenOneUsedAllocationFromPreviousPeriodicTrimWhenTrimResidencyPeriodicTrimIsCalledThenOneAllocationIsTrimmed) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // allocations have fence value == 0 by default
    WddmAllocation allocation1, allocation2;
    allocation1.getResidencyData().resident = true;
    // mark allocation used from last periodic trim
    allocation2.getResidencyData().lastFence = 11;
    allocation2.getResidencyData().resident = true;

    // Set last periodic fence value
    memoryManager->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);

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

HWTEST_F(WddmMemoryManagerResidencyTest, givenTripleAllocationWithUsedAndUnusedFragmentsSincePreviousTrimWhenTrimResidencyPeriodicTrimIsCalledThenProperFragmentsAreEvictedAndMarked) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1500);
    // 3-fragment Allocation
    WddmAllocation *allocationTriple = (WddmAllocation *)memoryManager->allocateGraphicsMemory(8196, ptr);
    // whole allocation unused since previous trim
    allocationTriple->getResidencyData().lastFence = 0;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->lastFence = 0;
    allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident = true;
    // this fragment was used
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->lastFence = 11;
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident = true;
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->lastFence = 0;
    allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident = true;

    // Set last periodic fence value
    memoryManager->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    wddm->getMonitoredFence().currentFenceValue = 20;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(allocationTriple);

    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    // 2 fragments evicted with one call
    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    // marked nonresident
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.PeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    memoryManager->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    *wddm->getMonitoredFence().cpuAddress = 20;

    memoryManager->trimCandidateList.resize(0);
    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, memoryManager->lastPeriodicTrimFenceValue);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenRestartPeriodicTrimWhenTrimCallbackCalledThenLastPeriodicTrimFenceIsSetToCurrentFenceValue) {
    SetUpMm<FamilyType>();
    D3DKMT_TRIMNOTIFICATION trimNotification = {0};
    trimNotification.Flags.RestartPeriodicTrim = 1;
    trimNotification.NumBytesToTrim = 0;

    // Set last periodic fence value
    memoryManager->lastPeriodicTrimFenceValue = 10;
    // Set current fence value to greater value
    *wddm->getMonitoredFence().cpuAddress = 20;

    memoryManager->trimCandidateList.resize(0);
    memoryManager->trimResidency(trimNotification.Flags, trimNotification.NumBytesToTrim);

    EXPECT_EQ(20u, memoryManager->lastPeriodicTrimFenceValue);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWithZeroSizeReturnsTrue) {
    SetUpMm<FamilyType>();
    bool status = memoryManager->trimResidencyToBudget(0);

    EXPECT_TRUE(status);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetAllDoneAllocations) {
    SetUpMm<FamilyType>();
    gdi->setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1, allocation2, allocation3;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_EQ(1u, memoryManager->trimCandidatesCount);
    memoryManager->compactTrimCandidateList();
    EXPECT_EQ(1u, memoryManager->trimCandidateList.size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetReturnsFalseWhenNumBytesToTrimIsNotZero) {
    SetUpMm<FamilyType>();
    gdi->setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;

    wddm->makeNonResidentResult.called = 0;
    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(0u, memoryManager->trimCandidateList.size());

    EXPECT_FALSE(status);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetStopsEvictingWhenNumBytesToTrimIsZero) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1(reinterpret_cast<void *>(0x1000), 0x1000, reinterpret_cast<void *>(0x1000), 0x1000, nullptr),
        allocation2(reinterpret_cast<void *>(0x1000), 0x3000, reinterpret_cast<void *>(0x1000), 0x3000, nullptr),
        allocation3(reinterpret_cast<void *>(0x1000), 0x1000, reinterpret_cast<void *>(0x1000), 0x1000, nullptr);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_TRUE(status);
    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);
    EXPECT_EQ(1u, memoryManager->trimCandidateList.size());

    EXPECT_EQ(trimListUnusedPosition, allocation1.getTrimCandidateListPosition());
    EXPECT_EQ(trimListUnusedPosition, allocation2.getTrimCandidateListPosition());
    EXPECT_NE(trimListUnusedPosition, allocation3.getTrimCandidateListPosition());
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetMarksEvictedAllocationNonResident) {
    SetUpMm<FamilyType>();
    gdi->setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1, allocation2, allocation3;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 2;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetWaitsFromCpuWhenLastFenceIsGreaterThanMonitored) {
    SetUpMm<FamilyType>();
    gdi->setNonZeroNumBytesToTrimInEvict();

    WddmAllocation allocation1;
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 2;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 2;
    wddm->getMonitoredFence().currentFenceValue = 3;

    wddm->makeNonResidentResult.called = 0;
    wddm->waitFromCpuResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);

    gdi->getWaitFromCpuArg().hDevice = (D3DKMT_HANDLE)0;

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(1u, wddm->makeNonResidentResult.called);
    EXPECT_FALSE(allocation1.getResidencyData().resident);

    EXPECT_EQ(wddm->getDevice(), gdi->getWaitFromCpuArg().hDevice);
}

HWTEST_F(WddmMemoryManagerResidencyTest, trimToBudgetEvictsDoneFragmentsOnly) {
    SetUpMm<FamilyType>();
    gdi->setNonZeroNumBytesToTrimInEvict();
    void *ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    WddmAllocation allocation1(ptr, 0x1000, ptr, 0x1000, nullptr);
    WddmAllocation allocation2(ptr, 0x1000, ptr, 0x1000, nullptr);
    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    void *ptrTriple = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ptr) + 0x500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptrTriple));

    allocationTriple->getResidencyData().lastFence = 1;
    allocationTriple->getResidencyData().resident = true;

    EXPECT_EQ(3u, allocationTriple->fragmentsStorage.fragmentCount);

    for (uint32_t i = 0; i < 3; i++) {
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->lastFence = 1;
        allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident = true;
    }

    // This should not be evicted
    allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->lastFence = 2;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(allocationTriple);
    memoryManager->addToTrimCandidateList(&allocation2);

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 2;

    wddm->makeNonResidentResult.called = 0;

    bool status = memoryManager->trimResidencyToBudget(3 * 4096);

    EXPECT_EQ(2u, wddm->makeNonResidentResult.called);

    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[0].residency->resident);
    EXPECT_TRUE(allocationTriple->fragmentsStorage.fragmentStorageData[1].residency->resident);
    EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[2].residency->resident);

    memoryManager->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerResidencyTest, checkTrimCandidateListCompaction) {
    SetUpMm<FamilyType>();
    memoryManager->trimCandidatesCount = 10;
    memoryManager->trimCandidateList.resize(20);

    bool comapctionRequired = memoryManager->checkTrimCandidateListCompaction();

    EXPECT_TRUE(comapctionRequired);

    memoryManager->trimCandidatesCount = 5;
    memoryManager->trimCandidateList.resize(20);

    comapctionRequired = memoryManager->checkTrimCandidateListCompaction();

    EXPECT_TRUE(comapctionRequired);

    memoryManager->trimCandidatesCount = 18;
    memoryManager->trimCandidateList.resize(20);

    comapctionRequired = memoryManager->checkTrimCandidateListCompaction();

    EXPECT_FALSE(comapctionRequired);
}

HWTEST_F(WddmMemoryManagerResidencyTest, givenThreeAllocationsAlignedSizeBiggerThanAllocSizeWhenBudgetEqualTwoAlignedAllocationThenEvictOnlyTwo) {
    SetUpMm<FamilyType>();
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

    WddmAllocation allocation1(ptr1, underlyingSize, ptr1, alignedSize, nullptr);
    WddmAllocation allocation2(ptr2, underlyingSize, ptr2, alignedSize, nullptr);
    WddmAllocation allocation3(ptr3, underlyingSize, ptr3, alignedSize, nullptr);

    allocation1.getResidencyData().resident = true;
    allocation1.getResidencyData().lastFence = 0;

    allocation2.getResidencyData().lastFence = 1;
    allocation2.getResidencyData().resident = true;

    allocation3.getResidencyData().lastFence = 1;
    allocation3.getResidencyData().resident = true;

    *wddm->getMonitoredFence().cpuAddress = 1;
    wddm->getMonitoredFence().lastSubmittedFence = 1;
    wddm->getMonitoredFence().currentFenceValue = 1;

    wddm->makeNonResidentResult.called = 0;

    memoryManager->trimCandidateList.resize(0);

    memoryManager->addToTrimCandidateList(&allocation1);
    memoryManager->addToTrimCandidateList(&allocation2);
    memoryManager->addToTrimCandidateList(&allocation3);

    bool status = memoryManager->trimResidencyToBudget(budget);
    EXPECT_TRUE(status);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_TRUE(allocation3.getResidencyData().resident);
}

HWTEST_F(BufferWithWddmMemory, ValidHostPtr) {
    SetUpMm<FamilyType>();
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

HWTEST_F(BufferWithWddmMemory, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    SetUpMm<FamilyType>();
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

HWTEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    SetUpMm<FamilyType>();
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = memoryManager->hostPtrManager;

    ASSERT_EQ(3u, hostPtrManager.getFragmentCount());

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < max_fragments_count; i++) {

        uintptr_t GpuPtr = (uintptr_t)(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->gpuPtr);
        uintptr_t CpuPtr = (uintptr_t)(reqs.AllocationFragments[i].allocationPtr);
        EXPECT_EQ(CpuPtr, GpuPtr);
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

HWTEST_F(BufferWithWddmMemory, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkAllocationsResidentWhenMakeResidentFails) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(&allocation2);
    memoryManager->pushAllocationForResidency(&allocation3);
    memoryManager->pushAllocationForResidency(&allocation4);

    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsDoesNotMarkTripleAllocationsResidentWhenMakeResidentFails) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2;
    void *ptr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1500);
    WddmAllocation *allocationTriple = static_cast<WddmAllocation *>(memoryManager->allocateGraphicsMemory(8196, ptr));
    ASSERT_NE(nullptr, allocationTriple);

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(allocationTriple);
    memoryManager->pushAllocationForResidency(&allocation2);

    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    for (uint32_t i = 0; i < allocationTriple->fragmentsStorage.fragmentCount; i++) {
        EXPECT_FALSE(allocationTriple->fragmentsStorage.fragmentStorageData[i].residency->resident);
    }

    memoryManager->freeGraphicsMemory(allocationTriple);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsFailsWhenMakeResidentFailsAndCantTrimFurther) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1, allocation2, allocation3, allocation4;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2);

    memoryManager->pushAllocationForResidency(&allocation1);
    memoryManager->pushAllocationForResidency(&allocation2);
    memoryManager->pushAllocationForResidency(&allocation3);
    memoryManager->pushAllocationForResidency(&allocation4);

    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);

    EXPECT_FALSE(allocation1.getResidencyData().resident);
    EXPECT_FALSE(allocation2.getResidencyData().resident);
    EXPECT_FALSE(allocation3.getResidencyData().resident);
    EXPECT_FALSE(allocation4.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsCallsMakeResidentWithCantTrimFurtherSetToTrueWhenTrimToBudgetReturnsFalse) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = 4 * 4096;  return false; };

    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, false, ::testing::_)).Times(1);
    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, true, ::testing::_)).Times(1);

    memoryManager->pushAllocationForResidency(&allocation1);

    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_FALSE(result);
}

HWTEST_F(WddmMemoryManagerTest2, givenAllocationPackWhenTheyArePassedToMakeResidentThenTheyAreUsedInsteadOfMemoryManagerMembers) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;
    WddmAllocation allocation2;
    WddmAllocation allocation3;
    allocation1.handle = 1;
    allocation2.handle = 2;
    allocation3.handle = 3;

    ResidencyContainer residencyPack;
    residencyPack.push_back(&allocation1);
    residencyPack.push_back(&allocation2);

    auto makeResidentWithOutBytesToTrim = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool {
        EXPECT_EQ(1, handles[0]);
        EXPECT_EQ(2, handles[1]);
        return true;
    };
    ON_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).WillByDefault(::testing::Invoke(makeResidentWithOutBytesToTrim));
    EXPECT_CALL(*wddm, makeResident(::testing::_, 2, false, ::testing::_)).Times(1);

    memoryManager->pushAllocationForResidency(&allocation3);
    bool result = memoryManager->makeResidentResidencyAllocations(&residencyPack);
    EXPECT_TRUE(result);
}

HWTEST_F(WddmMemoryManagerTest2, makeResidentResidencyAllocationsSucceedsWhenMakeResidentFailsAndTrimToBudgetSucceeds) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;
    void *cpuPtr = reinterpret_cast<void *>(wddm->getWddmMinAddress() + 0x1000);
    size_t allocationSize = 0x1000;
    WddmAllocation allocationToTrim(cpuPtr, allocationSize, cpuPtr, allocationSize, nullptr);

    allocationToTrim.getResidencyData().lastFence = wddm->getMonitoredFence().lastSubmittedFence;

    auto makeResidentWithOutBytesToTrim = [allocationSize](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { *numberOfBytesToTrim = allocationSize;  return false; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentWithOutBytesToTrim)).WillOnce(::testing::Return(true));

    memoryManager->addToTrimCandidateList(&allocationToTrim);

    memoryManager->pushAllocationForResidency(&allocation1);

    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);

    EXPECT_TRUE(result);

    EXPECT_TRUE(allocation1.getResidencyData().resident);
}

HWTEST_F(WddmMemoryManagerTest2, givenMemoryManagerWhenMakeResidentFailsThenMemoryBudgetExhaustedIsReturnedAsTrue) {
    SetUpMm<FamilyType>();
    WddmAllocation allocation1;

    auto makeResidentThatFails = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return false; };
    auto makeResidentThatSucceds = [](D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim) -> bool { return true; };

    EXPECT_CALL(*wddm, makeResident(::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(2).WillOnce(::testing::Invoke(makeResidentThatFails)).WillOnce(::testing::Invoke(makeResidentThatSucceds));

    memoryManager->pushAllocationForResidency(&allocation1);
    bool result = memoryManager->makeResidentResidencyAllocations(nullptr);
    EXPECT_TRUE(memoryManager->isMemoryBudgetExhausted());
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsEnabledThenCanDeferDeletions) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);
    EXPECT_EQ(0, deleter->deferDeletionCalled);
    memoryManager.tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1, deleter->deferDeletionCalled);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenWddmWhenAsyncDeleterIsDisabledThenCannotDeferDeletions) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(nullptr);
    memoryManager.tryDeferDeletions(nullptr, 0, 0, 0);
    EXPECT_EQ(1u, wddm->destroyAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenDrainIsCalledAndCreateAllocationIsCalledTwice) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);

    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockDeferredDeleter *deleter = new MockDeferredDeleter;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(deleter);

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
    auto allocation = memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(WddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm);
    memoryManager.setDeferredDeleter(nullptr);

    cl_image_desc imgDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    ImageInfo imgInfo;
    imgInfo.imgDesc = &imgDesc;
    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    memoryManager.allocateGraphicsMemoryForImage(imgInfo, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

TEST(WddmMemoryManagerDefaults, givenDefaultWddmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenHeap1BaseIsReturned) {
    WddmMock *wddm = new WddmMock;
    wddm->callBaseDestroyAllocations = false;
    MockWddmMemoryManager memoryManager(wddm);
    auto heapBase = wddm->getGfxPartition().Heap32[1].Base;
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress());
}

HWTEST_F(MockWddmMemoryManagerTest, givenValidateAllocationFunctionWhenItIsCalledWithTripleAllocationThenSuccessIsReturned) {
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    MockWddmMemoryManager memoryManager(wddm);

    auto wddmAlloc = (WddmAllocation *)memoryManager.allocateGraphicsMemory(4096u, reinterpret_cast<void *>(0x1000));

    EXPECT_TRUE(memoryManager.validateAllocationMock(wddmAlloc));

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhencreateGraphicsAllocationWithRequiredBitnessThenAllocated64kbAdress) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager64k(true, wddm);
    EXPECT_EQ(0, wddm->createAllocationResult.called);
    GraphicsAllocation *galloc = memoryManager64k.createGraphicsAllocationWithRequiredBitness(64 * 1024, nullptr, false);
    EXPECT_EQ(1, wddm->createAllocationResult.called);
    EXPECT_NE(nullptr, galloc);
    EXPECT_EQ(true, galloc->isLocked());
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % 65536U);
    memoryManager64k.freeGraphicsMemory(galloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenEnabled64kbpagesWhenSetLockThenLockIsSet) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager64k(true, wddm);
    EXPECT_EQ(0, wddm->createAllocationResult.called);
    GraphicsAllocation *galloc = memoryManager64k.createGraphicsAllocationWithRequiredBitness(64 * 1024, nullptr, false);
    galloc->setLocked(false);
    EXPECT_FALSE(galloc->isLocked());
    galloc->setLocked(true);
    EXPECT_TRUE(galloc->isLocked());
    memoryManager64k.freeGraphicsMemory(galloc);
}

HWTEST_F(OsAgnosticMemoryManagerUsingWddmTest, GivenEnabled64kbPagesWhenAllocationIsCreatedWithSizeSmallerThen64KBThenGraphicsAllocationsHas64KBAlignedUnderlyingsize) {
    DebugManagerStateRestore dbgRestore;
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMemoryManager memoryManager(true, wddm);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationWithRequiredBitness(1, nullptr);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(64 * MemoryConstants::kiloByte, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_NE(0llu, graphicsAllocation->getGpuAddress());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), graphicsAllocation->getUnderlyingBuffer());

    EXPECT_EQ(1u, graphicsAllocation->gmm->resourceParams.Flags.Info.Cacheable);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(MockWddmMemoryManagerTest, givenWddmWhenallocateGraphicsMemory64kbThenLockResultAndmapGpuVirtualAddressIsCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    WddmMock *wddm = new WddmMock;
    EXPECT_TRUE(wddm->init<FamilyType>());
    MockWddmMemoryManager memoryManager64k(wddm);
    uint32_t lockCount = wddm->lockResult.called;
    uint32_t mapGpuVirtualAddressResult = wddm->mapGpuVirtualAddressResult.called;
    GraphicsAllocation *galloc = memoryManager64k.allocateGraphicsMemory64kb(65536, 65536, true);
    EXPECT_EQ(lockCount + 1, wddm->lockResult.called);
    EXPECT_EQ(mapGpuVirtualAddressResult + 1, wddm->mapGpuVirtualAddressResult.called);
    EXPECT_NE(wddm->mapGpuVirtualAddressResult.cpuPtrPassed, nullptr);
    memoryManager64k.freeGraphicsMemory(galloc);
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MockWddmMemoryManagerTest, givenDefaultWddmMemoryManagerWhenItIsCreatedThenMemoryBudgetIsNotExhausted) {
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST_F(MockWddmMemoryManagerTest, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsTrueAndDeleterIsNotNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST_F(MockWddmMemoryManagerTest, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    WddmMock *wddm = new WddmMock;
    WddmMemoryManager memoryManager(false, wddm);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

HWTEST_F(MockWddmMemoryManagerTest, givenPageTableManagerWhenMapAuxGpuVaCalledThenUseWddmToMap) {
    auto myWddm = new WddmMock();
    EXPECT_TRUE(myWddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, myWddm);

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

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenMappedGpuVaThenMapAuxVa) {
    std::unique_ptr<Gmm> gmm(GmmHelper::create(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = true;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    GMM_DDI_UPDATEAUXTABLE givenDdiUpdateAuxTable = {};
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = GmmHelper::canonize(wddm.getGfxPartition().Standard.Base);
    expectedDdiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(1).WillOnce(Invoke([&](const GMM_DDI_UPDATEAUXTABLE *arg) {givenDdiUpdateAuxTable = *arg; return GMM_SUCCESS; }));

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false, false);
    ASSERT_TRUE(result);
    EXPECT_EQ(GmmHelper::canonize(wddm.getGfxPartition().Standard.Base), gpuVa);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &givenDdiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedAllocationWhenReleaseingThenUnmapAuxVa) {
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 123;

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u, 4096u));
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

HWTEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenReleaseingThenDontUnmapAuxVa) {
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u, 4096u));
    wddmAlloc->gmm->isRenderCompressed = false;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    memoryManager.freeGraphicsMemory(wddmAlloc);
}

HWTEST_F(MockWddmMemoryManagerTest, givenNonRenderCompressedAllocationWhenMappedGpuVaThenDontMapAuxVa) {
    std::unique_ptr<Gmm> gmm(GmmHelper::create(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm.resetPageTableManager(mockMngr);

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false, false);
    ASSERT_TRUE(result);
}

HWTEST_F(MockWddmMemoryManagerTest, givenFailingAllocationWhenMappedGpuVaThenReturnFalse) {
    std::unique_ptr<Gmm> gmm(GmmHelper::create(reinterpret_cast<void *>(123), 4096u, false));
    gmm->isRenderCompressed = false;
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock wddm;
    EXPECT_TRUE(wddm.init<FamilyType>());

    auto result = wddm.mapGpuVirtualAddressImpl(gmm.get(), 0, nullptr, 3, gpuVa, false, false, false);
    ASSERT_FALSE(result);
}

HWTEST_F(MockWddmMemoryManagerTest, givenRenderCompressedFlagSetWhenInternalIsUnsetThenDontUpdateAuxTable) {
    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0;
    WddmMock *wddm = new WddmMock();
    EXPECT_TRUE(wddm->init<FamilyType>());
    WddmMemoryManager memoryManager(false, wddm);

    auto mockMngr = new NiceMock<MockGmmPageTableMngr>();
    wddm->resetPageTableManager(mockMngr);

    auto myGmm = GmmHelper::create(reinterpret_cast<void *>(123), 4096u, false);
    myGmm->isRenderCompressed = false;
    myGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = 1;

    auto wddmAlloc = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemory(4096u, 4096u));
    delete wddmAlloc->gmm;
    wddmAlloc->gmm = myGmm;

    EXPECT_CALL(*mockMngr, updateAuxTable(_)).Times(0);

    auto result = wddm->mapGpuVirtualAddressImpl(myGmm, ALLOCATION_HANDLE, nullptr, 3, gpuVa, false, false, false);
    EXPECT_TRUE(result);
    memoryManager.freeGraphicsMemory(wddmAlloc);
}

HWTEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryWhenCreateAllocationFailsThenPopulateOsHandlesReturnsInvalidPointer) {
    SetUpMm<FamilyType>();
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

HWTEST_F(WddmMemoryManagerTest2, givenReadOnlyMemoryPassedToPopulateOsHandlesWhenCreateAllocationFailsThenAllocatedFragmentsAreNotStored) {
    SetUpMm<FamilyType>();
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
