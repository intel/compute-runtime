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

#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/device_command_stream.h"
#include "hw_cmds.h"
#include "runtime/event/event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/utilities/tag_allocator.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "test.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"
#include "runtime/os_interface/32bit_memory.h"
#include "unit_tests/mocks/mock_32bitAllocator.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <iostream>
#include <memory>

using namespace OCLRT;

static off_t lseekReturn = 4096u;
static int lseekCalledCount = 0;
static int mmapMockCallCount = 0;
static int munmapMockCallCount = 0;

off_t lseekMock(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
void *mmapMock(void *addr, size_t length, int prot, int flags,
               int fd, long offset) noexcept {
    mmapMockCallCount++;
    return (void *)(0x1000);
}

int munmapMock(void *addr, size_t length) noexcept {
    munmapMockCallCount++;
    return 0;
}

int closeMock(int) {
    return 0;
}

class TestedDrmMemoryManager : public DrmMemoryManager {
  public:
    TestedDrmMemoryManager(Drm *drm) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerConsumingCommandBuffers, false) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    };
    TestedDrmMemoryManager(Drm *drm, bool allowForcePin) : DrmMemoryManager(drm, gemCloseWorkerMode::gemCloseWorkerConsumingCommandBuffers, allowForcePin) {
        this->lseekFunction = &lseekMock;
        this->mmapFunction = &mmapMock;
        this->munmapFunction = &munmapMock;
        this->closeFunction = &closeMock;
        lseekReturn = 4096;
        lseekCalledCount = 0;
        mmapMockCallCount = 0;
        munmapMockCallCount = 0;
    }

    void unreference(BufferObject *bo) {
        DrmMemoryManager::unreference(bo);
    }

    BufferObject *allocUserptr(uintptr_t address, size_t size, uint64_t flags, bool softpin) {
        return DrmMemoryManager::allocUserptr(address, size, flags, softpin);
    }
    DrmGemCloseWorker *getgemCloseWorker() { return this->gemCloseWorker.get(); }
    Allocator32bit *getDrmInternal32BitAllocator() { return internal32bitAllocator.get(); }
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    TestedDrmMemoryManager *memoryManager = nullptr;
    DrmMockCustom *mock;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        this->mock = new DrmMockCustom;

        memoryManager = new (std::nothrow) TestedDrmMemoryManager(this->mock);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        memoryManager->getgemCloseWorker()->close(true);
    }

    void TearDown() override {
        delete memoryManager;

        if (this->mock->ioctl_expected >= 0) {
            EXPECT_EQ(this->mock->ioctl_expected, this->mock->ioctl_cnt);
        }

        delete this->mock;
        this->mock = nullptr;
        MemoryManagementFixture::TearDown();
    }

  protected:
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
};

typedef Test<DrmMemoryManagerFixture> DrmMemoryManagerTest;

TEST_F(DrmMemoryManagerTest, givenDefaultDrmMemoryMangerWhenItIsCreatedThenItContainsInternal32BitAllocator) {
    mock->ioctl_expected = 0;
    EXPECT_NE(nullptr, memoryManager->getDrmInternal32BitAllocator());
}

TEST_F(DrmMemoryManagerTest, pinBBnotCreated) {
    mock->ioctl_expected = 0;
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, pinBBisCreated) {
    //we don't check destructor, MM fixture will do this (leak detection)
    mock->ioctl_expected = 2;
    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    EXPECT_NE(nullptr, mm->getPinBB());
    delete mm;
}

TEST_F(DrmMemoryManagerTest, pinBBnotCreatedWhenIoctlFailed) {
    mock->ioctl_expected = 1;
    mock->ioctl_res = -1;
    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    EXPECT_EQ(nullptr, mm->getPinBB());
    delete mm;
}

TEST_F(DrmMemoryManagerTest, pinAfterAllocateWhenAskedAndAllowedAndBigAllocation) {
    mock->ioctl_expected = 3 + 2 + 1;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    auto alloc = mm->allocateGraphicsMemory(10 * 1014 * 1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedAndAllowedButSmallAllocation) {
    mock->ioctl_expected = 3 + 2;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    // one page is too small for early pinning
    auto alloc = mm->allocateGraphicsMemory(4 * 1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowed) {
    mock->ioctl_expected = 3 + 2;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    auto alloc = mm->allocateGraphicsMemory(1024, 1024, false, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowed) {
    mock->ioctl_expected = 3;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, false);
    ASSERT_EQ(nullptr, mm->getPinBB());

    auto alloc = mm->allocateGraphicsMemory(1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
}

// ---- HostPtr
TEST_F(DrmMemoryManagerTest, pinAfterAllocateWhenAskedAndAllowedAndBigAllocationHostPtr) {
    mock->ioctl_expected = 3 + 2 + 1;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    size_t size = 10 * 1024 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = mm->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedAndAllowedButSmallAllocationHostPtr) {
    mock->ioctl_expected = 3 + 2;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    // one page is too small for early pinning
    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = mm->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowedHostPtr) {
    mock->ioctl_expected = 3 + 2;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, true);
    ASSERT_NE(nullptr, mm->getPinBB());

    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = mm->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowedHostPtr) {
    mock->ioctl_expected = 3;

    auto mm = new (std::nothrow) TestedDrmMemoryManager(this->mock, false);
    ASSERT_EQ(nullptr, mm->getPinBB());

    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = mm->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    mm->freeGraphicsMemory(alloc);

    delete mm;
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, unreference) {
    mock->ioctl_expected = 2; //create+close
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul, true);
    ASSERT_NE(nullptr, bo);
    memoryManager->unreference(bo);
}

TEST_F(DrmMemoryManagerTest, UnreferenceNullPtr) {
    memoryManager->unreference(nullptr);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerModeInactiveThenGemCloseWorkerIsNotCreated) {
    class MyTestedDrmMemoryManager : public DrmMemoryManager {
      public:
        MyTestedDrmMemoryManager(Drm *drm, gemCloseWorkerMode mode) : DrmMemoryManager(drm, mode, false) {}
        DrmGemCloseWorker *getgemCloseWorker() { return this->gemCloseWorker.get(); }
    };

    MyTestedDrmMemoryManager drmMemoryManger(this->mock, gemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_EQ(nullptr, drmMemoryManger.getgemCloseWorker());
}

TEST_F(DrmMemoryManagerTest, AllocateThenFree) {
    mock->ioctl_expected = 3;

    auto alloc = memoryManager->allocateGraphicsMemory(1024, 1024);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, AllocateNewFail) {
    mock->ioctl_expected = -1; //don't care

    InjectedFunction method = [this](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemory(1024, 1024);

        if (nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, ptr);
        } else {
            EXPECT_NE(nullptr, ptr);
            memoryManager->freeGraphicsMemory(ptr);
        }
    };
    injectFailures(method);
}

TEST_F(DrmMemoryManagerTest, Allocate0Bytes) {
    mock->ioctl_expected = 3;

    auto ptr = memoryManager->allocateGraphicsMemory(static_cast<size_t>(0), static_cast<size_t>(0));
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate3Bytes) {
    mock->ioctl_expected = 3;

    auto ptr = memoryManager->allocateGraphicsMemory(3, 3);
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, AllocateUserptrFail) {
    mock->ioctl_expected = 1;
    mock->ioctl_res = -1;

    auto ptr = memoryManager->allocateGraphicsMemory(3, 3);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(DrmMemoryManagerTest, FreeNullPtr) {
    memoryManager->freeGraphicsMemory(nullptr);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr) {
    mock->ioctl_expected = 3;

    void *ptr = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptr);

    auto alloc = memoryManager->allocateGraphicsMemory(1024, ptr);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptr, bo->peekAddress());
    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr_Nullptr) {
    mock->ioctl_expected = 3;

    void *ptr = nullptr;

    auto alloc = memoryManager->allocateGraphicsMemory(1024, ptr);
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptr, bo->peekAddress());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr_MisAligned) {
    mock->ioctl_expected = 3;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    void *ptr = ptrOffset(ptrT, 128);

    auto alloc = memoryManager->allocateGraphicsMemory(1024, ptr);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptrT, bo->peekAddress());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptrT);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr_UserptrFail) {
    mock->ioctl_expected = 1;
    mock->ioctl_res = -1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    auto alloc = memoryManager->allocateGraphicsMemory(1024, ptrT);
    EXPECT_EQ(nullptr, alloc);

    ::alignedFree(ptrT);
}

TEST_F(DrmMemoryManagerTest, getSystemSharedMemory) {
    uint64_t mem = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected = 1; // get aperture
    EXPECT_GT(mem, 0u);
}

TEST_F(DrmMemoryManagerTest, getMaxApplicationAddress) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if (is64bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    }
}

TEST_F(DrmMemoryManagerTest, getMinimumSystemSharedMemory) {
    auto hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));

    // gpuMemSize < hostMemSize
    auto gpuMemorySize = hostMemorySize - 1u;
    mock->gpuMemSize = gpuMemorySize;
    uint64_t systemSharedMemorySize = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected = 1; // get aperture
    EXPECT_EQ(gpuMemorySize, systemSharedMemorySize);

    // gpuMemSize > hostMemSize
    gpuMemorySize = hostMemorySize + 1u;
    mock->gpuMemSize = gpuMemorySize;
    systemSharedMemorySize = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected = 2; // get aperture again
    EXPECT_EQ(hostMemorySize, systemSharedMemorySize);
}

TEST_F(DrmMemoryManagerTest, BoWaitFailure) {
    testing::internal::CaptureStderr();
    mock->ioctl_expected = 3; //create+wait+close
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul, true);
    ASSERT_NE(nullptr, bo);
    mock->ioctl_res = -EIO;
    EXPECT_THROW(bo->wait(-1), std::exception);
    mock->ioctl_res = 1;

    memoryManager->unreference(bo);
    testing::internal::GetCapturedStderr();
}

TEST_F(DrmMemoryManagerTest, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    mock->ioctl_expected = 3;
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    storage.fragmentStorageData[0].fragmentSize = 1;
    memoryManager->populateOsHandles(storage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage);
}

TEST_F(DrmMemoryManagerTest, GivenNoInputsWhenOsHandleIsCreatedThenAllBoHandlesAreInitializedAsNullPtrs) {
    OsHandle boHandle;
    EXPECT_EQ(nullptr, boHandle.bo);

    std::unique_ptr<OsHandle> boHandle2(new OsHandle);
    EXPECT_EQ(nullptr, boHandle2->bo);
}

TEST_F(DrmMemoryManagerTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {

    OsHandleStorage handleStorage;
    auto ptr = (void *)0x1000;
    auto ptr2 = (void *)0x1001;
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

    delete allocation;
}

TEST_F(DrmMemoryManagerTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocation64kThenNullPtr) {
    auto allocation = memoryManager->allocateGraphicsMemory64kb(65536, 65536, false);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    mock->ioctl_expected = 9;
    auto ptr = (void *)0x1001;
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(size, ptr);

    auto &hostPtrManager = memoryManager->hostPtrManager;

    ASSERT_EQ(3u, hostPtrManager.getFragmentCount());

    auto reqs = HostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < max_fragments_count; i++) {
        ASSERT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekSize());
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekAddress());
        EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekIsAllocated());
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(DrmMemoryManagerTest, testProfilingAllocatorCleanup) {
    mock->ioctl_expected = -1; //don't care
    MemoryManager *memMngr = memoryManager;
    TagAllocator<HwTimeStamps> *allocator = memMngr->getEventTsAllocator();
    EXPECT_NE(nullptr, allocator);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationThen32BitDrmAllocationIsBeingReturned) {
    mock->ioctl_expected = 3;
    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, MemoryType::EXTERNAL_ALLOCATION);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    uintptr_t address64bit = (uintptr_t)allocation->getGpuAddressToPatch();
    if (is32BitOsAllocatorAvailable) {
        EXPECT_LT(address64bit, max32BitAddress);
    }
    auto bo = allocation->getBO();
    EXPECT_GE(bo->peekUnmapSize(), 0u);
    EXPECT_TRUE(allocation->is32BitAllocation);

    EXPECT_EQ(memoryManager->allocator32Bit->getBase(), allocation->gpuBaseAddress);

    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_EXTERNAL);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhensetForce32BitAllocationsIsCalledWithTrueMutlipleTimesThenAllocatorIsReused) {
    mock->ioctl_expected = 0;
    EXPECT_EQ(nullptr, memoryManager->allocator32Bit.get());
    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    auto currentAllocator = memoryManager->allocator32Bit.get();
    memoryManager->setForce32BitAllocations(true);
    EXPECT_EQ(memoryManager->allocator32Bit.get(), currentAllocator);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhensetForce32BitAllocationsIsCalledWithFalseThenAllocatorIsNotDeleted) {
    mock->ioctl_expected = 0;
    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    memoryManager->setForce32BitAllocations(false);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
}

TEST_F(DrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferAllocationThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
        mock->ioctl_expected = 3;
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        memoryManager->setForce32BitAllocations(true);
        context.setMemoryManager(memoryManager);
        memoryManager->setForce32BitAllocations(true);

        auto size = MemoryConstants::pageSize;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_ALLOC_HOST_PTR,
            size,
            nullptr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
        auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;

        uintptr_t address64bit = (uintptr_t)bufferAddress;

        if (is32BitOsAllocatorAvailable) {
            EXPECT_LT(address64bit - baseAddress, max32BitAddress);
        }

        delete buffer;
}

TEST_F(DrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFromHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
        if (DebugManager.flags.UseNewHeapAllocator.get())
            mock->ioctl_expected = 3;
        else
            mock->ioctl_expected = 6;

        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        memoryManager->setForce32BitAllocations(true);
        context.setMemoryManager(memoryManager);

        auto size = MemoryConstants::pageSize;
        void *ptr = (void *)0x1000;
        auto ptrOffset = MemoryConstants::cacheLineSize;
        uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            (void *)offsetedPtr,
            retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(buffer->isMemObjZeroCopy());
        auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
        auto drmAllocation = (DrmAllocation *)buffer->getGraphicsAllocation();

        uintptr_t address64bitOnGpu = (uintptr_t)bufferAddress;

        if (is32BitOsAllocatorAvailable) {
            auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;
            EXPECT_LT(address64bitOnGpu - baseAddress, max32BitAddress);
        }

        EXPECT_TRUE(drmAllocation->is32BitAllocation);

        auto allocationCpuPtr = (uintptr_t)drmAllocation->getUnderlyingBuffer();
        auto allocationPageOffset = allocationCpuPtr - alignDown(allocationCpuPtr, MemoryConstants::pageSize);

        auto allocationGpuPtr = (uintptr_t)drmAllocation->getGpuAddress();
        auto allocationGpuOffset = allocationGpuPtr - alignDown(allocationGpuPtr, MemoryConstants::pageSize);

        auto bufferObject = drmAllocation->getBO();

        if (DebugManager.flags.UseNewHeapAllocator.get()) {
            EXPECT_NE(0u, bufferObject->peekUnmapSize());
        } else {
            EXPECT_EQ(0u, bufferObject->peekUnmapSize());
        }
        EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), (void *)offsetedPtr);

        // Gpu address should be different
        EXPECT_NE(offsetedPtr, drmAllocation->getGpuAddress());
        // Gpu address offset iqual to cpu offset
        EXPECT_EQ(allocationGpuOffset, ptrOffset);

        EXPECT_EQ(allocationPageOffset, ptrOffset);
        EXPECT_FALSE(bufferObject->peekIsAllocated());

        auto boAddress = bufferObject->peekAddress();
        EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

        delete buffer;
}

TEST_F(DrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFrom64BitHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    {
        if (is32bit) {
            mock->ioctl_expected = -1;
        } else {
            mock->ioctl_expected = 3;
            DebugManager.flags.Force32bitAddressing.set(true);
            MockContext context;
            memoryManager->setForce32BitAllocations(true);
            context.setMemoryManager(memoryManager);

            auto size = MemoryConstants::pageSize;
            void *ptr = (void *)0x100000000000;
            auto ptrOffset = MemoryConstants::cacheLineSize;
            uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
            auto retVal = CL_SUCCESS;

            auto buffer = Buffer::create(
                &context,
                CL_MEM_USE_HOST_PTR,
                size,
                (void *)offsetedPtr,
                retVal);
            EXPECT_EQ(CL_SUCCESS, retVal);

            EXPECT_TRUE(buffer->isMemObjZeroCopy());
            auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();

            uintptr_t address64bit = (uintptr_t)bufferAddress;

            if (is32BitOsAllocatorAvailable) {
                auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;
                EXPECT_LT(address64bit - baseAddress, max32BitAddress);
            }

            auto drmAllocation = (DrmAllocation *)buffer->getGraphicsAllocation();

            EXPECT_TRUE(drmAllocation->is32BitAllocation);

            auto allocationCpuPtr = (uintptr_t)drmAllocation->getUnderlyingBuffer();
            auto allocationPageOffset = allocationCpuPtr - alignDown(allocationCpuPtr, MemoryConstants::pageSize);
            auto bufferObject = drmAllocation->getBO();

            EXPECT_NE(0u, bufferObject->peekUnmapSize());

            if (DebugManager.flags.UseNewHeapAllocator.get() == false) {
                EXPECT_NE(drmAllocation->getUnderlyingBuffer(), (void *)offsetedPtr);
            }

            EXPECT_EQ(allocationPageOffset, ptrOffset);
            EXPECT_FALSE(bufferObject->peekIsAllocated());

            auto totalAllocationSize = alignSizeWholePage((void *)offsetedPtr, size);
            EXPECT_EQ(totalAllocationSize, bufferObject->peekUnmapSize());

            auto boAddress = bufferObject->peekAddress();
            EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

            delete buffer;
            DebugManager.flags.Force32bitAddressing.set(false);
        }
    }
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWithHostPtrAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected = 1;

    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    void *host_ptr = (void *)0x1000;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, host_ptr, MemoryType::EXTERNAL_ALLOCATION);

    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected = 1;

    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, MemoryType::EXTERNAL_ALLOCATION);

    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, GivenSizeAbove2GBWhenUseHostPtrAndAllocHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
        mock->ioctl_expected = -1;
        DebugManager.flags.Force32bitAddressing.set(true);
        MockContext context;
        memoryManager->setForce32BitAllocations(true);
        context.setMemoryManager(memoryManager);

        size_t size = 2 * 1024 * 1024 * 1024u;
        void *ptr = (void *)0x100000000000;
        auto retVal = CL_SUCCESS;

        auto buffer = Buffer::create(
            &context,
            CL_MEM_USE_HOST_PTR,
            size,
            (void *)ptr,
            retVal);

        size_t size2 = 2 * 1025 * 1024 * 1024u;

        auto buffer2 = Buffer::create(
            &context,
            CL_MEM_ALLOC_HOST_PTR,
            size2,
            nullptr,
            retVal);

        EXPECT_NE(retVal, CL_SUCCESS);
        EXPECT_EQ(nullptr, buffer2);

        if (is32BitOsAllocatorAvailable && buffer) {
            auto bufferPtr = buffer->getGraphicsAllocation()->getGpuAddress();

            if (DebugManager.flags.UseNewHeapAllocator.get() == false) {
                uintptr_t maxMmap32BitAddress = 0x80000000;
                EXPECT_EQ((uintptr_t)bufferPtr, maxMmap32BitAddress);
            }

            EXPECT_TRUE(buffer->getGraphicsAllocation()->is32BitAllocation);
            auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;
            EXPECT_LT((uintptr_t)(bufferPtr - baseAddress), max32BitAddress);
        }

        delete buffer;
}

TEST_F(DrmMemoryManagerTest, GivenSizeAbove2GBWhenAllocHostPtrAndUseHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected = -1;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context;
    memoryManager->setForce32BitAllocations(true);
    context.setMemoryManager(memoryManager);

    size_t size = 2 * 1024 * 1024 * 1024u;
    void *ptr = (void *)0x100000000000;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    size_t size2 = 2 * 1025 * 1024 * 1024u;

    auto buffer2 = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size2,
        (void *)ptr,
        retVal);

    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(nullptr, buffer2);

    if (is32BitOsAllocatorAvailable && buffer) {
        auto bufferPtr = buffer->getGraphicsAllocation()->getGpuAddress();

        if (DebugManager.flags.UseNewHeapAllocator.get() == false) {
            uintptr_t maxMmap32BitAddress = 0x80000000;
            EXPECT_EQ((uintptr_t)bufferPtr, maxMmap32BitAddress);
        }

        EXPECT_TRUE(buffer->getGraphicsAllocation()->is32BitAllocation);
        auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;
        EXPECT_LT((uintptr_t)(bufferPtr - baseAddress), max32BitAddress);
    }

    delete buffer;
}
TEST_F(DrmMemoryManagerTest, Given32BitDeviceWithMemoryManagerWhenAllHeapsAreExhaustedThenOptimizationIsTurningOfIfNoProgramsAreCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);
    mock->ioctl_expected = 3;
    std::unique_ptr<Device> pDevice(Device::create<OCLRT::MockDevice>(nullptr));
    memoryManager->device = pDevice.get();
    memoryManager->setForce32BitAllocations(true);

    mockAllocator32Bit::resetState();
    fail32BitMmap = true;
    failLowerRanger = true;
    failUpperRange = true;
    failMmap = true;
    mockAllocator32Bit::OsInternalsPublic *osInternals = mockAllocator32Bit::createOsInternals();
    osInternals->mmapFunction = MockMmap;
    osInternals->munmapFunction = MockMunmap;
    std::unique_ptr<mockAllocator32Bit> mock32BitAllocator(new mockAllocator32Bit(osInternals));

    memoryManager->allocator32Bit.reset(mock32BitAllocator.release());

    //ask for 4GB
    auto allocationSize = 4096u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationWithRequiredBitness(allocationSize, nullptr);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_FALSE(pDevice->getDeviceInfo().force32BitAddressess);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, Given32BitDeviceWithMemoryManagerWhenAllHeapsAreExhaustedAndThereAreProgramsThenOptimizationIsStillOnAndFailureIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.Force32bitAddressing.set(true);
    mock->ioctl_expected = 0;
    std::unique_ptr<Device> pDevice(Device::create<OCLRT::MockDevice>(nullptr));
    pDevice->increaseProgramCount();
    memoryManager->device = pDevice.get();
    memoryManager->setForce32BitAllocations(true);

    //ask for 4GB - 1
    size_t allocationSize = (4 * 1023 * 1024 * (size_t)1024u - 1) + 4 * 1024 * (size_t)1024u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationWithRequiredBitness(allocationSize, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);
    EXPECT_TRUE(pDevice->getDeviceInfo().force32BitAddressess);
}

TEST_F(DrmMemoryManagerTest, Given32BitDeviceWithMemoryManagerWhenInternalHeapIsExhaustedAndNewAllocationsIsMadeThenNullIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.Force32bitAddressing.set(true);
    mock->ioctl_expected = 0;
    memoryManager->setForce32BitAllocations(true);
    std::unique_ptr<Device> pDevice(Device::create<OCLRT::MockDevice>(nullptr));
    memoryManager->device = pDevice.get();

    auto allocator = memoryManager->getDrmInternal32BitAllocator();
    size_t size = 4 * GB - 4096;
    auto alloc = allocator->allocate(size);
    EXPECT_NE(nullptr, alloc);

    size_t allocationSize = 4096 * 3;
    auto graphicsAllocation = memoryManager->createInternalGraphicsAllocation(nullptr, allocationSize);
    EXPECT_EQ(nullptr, graphicsAllocation);
    EXPECT_TRUE(memoryManager->device->getDeviceInfo().force32BitAddressess);
}

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenProperIoctlsAreCalledAndUnmapSizeIsNonZero) {
    //GEM CREATE + SET_TILING + WAIT + CLOSE
    mock->ioctl_expected = 4;
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D; // tiled
    imgDesc.image_width = 512;
    imgDesc.image_height = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.imgDesc = &imgDesc;
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    auto queryGmm = MockGmm::queryImgParams(imgInfo);
    auto imageGraphicsAllocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    queryGmm.release();

    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    EXPECT_EQ(imgInfo.size, drmAllocation->getBO()->peekUnmapSize());
    EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imgInfo.size, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_Y;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(imgInfo.rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    memoryManager->freeGraphicsMemory(imageGraphicsAllocation);
    EXPECT_EQ(1, mmapMockCallCount);
    EXPECT_EQ(1, munmapMockCallCount);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    //GEM CREATE + SET_TILING + WAIT + CLOSE
    mock->ioctl_expected = 4;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    auto imageSize = drmAllocation->getUnderlyingBufferSize();
    auto rowPitch = dstImage->getImageDesc().image_row_pitch;

    EXPECT_EQ(imageSize, drmAllocation->getBO()->peekUnmapSize());

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imageSize, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_Y;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedAndAllocationFailsThenReturnNullptr) {
    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    InjectedFunction method = [&](size_t failureIndex) {
        cl_mem_flags flags = CL_MEM_WRITE_ONLY;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
        if (nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, dstImage.get());
        } else {
            EXPECT_EQ(nullptr, dstImage.get());
        }
    };

    injectFailures(method);
    mock->reset();
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    //GEM CREATE + SET_TILING + WAIT + CLOSE
    mock->ioctl_expected = 4;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    auto imageSize = drmAllocation->getUnderlyingBufferSize();
    auto rowPitch = dstImage->getImageDesc().image_row_pitch;

    EXPECT_EQ(imageSize, drmAllocation->getBO()->peekUnmapSize());

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imageSize, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_Y;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    //USERPTR + WAIT + CLOSE
    mock->ioctl_expected = 3;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    char data[64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);

    EXPECT_EQ(0u, drmAllocation->getBO()->peekUnmapSize());

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_NONE;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhen1DarrayImageIsBeingCreatedFromHostPtrThenTilingIsNotCalled) {
    //USERPTR + WAIT + CLOSE
    mock->ioctl_expected = 3;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);

    EXPECT_EQ(0u, drmAllocation->getBO()->peekUnmapSize());

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_NONE;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledThenGraphicsAllocationIsReturned) {
    osHandle handle = 1u;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE + WAIT + CLOSE
    mock->ioctl_expected = 3;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_EQ(bo->peekUnmapSize(), size);
    EXPECT_NE(nullptr, bo->peekAddress());
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenAllocationFailsThenReturnNullPtr) {
    osHandle handle = 1u;

    InjectedFunction method = [this, &handle](size_t failureIndex) {
        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
        if (nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, graphicsAllocation);
            memoryManager->freeGraphicsMemory(graphicsAllocation);
        } else {
            EXPECT_EQ(nullptr, graphicsAllocation);
        }
    };

    injectFailures(method);
    mock->reset();
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndThreeOsHandlesWhenReuseCreatesAreCalledThenGraphicsAllocationsAreReturned) {
    osHandle handles[] = {1u, 2u, 3u};
    //3xDRM_IOCTL_PRIME_FD_TO_HANDLE + 3xWAIT + 2xCLOSE
    mock->ioctl_expected = 8;
    size_t size = 4096u;
    GraphicsAllocation *graphicsAllocations[3];
    DrmAllocation *drmAllocation;
    BufferObject *bo;
    unsigned int expectedRefCount;

    this->mock->outputHandle = 2u;

    for (unsigned int i = 0; i < 3; ++i) {
        expectedRefCount = i < 2 ? i + 1 : 1;
        if (i == 2)
            this->mock->outputHandle = 3u;

        graphicsAllocations[i] = memoryManager->createGraphicsAllocationFromSharedHandle(handles[i], false, true);
        //Clang-tidy false positive WA
        if (graphicsAllocations[i] == nullptr) {
            ASSERT_FALSE(true);
            continue;
        }
        ASSERT_NE(nullptr, graphicsAllocations[i]);

        EXPECT_NE(nullptr, graphicsAllocations[i]->getUnderlyingBuffer());
        EXPECT_EQ(size, graphicsAllocations[i]->getUnderlyingBufferSize());
        EXPECT_EQ(this->mock->inputFd, (int)handles[i]);
        EXPECT_EQ(this->mock->setTilingHandle, 0u);

        drmAllocation = static_cast<DrmAllocation *>(graphicsAllocations[i]);
        bo = drmAllocation->getBO();
        EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
        EXPECT_EQ(bo->peekUnmapSize(), size);
        EXPECT_NE(nullptr, bo->peekAddress());
        EXPECT_TRUE(bo->peekIsAllocated());
        EXPECT_EQ(expectedRefCount, bo->getRefCount());
        EXPECT_EQ(size, bo->peekSize());

        EXPECT_EQ(handles[i], graphicsAllocations[i]->peekSharedHandle());
    }

    for (const auto &it : graphicsAllocations) {
        //Clang-tidy false positive WA
        if (it != nullptr)
            memoryManager->freeGraphicsMemory(it);
    }
}

TEST_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleAndBitnessRequiredIsCreatedThenItIs32BitAllocation) {
    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE + WAIT + CLOSE
    mock->ioctl_expected = 3;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, true);
    auto drmAllocation = (DrmAllocation *)graphicsAllocation;
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    EXPECT_EQ(BIT32_ALLOCATOR_EXTERNAL, drmAllocation->getBO()->peekAllocationType());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0, mmapMockCallCount);
    EXPECT_EQ(0, munmapMockCallCount);
}

TEST_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDoesntRequireBitnessThenItIsNot32BitAllocation) {
    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE + WAIT + CLOSE
    mock->ioctl_expected = 3;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
    auto drmAllocation = (DrmAllocation *)graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(1, mmapMockCallCount);
    EXPECT_EQ(1, munmapMockCallCount);
}

TEST_F(DrmMemoryManagerTest, givenNon32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDRequireBitnessThenItIsNot32BitAllocation) {
    memoryManager->setForce32BitAllocations(false);
    osHandle handle = 1u;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE + WAIT + CLOSE
    mock->ioctl_expected = 3;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, true);
    auto drmAllocation = (DrmAllocation *)graphicsAllocation;
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(1, mmapMockCallCount);
    EXPECT_EQ(1, munmapMockCallCount);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromNTHandle((void *)1);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockCalledThenDoNothing) {
    mock->ioctl_expected = 3;
    auto allocation = memoryManager->allocateGraphicsMemory(1, 1);
    ASSERT_NE(nullptr, allocation);

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(nullptr, ptr);
    memoryManager->unlockResource(allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    mock->ioctl_expected = 3;
    auto gmm = Gmm::create(nullptr, 123, false);
    auto allocation = memoryManager->allocateGraphicsMemory(123, 123);
    allocation->gmm = gmm;

    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager->mapAuxGpuVA(allocation));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, given32BitAllocatorWithHeapAllocatorWhenLargerFragmentIsReusedThenOnlyUnmapSizeIsLargerWhileSizeStaysTheSame) {
    DebugManagerStateRestore dbgFlagsKeeper;
    //USERPTR + WAIT + CLOSE
    mock->ioctl_expected = 3;

    DebugManager.flags.UseNewHeapAllocator.set(true);
    memoryManager->setForce32BitAllocations(true);

    size_t allocationSize = 4 * MemoryConstants::pageSize;
    auto ptr = memoryManager->allocator32Bit->allocate(allocationSize);
    size_t smallAllocationSize = MemoryConstants::pageSize;
    memoryManager->allocator32Bit->allocate(smallAllocationSize);

    //now free first allocation , this will move it to chunks
    memoryManager->allocator32Bit->free(ptr, allocationSize);

    //now ask for 3 pages, this will give ptr from chunks
    size_t pages3size = 3 * MemoryConstants::pageSize;

    void *host_ptr = (void *)0x1000;
    DrmAllocation *graphicsAlloaction = memoryManager->allocate32BitGraphicsMemory(pages3size, host_ptr, MemoryType::EXTERNAL_ALLOCATION);

    auto bo = graphicsAlloaction->getBO();
    EXPECT_EQ(allocationSize, bo->peekUnmapSize());
    EXPECT_EQ(pages3size, bo->peekSize());
    EXPECT_EQ((uint64_t)(uintptr_t)ptr, graphicsAlloaction->getGpuAddress());

    memoryManager->freeGraphicsMemory(graphicsAlloaction);
}

TEST_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * 1024;
    lseekReturn = realSize;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE + WAIT + CLOSE
    mock->ioctl_expected = 3;
    osHandle sharedHandle = 1u;

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(realSize, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)sharedHandle);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_EQ(bo->peekUnmapSize(), realSize);
    EXPECT_NE(nullptr, bo->peekAddress());
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(realSize, bo->peekSize());
    EXPECT_EQ(1, lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}
TEST_F(DrmMemoryManagerTest, givenMemoryManagerSupportingVirutalPaddingWhenItIsRequiredThenNewGraphicsAllocationIsCreated) {
    //USERPTR * 3  + 3 * WAIT + 3* CLOSE
    mock->ioctl_expected = 9;
    //first let's create normal buffer
    auto bufferSize = MemoryConstants::pageSize;
    auto buffer = memoryManager->allocateGraphicsMemory(bufferSize, MemoryConstants::pageSize);

    //buffer should have size 16
    EXPECT_EQ(bufferSize, buffer->getUnderlyingBufferSize());

    EXPECT_EQ(nullptr, memoryManager->peekPaddingAllocation());

    auto bufferWithPaddingSize = 8192u;
    auto paddedAllocation = memoryManager->createGraphicsAllocationWithPadding(buffer, 8192u);
    EXPECT_NE(nullptr, paddedAllocation);

    EXPECT_NE(0u, paddedAllocation->getGpuAddress());
    EXPECT_NE(0u, paddedAllocation->getGpuAddressToPatch());
    EXPECT_NE(buffer->getGpuAddress(), paddedAllocation->getGpuAddress());
    EXPECT_NE(buffer->getGpuAddressToPatch(), paddedAllocation->getGpuAddressToPatch());
    EXPECT_EQ(buffer->getUnderlyingBuffer(), paddedAllocation->getUnderlyingBuffer());

    EXPECT_EQ(bufferWithPaddingSize, paddedAllocation->getUnderlyingBufferSize());
    EXPECT_FALSE(paddedAllocation->isCoherent());
    EXPECT_EQ(0u, paddedAllocation->fragmentsStorage.fragmentCount);

    auto bufferbo = ((DrmAllocation *)buffer)->getBO();
    auto bo = ((DrmAllocation *)paddedAllocation)->getBO();
    EXPECT_NE(nullptr, bo);

    EXPECT_EQ(bufferWithPaddingSize, bo->peekUnmapSize());
    EXPECT_EQ(MMAP_ALLOCATOR, bo->peekAllocationType());

    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_NE(bufferbo->peekHandle(), bo->peekHandle());

    memoryManager->freeGraphicsMemory(paddedAllocation);
    memoryManager->freeGraphicsMemory(buffer);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected = 3;
    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = (DrmAllocation *)memoryManager->createInternalGraphicsAllocation(ptr, bufferSize);
    ASSERT_NE(nullptr, drmAllocation);

    auto internalAllocator = memoryManager->getDrmInternal32BitAllocator();

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation);

    auto gpuPtr = drmAllocation->getGpuAddress();

    auto heapBase = internalAllocator->getBase();
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->gpuBaseAddress, heapBase);

    auto bo = drmAllocation->getBO();
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_INTERNAL);
    EXPECT_EQ(bo->peekUnmapSize(), bufferSize);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected = 3;
    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = (void *)0x100000;
    auto drmAllocation = (DrmAllocation *)memoryManager->createInternalGraphicsAllocation(ptr, bufferSize);
    ASSERT_NE(nullptr, drmAllocation);

    auto internalAllocator = memoryManager->getDrmInternal32BitAllocator();

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(ptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation);

    auto gpuPtr = drmAllocation->getGpuAddress();

    auto heapBase = internalAllocator->getBase();
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->gpuBaseAddress, heapBase);

    auto bo = drmAllocation->getBO();
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_INTERNAL);
    EXPECT_EQ(bo->peekUnmapSize(), bufferSize);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerSupportingVirutalPaddingWhenAllocUserptrFailsThenReturnsNullptr) {
    mock->ioctl_expected = 7;

    this->ioctlResExt = {2, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    //first let's create normal buffer
    auto bufferSize = MemoryConstants::pageSize;
    auto buffer = memoryManager->allocateGraphicsMemory(bufferSize, MemoryConstants::pageSize);

    //buffer should have size 16
    EXPECT_EQ(bufferSize, buffer->getUnderlyingBufferSize());

    EXPECT_EQ(nullptr, memoryManager->peekPaddingAllocation());

    auto bufferWithPaddingSize = 8192u;
    auto paddedAllocation = memoryManager->createGraphicsAllocationWithPadding(buffer, bufferWithPaddingSize);
    EXPECT_EQ(nullptr, paddedAllocation);

    memoryManager->freeGraphicsMemory(buffer);
}

TEST_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenAskedForVirtualPaddingSupportThenTrueIsReturned) {
    EXPECT_TRUE(memoryManager->peekVirtualPaddingSupport());
}

TEST_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, memoryManager->getAlignedMallocRestrictions());
}

TEST(Allocator32BitUsingHeapAllocator, given32BitAllocatorWhenMMapFailsThenNullptrIsReturned) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(true);

    mockAllocator32Bit::resetState();
    failMmap = true;
    mockAllocator32Bit::OsInternalsPublic *osInternals = mockAllocator32Bit::createOsInternals();
    osInternals->mmapFunction = MockMmap;
    osInternals->munmapFunction = MockMunmap;
    mockAllocator32Bit *mock32BitAllocator = new mockAllocator32Bit(osInternals);
    size_t size = 100u;
    auto ptr = mock32BitAllocator->allocate(size);
    EXPECT_EQ(nullptr, ptr);
    EXPECT_EQ(2u, mmapCallCount);
    delete mock32BitAllocator;
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(Allocator32BitUsingHeapAllocator, given32BitAllocatorWhenFirstMMapFailsThenSecondIsCalledWithSmallerSize) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(true);

    mockAllocator32Bit::resetState();
    mmapFailCount = 1u;
    mockAllocator32Bit::OsInternalsPublic *osInternals = mockAllocator32Bit::createOsInternals();
    osInternals->mmapFunction = MockMmap;
    osInternals->munmapFunction = MockMunmap;
    mockAllocator32Bit *mock32BitAllocator = new mockAllocator32Bit(osInternals);
    size_t size = 100u;
    auto ptr = mock32BitAllocator->allocate(size);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(2u, mmapCallCount);

    EXPECT_NE(nullptr, osInternals->heapBasePtr);
    EXPECT_NE(0u, osInternals->heapSize);

    delete mock32BitAllocator;
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, allocateReturnsPointer) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    size_t size = 100u;
    auto ptr = mock32BitAllocator.allocate(size);
    EXPECT_NE(0u, (uintptr_t)ptr);
    EXPECT_EQ(1u, mmapCallCount);
    mock32BitAllocator.free(ptr, size);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, freeMapFailedPointer) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    size_t size = 100u;
    int result = mock32BitAllocator.free(MAP_FAILED, size);
    EXPECT_EQ(0, result);

    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, freeNullPtrPointer) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    uint32_t size = 100u;
    int result = mock32BitAllocator.free(nullptr, size);
    EXPECT_EQ(0, result);

    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, freeLowerRangeAfterTwoMmapFails) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    mmapFailCount = 2;
    size_t size = 100u;
    auto ptr = mock32BitAllocator.allocate(size);
    EXPECT_EQ(3u, mmapCallCount);
    int result = mock32BitAllocator.free(ptr, size);
    EXPECT_EQ(0, result);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32BitAllocatorWhenMMapFailsThenUpperHeapIsBrowsedForAllocations) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    size_t size = 100u;
    auto ptr = mock32BitAllocator.allocate(size);
    EXPECT_EQ(maxMmap32BitAddress, (uintptr_t)ptr);
    EXPECT_EQ(2u, mmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32BitAllocatorWith32AndUpperHeapsExhaustedThenPointerFromLowerHeapIsReturned) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    failUpperRange = true;
    size_t size = 100u;
    auto ptr = mock32BitAllocator.allocate(size);
    EXPECT_EQ(lowerRangeStart, (uintptr_t)ptr);
    EXPECT_EQ(3u, mmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32bitRegionExhaustedWhenTwoAllocationsAreCreatedThenSecondIsAfterFirst) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    size_t size = 100u;
    auto ptr = (uintptr_t)mock32BitAllocator.allocate(size);
    EXPECT_EQ(maxMmap32BitAddress, ptr);

    auto alignedSize = alignUp(size, MemoryConstants::pageSize);
    auto ptr2 = (uintptr_t)mock32BitAllocator.allocate(size);
    EXPECT_EQ(maxMmap32BitAddress + alignedSize, ptr2);

    EXPECT_EQ(4u, mmapCallCount);
    mock32BitAllocator.free((void *)ptr2, size);

    auto getInternals = mock32BitAllocator.getosInternal();
    EXPECT_EQ(ptr2, getInternals->upperRangeAddress);

    mock32BitAllocator.free((void *)ptr, size);
    EXPECT_EQ(ptr, getInternals->upperRangeAddress);

    EXPECT_EQ(2u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32bitRegionAndUpperRegionExhaustedWhenTwoAllocationsAreCreatedThenSecondIsAfterFirst) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    failUpperRange = true;
    size_t size = 100u;
    auto ptr = (uintptr_t)mock32BitAllocator.allocate(size);
    EXPECT_EQ(lowerRangeStart, ptr);

    auto alignedSize = alignUp(size, MemoryConstants::pageSize);
    auto ptr2 = (uintptr_t)mock32BitAllocator.allocate(size);
    EXPECT_EQ(lowerRangeStart + alignedSize, ptr2);

    EXPECT_EQ(6u, mmapCallCount);
    mock32BitAllocator.free((void *)ptr2, size);

    auto getInternals = mock32BitAllocator.getosInternal();
    EXPECT_EQ(ptr2, getInternals->lowerRangeAddress);

    mock32BitAllocator.free((void *)ptr, size);
    EXPECT_EQ(ptr, getInternals->lowerRangeAddress);

    EXPECT_EQ(4u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32bitAllocatorWithAllHeapsExhaustedWhenAskedForAllocationThenNullptrIsReturned) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    failLowerRanger = true;
    failUpperRange = true;
    size_t size = 100u;

    auto ptr = mock32BitAllocator.allocate(size);

    EXPECT_EQ(nullptr, ptr);

    EXPECT_EQ(3u, mmapCallCount);
    EXPECT_EQ(2u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, given32bitAllocatorWithUpperHeapCloseToFullWhenAskedForAllocationThenAllocationFromLowerHeapIsReturned) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    fail32BitMmap = true;
    size_t size = 3 * 1024 * 1024 * 1029u;

    auto ptr = mock32BitAllocator.allocate(size);

    EXPECT_EQ(lowerRangeHeapStart, (uintptr_t)ptr);
    EXPECT_EQ(3u, mmapCallCount);
    EXPECT_EQ(1u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, givenMapFailedAsInputToFreeFunctionWhenItIsCalledThenUnmapIsNotCalled) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    mock32BitAllocator.free(MAP_FAILED, 100u);
    EXPECT_EQ(0u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, givenNullptrAsInputToFreeFunctionWhenItIsCalledThenUnmapIsNotCalled) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    mock32BitAllocator.free(nullptr, 100u);
    EXPECT_EQ(0u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

#include <chrono>
#include <iostream>

TEST(MmapFlags, givenVariousMmapParametersGetTimeDeltaForTheOperation) {
    //disabling this test in CI.
    return;
    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::nanoseconds ns;
    typedef std::chrono::duration<double> fsec;

    std::vector<void *> pointersForFree;
    //allocate 4GB.
    auto size = 4 * 1024 * 1024 * 1023u;
    unsigned int maxTime = 0;
    unsigned int minTime = -1;
    unsigned int totalTime = 0;

    auto iterCount = 10;

    for (int i = 0; i < iterCount; i++) {
        auto t0 = Time::now();
        auto gpuRange = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        auto t1 = Time::now();
        pointersForFree.push_back(gpuRange);
        fsec fs = t1 - t0;
        ns d = std::chrono::duration_cast<ns>(fs);
        unsigned int duration = (unsigned int)d.count();
        totalTime += duration;
        minTime = std::min(duration, minTime);
        maxTime = std::max(duration, maxTime);
    }

    std::cout << "\n"
              << "min = " << minTime << "\nmax = " << maxTime << "\naverage = " << totalTime / iterCount << std::endl;
    for (auto &ptr : pointersForFree) {
        auto t0 = Time::now();
        munmap(ptr, size);
        auto t1 = Time::now();
        fsec fs = t1 - t0;
        ns d = std::chrono::duration_cast<ns>(fs);
        unsigned int duration = (unsigned int)d.count();
        std::cout << "\nfreeing ptr " << ptr << " of size " << size << "time " << duration;
    }
}

TEST(DrmMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(DrmMemoryManager, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(DrmMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}
