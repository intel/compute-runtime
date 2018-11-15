/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/event/event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/32bit_memory.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/utilities/tag_allocator.h"

#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_32bitAllocator.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/linux/mock_drm_command_stream_receiver.h"
#include "unit_tests/mocks/linux/mock_drm_memory_manager.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "test.h"
#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>
#include <memory>

using namespace OCLRT;

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    TestedDrmMemoryManager *memoryManager = nullptr;
    DrmMockCustom *mock;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        this->mock = new DrmMockCustom;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->incRefInternal();
        memoryManager = new (std::nothrow) TestedDrmMemoryManager(this->mock, *executionEnvironment);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
    }

    void TearDown() override {
        delete memoryManager;
        executionEnvironment->decRefInternal();

        this->mock->testIoctls();

        delete this->mock;
        this->mock = nullptr;
        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment *executionEnvironment;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation : public MemoryManagementFixture {
  public:
    TestedDrmMemoryManager *memoryManager = nullptr;
    DrmMockCustom *mock;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        this->mock = new DrmMockCustom;
        memoryManager = new (std::nothrow) TestedDrmMemoryManager(this->mock, executionEnvironment);
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
    }

    void TearDown() override {
        delete memoryManager;
        delete this->mock;
        this->mock = nullptr;
        MemoryManagementFixture::TearDown();
    }

  protected:
    ExecutionEnvironment executionEnvironment;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
};

typedef Test<DrmMemoryManagerFixture> DrmMemoryManagerTest;
typedef Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation> DrmMemoryManagerWithExplicitExpectationsTest;

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryMangerWhenItIsCreatedThenItContainsInternal32BitAllocator) {
    EXPECT_NE(nullptr, memoryManager->getDrmInternal32BitAllocator());
}

TEST_F(DrmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    DrmAllocation gfxAllocation(nullptr, cpuPtr, size, MemoryPool::MemoryNull);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_EQ(fragment->osInternalStorage->bo, gfxAllocation.getBO());
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenforcePinAllowedWhenMemoryManagerIsCreatedThenPinBbIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, pinBBisCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, givenNotAllowedForcePinWhenMemoryManagerIsCreatedThenPinBBIsNotCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, false, false, *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, pinBBnotCreatedWhenIoctlFailed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;
    auto memoryManager = new (std::nothrow) TestedDrmMemoryManager(this->mock, true, false, *executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
    delete memoryManager;
}

TEST_F(DrmMemoryManagerTest, pinAfterAllocateWhenAskedAndAllowedAndBigAllocation) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    auto alloc = memoryManager->allocateGraphicsMemory(10 * 1014 * 1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedAndAllowedButSmallAllocation) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning
    auto alloc = memoryManager->allocateGraphicsMemory(4 * 1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    auto alloc = memoryManager->allocateGraphicsMemory(1024, 1024, false, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, false, false, *executionEnvironment);

    auto alloc = memoryManager->allocateGraphicsMemory(1024, 1024, true, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

// ---- HostPtr
TEST_F(DrmMemoryManagerTest, pinAfterAllocateWhenAskedAndAllowedAndBigAllocationHostPtr) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    size_t size = 10 * 1024 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, givenSmallAllocationHostPtrAllocationWhenForcePinIsTrueThenBufferObjectIsNotPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning
    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowedHostPtr) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowedHostPtr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(this->mock, false, false, *executionEnvironment);

    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, unreference) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul, true);
    ASSERT_NE(nullptr, bo);
    memoryManager->unreference(bo);
}

TEST_F(DrmMemoryManagerTest, UnreferenceNullPtr) {
    memoryManager->unreference(nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerModeInactiveThenGemCloseWorkerIsNotCreated) {
    DrmMemoryManager drmMemoryManger(this->mock, gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    EXPECT_EQ(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerActiveThenGemCloseWorkerIsCreated) {
    DrmMemoryManager drmMemoryManger(this->mock, gemCloseWorkerMode::gemCloseWorkerActive, false, false, executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerTest, AllocateThenFree) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(1024));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, AllocateNewFail) {
    mock->ioctl_expected.total = -1; //don't care

    InjectedFunction method = [this](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemory(1024);

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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemory(static_cast<size_t>(0));
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate3Bytes) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemory(3);
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, AllocateUserptrFail) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    auto ptr = memoryManager->allocateGraphicsMemory(3);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(DrmMemoryManagerTest, FreeNullPtr) {
    memoryManager->freeGraphicsMemory(nullptr);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    auto alloc = memoryManager->allocateGraphicsMemory(1024, ptrT);
    EXPECT_EQ(nullptr, alloc);

    ::alignedFree(ptrT);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhengetSystemSharedMemoryIsCalledThenContextGetParamIsCalled) {
    mock->getContextParamRetValue = 16 * MemoryConstants::gigaByte;
    uint64_t mem = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected.contextGetParam = 1;
    EXPECT_EQ(mock->recordedGetContextParam.param, static_cast<__u64>(I915_CONTEXT_PARAM_GTT_SIZE));
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
    mock->getContextParamRetValue = gpuMemorySize;

    uint64_t systemSharedMemorySize = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected.contextGetParam = 1;

    EXPECT_EQ(gpuMemorySize, systemSharedMemorySize);
    mock->testIoctls();

    // gpuMemSize > hostMemSize
    gpuMemorySize = hostMemorySize + 1u;
    mock->getContextParamRetValue = gpuMemorySize;
    systemSharedMemorySize = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected.contextGetParam = 2;
    EXPECT_EQ(hostMemorySize, systemSharedMemorySize);
    mock->testIoctls();
}

TEST_F(DrmMemoryManagerTest, BoWaitFailure) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul, true);
    ASSERT_NE(nullptr, bo);
    mock->ioctl_res = -EIO;
    EXPECT_THROW(bo->wait(-1), std::exception);
    mock->ioctl_res = 1;

    memoryManager->unreference(bo);
}

TEST_F(DrmMemoryManagerTest, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    memoryManager->populateOsHandles(storage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesReturnsInvalidHostPointerError) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;

    MemoryManager::AllocationStatus result = testedMemoryManager->populateOsHandles(storage);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    testedMemoryManager->cleanOsHandles(storage);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenPinningFailWithErrorDifferentThanEfaultThenPopulateOsHandlesReturnsError) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = ENOMEM;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;

    MemoryManager::AllocationStatus result = testedMemoryManager->populateOsHandles(storage);

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    testedMemoryManager->cleanOsHandles(storage);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, GivenNoInputsWhenOsHandleIsCreatedThenAllBoHandlesAreInitializedAsNullPtrs) {
    OsHandle boHandle;
    EXPECT_EQ(nullptr, boHandle.bo);

    std::unique_ptr<OsHandle> boHandle2(new OsHandle);
    EXPECT_EQ(nullptr, boHandle2->bo);
}

TEST_F(DrmMemoryManagerTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;
    auto ptr = reinterpret_cast<void *>(0x1000);
    auto ptr2 = reinterpret_cast<void *>(0x1001);
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
    auto allocation = memoryManager->allocateGraphicsMemory64kb(65536, 65536, false, false);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    mock->ioctl_expected.gemUserptr = 3;
    mock->ioctl_expected.gemWait = 3;
    mock->ioctl_expected.gemClose = 3;

    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(size, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekSize());
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekAddress());
        EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekIsAllocated());
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, testProfilingAllocatorCleanup) {
    MemoryManager *memMngr = memoryManager;
    TagAllocator<HwTimeStamps> *allocator = memMngr->obtainEventTsAllocator(2);
    EXPECT_NE(nullptr, allocator);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationThen32BitDrmAllocationIsBeingReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);
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
    mock->ioctl_expected.reset();

    EXPECT_EQ(nullptr, memoryManager->allocator32Bit.get());
    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    auto currentAllocator = memoryManager->allocator32Bit.get();
    memoryManager->setForce32BitAllocations(true);
    EXPECT_EQ(memoryManager->allocator32Bit.get(), currentAllocator);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhensetForce32BitAllocationsIsCalledWithFalseThenAllocatorIsNotDeleted) {
    mock->ioctl_expected.reset();

    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    memoryManager->setForce32BitAllocations(false);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
}

TEST_F(DrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferAllocationThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    if (DebugManager.flags.UseNewHeapAllocator.get()) {
        mock->ioctl_expected.gemUserptr = 1;
        mock->ioctl_expected.gemWait = 1;
        mock->ioctl_expected.gemClose = 1;
    } else {
        mock->ioctl_expected.gemUserptr = 2;
        mock->ioctl_expected.gemWait = 2;
        mock->ioctl_expected.gemClose = 2;
    }

    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context;
    memoryManager->setForce32BitAllocations(true);
    context.setMemoryManager(memoryManager);

    auto size = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x1000);
    auto ptrOffset = MemoryConstants::cacheLineSize;
    uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        reinterpret_cast<void *>(offsetedPtr),
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();
    auto drmAllocation = static_cast<DrmAllocation *>(buffer->getGraphicsAllocation());

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
    EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), reinterpret_cast<void *>(offsetedPtr));

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
            mock->ioctl_expected.total = -1;
        } else {
            mock->ioctl_expected.gemUserptr = 1;
            mock->ioctl_expected.gemWait = 1;
            mock->ioctl_expected.gemClose = 1;

            DebugManager.flags.Force32bitAddressing.set(true);
            MockContext context;
            memoryManager->setForce32BitAllocations(true);
            context.setMemoryManager(memoryManager);

            auto size = MemoryConstants::pageSize;
            void *ptr = reinterpret_cast<void *>(0x100000000000);
            auto ptrOffset = MemoryConstants::cacheLineSize;
            uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
            auto retVal = CL_SUCCESS;

            auto buffer = Buffer::create(
                &context,
                CL_MEM_USE_HOST_PTR,
                size,
                reinterpret_cast<void *>(offsetedPtr),
                retVal);
            EXPECT_EQ(CL_SUCCESS, retVal);

            EXPECT_TRUE(buffer->isMemObjZeroCopy());
            auto bufferAddress = buffer->getGraphicsAllocation()->getGpuAddress();

            uintptr_t address64bit = (uintptr_t)bufferAddress;

            if (is32BitOsAllocatorAvailable) {
                auto baseAddress = buffer->getGraphicsAllocation()->gpuBaseAddress;
                EXPECT_LT(address64bit - baseAddress, max32BitAddress);
            }

            auto drmAllocation = static_cast<DrmAllocation *>(buffer->getGraphicsAllocation());

            EXPECT_TRUE(drmAllocation->is32BitAllocation);

            auto allocationCpuPtr = (uintptr_t)drmAllocation->getUnderlyingBuffer();
            auto allocationPageOffset = allocationCpuPtr - alignDown(allocationCpuPtr, MemoryConstants::pageSize);
            auto bufferObject = drmAllocation->getBO();

            EXPECT_NE(0u, bufferObject->peekUnmapSize());

            if (DebugManager.flags.UseNewHeapAllocator.get() == false) {
                EXPECT_NE(drmAllocation->getUnderlyingBuffer(), reinterpret_cast<void *>(offsetedPtr));
            }

            EXPECT_EQ(allocationPageOffset, ptrOffset);
            EXPECT_FALSE(bufferObject->peekIsAllocated());

            auto totalAllocationSize = alignSizeWholePage(reinterpret_cast<void *>(offsetedPtr), size);
            EXPECT_EQ(totalAllocationSize, bufferObject->peekUnmapSize());

            auto boAddress = bufferObject->peekAddress();
            EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

            delete buffer;
            DebugManager.flags.Force32bitAddressing.set(false);
        }
    }
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWithHostPtrAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    void *host_ptr = reinterpret_cast<void *>(0x1000);
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, host_ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, GivenSizeAbove2GBWhenUseHostPtrAndAllocHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.total = -1;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context;
    memoryManager->setForce32BitAllocations(true);
    context.setMemoryManager(memoryManager);

    size_t size = 2 * 1024 * 1024 * 1024u;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        ptr,
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
    mock->ioctl_expected.total = -1;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context;
    memoryManager->setForce32BitAllocations(true);
    context.setMemoryManager(memoryManager);

    size_t size = 2 * 1024 * 1024 * 1024u;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
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
        ptr,
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

TEST_F(DrmMemoryManagerTest, Given32BitDeviceWithMemoryManagerWhenInternalHeapIsExhaustedAndNewAllocationsIsMadeThenNullIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.Force32bitAddressing.set(true);
    memoryManager->setForce32BitAllocations(true);
    std::unique_ptr<Device> pDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    auto allocator = memoryManager->getDrmInternal32BitAllocator();
    size_t size = getSizeToMap();
    auto alloc = allocator->allocate(size);
    EXPECT_NE(0llu, alloc);

    size_t allocationSize = 4 * 1024 * 1024 * 1024llu;
    auto graphicsAllocation = memoryManager->allocate32BitGraphicsMemory(allocationSize, nullptr, AllocationOrigin::INTERNAL_ALLOCATION);
    EXPECT_EQ(nullptr, graphicsAllocation);
    EXPECT_TRUE(pDevice->getDeviceInfo().force32BitAddressess);
}

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenProperIoctlsAreCalledAndUnmapSizeIsNonZero) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D; // tiled
    imgDesc.image_width = 512;
    imgDesc.image_height = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.imgDesc = &imgDesc;
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);
    auto imageGraphicsAllocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    queryGmm.release();

    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_NE(0u, imageGraphicsAllocation->getGpuAddress());
    EXPECT_EQ(nullptr, imageGraphicsAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(imageGraphicsAllocation->gmm->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    EXPECT_EQ(imgInfo.size, drmAllocation->getBO()->peekUnmapSize());

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imgInfo.size, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_Y;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(imgInfo.rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    memoryManager->freeGraphicsMemory(imageGraphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    MockContext context;
    context.setMemoryManager(memoryManager);

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

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    MockContext context;
    context.setMemoryManager(memoryManager);

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
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());
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

    cl_image_desc imageDesc = {};

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
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::unique_ptr<Device> device(MockDevice::createWithExecutionEnvironment<MockDevice>(*platformDevices, executionEnvironment, 0u));
    MockContext context(device.get());
    context.setMemoryManager(memoryManager);

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

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    MockContext context;
    context.setMemoryManager(memoryManager);

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

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;
    imageDesc.num_mip_levels = 1u;

    char data[64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());
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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

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
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
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
    mock->ioctl_expected.primeFdToHandle = 3;
    mock->ioctl_expected.gemWait = 3;
    mock->ioctl_expected.gemClose = 2;

    osHandle handles[] = {1u, 2u, 3u};
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

        graphicsAllocations[i] = memoryManager->createGraphicsAllocationFromSharedHandle(handles[i], false);
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
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    EXPECT_EQ(BIT32_ALLOCATOR_EXTERNAL, drmAllocation->getBO()->peekAllocationType());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0, mmapMockCallCount);
    EXPECT_EQ(0, munmapMockCallCount);
}

TEST_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDoesntRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenBufferFromSharedHandleIsCreatedThenItIsLimitedRangeAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, drmAllocation->getBO()->peekAllocationType());
    EXPECT_EQ(1, lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenNon32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(false);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, true);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation);
    EXPECT_EQ(1, lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenSharedHandleWhenAllocationIsCreatedAndIoctlPrimeFdToHandleFailsThenNullPtrIsReturned) {
    mock->ioctl_expected.primeFdToHandle = 1;
    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &this->ioctlResExt;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, false);
    EXPECT_EQ(nullptr, graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatShareTheSameBufferObjectWhenTheyAreMadeResidentThenOnlyOneBoIsPassedToExec) {
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);

    executionEnvironment->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->osInterface->get()->setDrm(mock);
    auto testedCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*executionEnvironment);
    executionEnvironment->commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(testedCsr));

    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);
    EXPECT_EQ(2u, testedCsr->getResidencyAllocations().size());

    OsContext osContext(executionEnvironment->osInterface.get(), 0u);
    testedCsr->processResidency(testedCsr->getResidencyAllocations(), osContext);

    EXPECT_EQ(1u, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatDoesnShareTheSameBufferObjectWhenTheyAreMadeResidentThenTwoBoIsPassedToExec) {
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);
    mock->outputHandle++;
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, false);

    executionEnvironment->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->osInterface->get()->setDrm(mock);
    auto testedCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*executionEnvironment);
    executionEnvironment->commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(testedCsr));

    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);
    EXPECT_EQ(2u, testedCsr->getResidencyAllocations().size());

    OsContext osContext(executionEnvironment->osInterface.get(), 0u);
    testedCsr->processResidency(testedCsr->getResidencyAllocations(), osContext);

    EXPECT_EQ(2u, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1));
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemSetDomain = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto allocation = memoryManager->allocateGraphicsMemory(1);
    ASSERT_NE(nullptr, allocation);

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithCpuPtrThenReturnCpuPtrAndSetCpuDomain) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemSetDomain = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto allocation = memoryManager->allocateGraphicsMemory(1);
    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(allocation->getUnderlyingBuffer(), ptr);

    //check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ(0u, mock->setDomainWriteDomain);

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithoutCpuPtrThenReturnLockedPtrAndSetCpuDomain) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemMmap = 1;
    mock->ioctl_expected.gemSetDomain = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 512;
    imgDesc.image_height = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.imgDesc = &imgDesc;
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);
    auto allocation = memoryManager->allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    queryGmm.release();
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    //check DRM_IOCTL_I915_GEM_MMAP input params
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->mmapHandle);
    EXPECT_EQ(0u, mock->mmapPad);
    EXPECT_EQ(0u, mock->mmapOffset);
    EXPECT_EQ(drmAllocation->getBO()->peekSize(), mock->mmapSize);
    EXPECT_EQ(0u, mock->mmapFlags);

    //check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ(0u, mock->setDomainWriteDomain);

    memoryManager->unlockResource(allocation);
    EXPECT_EQ(nullptr, drmAllocation->getBO()->peekLockedAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnNullAllocationThenReturnNullPtr) {
    GraphicsAllocation *allocation = nullptr;

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithoutBufferObjectThenReturnNullPtr) {
    DrmAllocation drmAllocation(nullptr, nullptr, 0u, 0u);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledButFailsOnIoctlMmapThenReturnNullPtr) {
    mock->ioctl_expected.gemMmap = 1;
    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    DrmMockCustom drmMock;
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 1, true) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(&bo, nullptr, 0u, 0u);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationWithoutBufferObjectThenReturnFalse) {
    DrmAllocation drmAllocation(nullptr, nullptr, 0u, 0u);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledButFailsOnIoctlSetDomainThenReturnFalse) {
    mock->ioctl_expected.gemSetDomain = 1;
    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    DrmMockCustom drmMock;
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 1, true) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(&bo, nullptr, 0u, 0u);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationThenReturnSetWriteDomain) {
    mock->ioctl_expected.gemSetDomain = 1;

    DrmMockCustom drmMock;
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 1, true) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(&bo, nullptr, 0u, 0u);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, true);
    EXPECT_TRUE(success);

    //check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    EXPECT_EQ((uint32_t)drmAllocation.getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainWriteDomain);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto gmm = new Gmm(nullptr, 123, false);
    auto allocation = memoryManager->allocateGraphicsMemory(123);
    allocation->gmm = gmm;

    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager->mapAuxGpuVA(allocation));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, given32BitAllocatorWithHeapAllocatorWhenLargerFragmentIsReusedThenOnlyUnmapSizeIsLargerWhileSizeStaysTheSame) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    DebugManagerStateRestore dbgFlagsKeeper;
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

    void *host_ptr = reinterpret_cast<void *>(0x1000);
    DrmAllocation *graphicsAlloaction = memoryManager->allocate32BitGraphicsMemory(pages3size, host_ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    auto bo = graphicsAlloaction->getBO();
    EXPECT_EQ(allocationSize, bo->peekUnmapSize());
    EXPECT_EQ(pages3size, bo->peekSize());
    EXPECT_EQ((uint64_t)(uintptr_t)ptr, graphicsAlloaction->getGpuAddress());

    memoryManager->freeGraphicsMemory(graphicsAlloaction);
}

TEST_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * 1024;
    lseekReturn = realSize;
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
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
    mock->ioctl_expected.gemUserptr = 3;
    mock->ioctl_expected.gemWait = 3;
    mock->ioctl_expected.gemClose = 3;
    //first let's create normal buffer
    auto bufferSize = MemoryConstants::pageSize;
    auto buffer = memoryManager->allocateGraphicsMemory(bufferSize);

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

    auto bufferbo = static_cast<DrmAllocation *>(buffer)->getBO();
    auto bo = static_cast<DrmAllocation *>(paddedAllocation)->getBO();
    EXPECT_NE(nullptr, bo);

    EXPECT_EQ(bufferWithPaddingSize, bo->peekUnmapSize());

    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_NE(bufferbo->peekHandle(), bo->peekHandle());

    memoryManager->freeGraphicsMemory(paddedAllocation);
    memoryManager->freeGraphicsMemory(buffer);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, AllocationOrigin::INTERNAL_ALLOCATION));
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
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x100000);
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, AllocationOrigin::INTERNAL_ALLOCATION));
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
    mock->ioctl_expected.gemUserptr = 3;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;
    this->ioctlResExt = {2, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    //first let's create normal buffer
    auto bufferSize = MemoryConstants::pageSize;
    auto buffer = memoryManager->allocateGraphicsMemory(bufferSize);

    //buffer should have size 16
    EXPECT_EQ(bufferSize, buffer->getUnderlyingBufferSize());

    EXPECT_EQ(nullptr, memoryManager->peekPaddingAllocation());

    auto bufferWithPaddingSize = 8192u;
    auto paddedAllocation = memoryManager->createGraphicsAllocationWithPadding(buffer, bufferWithPaddingSize);
    EXPECT_EQ(nullptr, paddedAllocation);

    memoryManager->freeGraphicsMemory(buffer);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryManagerWhenAskedForVirtualPaddingSupportThenTrueIsReturned) {
    EXPECT_TRUE(memoryManager->peekVirtualPaddingSupport());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenNullPtrIsReturned) {
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
    EXPECT_EQ(0llu, ptr);
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
    EXPECT_NE(0llu, ptr);
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
    int result = mock32BitAllocator.free(reinterpret_cast<uint64_t>(MAP_FAILED), size);
    EXPECT_EQ(0, result);

    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, freeNullPtrPointer) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    uint32_t size = 100u;
    int result = mock32BitAllocator.free(0llu, size);
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
    mock32BitAllocator.free(ptr2, size);

    auto getInternals = mock32BitAllocator.getosInternal();
    EXPECT_EQ(ptr2, getInternals->upperRangeAddress);

    mock32BitAllocator.free(ptr, size);
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
    mock32BitAllocator.free(ptr2, size);

    auto getInternals = mock32BitAllocator.getosInternal();
    EXPECT_EQ(ptr2, getInternals->lowerRangeAddress);

    mock32BitAllocator.free(ptr, size);
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

    EXPECT_EQ(0llu, ptr);

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
    mock32BitAllocator.free(reinterpret_cast<uint64_t>(MAP_FAILED), 100u);
    EXPECT_EQ(0u, unmapCallCount);
    DebugManager.flags.UseNewHeapAllocator.set(flagToRestore);
}

TEST(DrmAllocator32Bit, givenNullptrAsInputToFreeFunctionWhenItIsCalledThenUnmapIsNotCalled) {
    bool flagToRestore = DebugManager.flags.UseNewHeapAllocator.get();
    DebugManager.flags.UseNewHeapAllocator.set(false);

    mockAllocator32Bit mock32BitAllocator;
    mock32BitAllocator.free(0llu, 100u);
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
    ExecutionEnvironment executionEnvironment;
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(DrmMemoryManager, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    ExecutionEnvironment executionEnvironment;
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(DrmMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    ExecutionEnvironment executionEnvironment;
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    DrmMemoryManager memoryManager(Drm::get(0), gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(DrmMemoryManager, givenDefaultDrmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenInternalHeapBaseIsReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), true, true, executionEnvironment));
    auto internalAllocator = memoryManager->getDrmInternal32BitAllocator();
    auto heapBase = internalAllocator->getBase();
    EXPECT_EQ(heapBase, memoryManager->getInternalHeapBaseAddress());
}

TEST(DrmMemoryManager, givenMemoryManagerWithEnabledHostMemoryValidationWhenFeatureIsQueriedThenTrueIsReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_TRUE(memoryManager->isValidateHostMemoryEnabled());
}

TEST(DrmMemoryManager, givenMemoryManagerWithDisabledHostMemoryValidationWhenFeatureIsQueriedThenFalseIsReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, false, executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_FALSE(memoryManager->isValidateHostMemoryEnabled());
}

TEST(DrmMemoryManager, givenEnabledHostMemoryValidationWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->getPinBB());
}

TEST(DrmMemoryManager, givenEnabledHostMemoryValidationAndForcePinWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), true, true, executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->getPinBB());
}

TEST(DrmMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    auto size = 4096u;

    auto allocation = memoryManager->allocateGraphicsMemory(size);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);

    allocation = memoryManager->allocateGraphicsMemory(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    memoryManager->setForce32BitAllocations(true);

    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMIsCalledThen4KBGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryForSVM(size, false);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST(DrmMemoryManager, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    auto osHandle = 1u;
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManager, DISABLED_givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(size, MemoryConstants::preferredAlignment, false, false);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManager, DISABLED_givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMIsCalledThenMemoryPoolIsSystem64KBPages) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, true, executionEnvironment));
    auto size = MemoryConstants::pageSize;

    auto svmAllocation = memoryManager->allocateGraphicsMemoryForSVM(size, false);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(svmAllocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPinBBAllocationFailsThenUnrecoverableIsCalled) {
    this->mock->ioctl_res = -1;
    this->mock->ioctl_expected.gemUserptr = 1;
    EXPECT_THROW(
        {
            std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
            EXPECT_NE(nullptr, testedMemoryManager.get());
        },
        std::exception);
    this->mock->ioctl_res = 0;
    this->mock->testIoctls();
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledThenHostMemoryIsValidated) {

    std::unique_ptr<DrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager.get());
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1; // for pinning - host memory validation

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;
    auto result = testedMemoryManager->populateOsHandles(handleStorage);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;

    testedMemoryManager->cleanOsHandles(handleStorage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledWithFirstFragmentAlreadyAllocatedThenNewBosAreValidated) {

    class PinBufferObject : public BufferObject {
      public:
        PinBufferObject(Drm *drm) : BufferObject(drm, 1, true) {
        }

        int pin(BufferObject *boToPin[], size_t numberOfBos) override {
            for (size_t i = 0; i < numberOfBos; i++) {
                pinnedBoArray[i] = boToPin[i];
            }
            numberOfBosPinned = numberOfBos;
            return 0;
        }
        BufferObject *pinnedBoArray[5];
        size_t numberOfBosPinned;
    };

    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager.get());
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    PinBufferObject *pinBB = new PinBufferObject(this->mock);
    testedMemoryManager->injectPinBB(pinBB);

    mock->reset();
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 0; // pinning for host memory validation is mocked

    OsHandleStorage handleStorage;
    OsHandle handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = testedMemoryManager->populateOsHandles(handleStorage);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_EQ(handleStorage.fragmentStorageData[1].osHandleStorage->bo, pinBB->pinnedBoArray[0]);
    EXPECT_EQ(handleStorage.fragmentStorageData[2].osHandleStorage->bo, pinBB->pinnedBoArray[1]);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    testedMemoryManager->cleanOsHandles(handleStorage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenValidateHostPtrMemoryEnabledWhenHostPtrAllocationIsCreatedWithoutForcingPinThenBufferObjectIsPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, true, true, executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    size_t size = 10 * 1024 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenValidHostPointerIsPassedToPopulateThenSuccessIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    auto result = testedMemoryManager->populateOsHandles(storage);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    testedMemoryManager->cleanOsHandles(storage);
}

TEST_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledWhenSmallAllocationIsCreatedThenBufferObjectIsPinned) {
    mock->ioctl_expected.gemUserptr = 2;  // 1 pinBB, 1 small allocation
    mock->ioctl_expected.execbuffer2 = 1; // pinning
    mock->ioctl_expected.gemWait = 1;     // in freeGraphicsAllocation
    mock->ioctl_expected.gemClose = 2;    // 1 pinBB, 1 small allocation

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, true, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning but pinning is used for host memory validation
    size_t size = 4 * 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, false);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, givenForcePinAllowedAndNoPinBBInMemoryManagerWhenAllocationWithForcePinFlagTrueIsCreatedThenAllocationIsNotPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_res = -1;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(this->mock, true, false, *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
    mock->ioctl_res = 0;

    auto allocation = memoryManager->allocateGraphicsMemory(4096, 4096, true, false);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsNotCreated) {
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(0, nullptr));
    uintptr_t ptr = 0x12345;
    size_t size = 100u;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(size, nullptr));
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(0, reinterpret_cast<void *>(ptr)));
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    void *hostPtr = reinterpret_cast<void *>(0x5001);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(13, hostPtr);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(reinterpret_cast<void *>(0x5001), allocation->getUnderlyingBuffer());
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->allocationOffset);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledButAllocationFailedThenNullPtrReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    size_t size = 64 * 1024 * 1024 * 1024llu;
    void *hostPtr = reinterpret_cast<void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(size, hostPtr));
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithHostPtrIsPassedAndWhenAllocUserptrFailsThenFails) {
    auto size = 10u;
    void *hostPtr = reinterpret_cast<void *>(0x1000);

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    mock->ioctl_expected.gemUserptr = 1;
    this->ioctlResExt = {0, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(size, hostPtr);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationEnabledWhenAllocationIsCreatedThenBufferObjectIsPinnedOnlyOnce) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    size_t size = 1024;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationDisabledWhenAllocationIsCreatedThenBufferObjectIsNotPinned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(this->mock, false, false, executionEnvironment));
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    size_t size = 10 * 1024 * 1024; // bigger than threshold
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemory(size, ptr, true);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesMarksFragmentsToFree) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager.get());
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    OsHandle handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = testedMemoryManager->populateOsHandles(handleStorage);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_TRUE(handleStorage.fragmentStorageData[1].freeTheFragment);
    EXPECT_TRUE(handleStorage.fragmentStorageData[2].freeTheFragment);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    testedMemoryManager->cleanOsHandles(handleStorage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesDoesNotStoreTheFragments) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    OsHandle handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = testedMemoryManager->populateOsHandles(handleStorage);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(testedMemoryManager->getHostPtrManager());

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    EXPECT_EQ(nullptr, hostPtrManager->getFragment(handleStorage.fragmentStorageData[1].cpuPtr));
    EXPECT_EQ(nullptr, hostPtrManager->getFragment(handleStorage.fragmentStorageData[2].cpuPtr));

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    testedMemoryManager->cleanOsHandles(handleStorage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenPopulateOsHandlesSucceedsThenFragmentIsStoredInHostPtrManager) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    auto result = testedMemoryManager->populateOsHandles(handleStorage);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(testedMemoryManager->getHostPtrManager());
    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    EXPECT_NE(nullptr, hostPtrManager->getFragment(handleStorage.fragmentStorageData[0].cpuPtr));

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    testedMemoryManager->cleanOsHandles(handleStorage);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCleanOsHandlesDeletesHandleDataThenOsHandleStorageAndResidencyIsSetToNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(this->mock, false, true, executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData();
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = new OsHandle();
    handleStorage.fragmentStorageData[1].residency = new ResidencyData();
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[1].fragmentSize = 4096;

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;

    testedMemoryManager->cleanOsHandles(handleStorage);

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].residency);
    }
}

TEST_F(DrmMemoryManagerTest, ifLimitedRangeAllocatorAvailableWhenAskedForAllocationThenLimitedRangePointerIsReturned) {
    ExecutionEnvironment executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(Drm::get(0), false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto limitedRangeAllocator = memoryManager->getDrmLimitedRangeAllocator();
    size_t size = 100u;
    auto ptr = limitedRangeAllocator->allocate(size);
    uintptr_t address64bit = (uintptr_t)ptr - (uintptr_t)limitedRangeAllocator->getBase();

    EXPECT_LT(address64bit, platformDevices[0]->capabilityTable.gpuAddressSpace);

    EXPECT_LT(0u, address64bit);

    limitedRangeAllocator->free(ptr, size);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWithNonZeroBaseAndSizeWhenAskedForBaseThenCorrectBaseIsReturned) {
    uint64_t size = 100u;
    uint64_t base = 0x23000;
    AllocatorLimitedRange allocator(base, size);

    EXPECT_EQ(base, allocator.getBase());
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWithZeroBaseAndSizeWhenAskedForBaseThenCorrectBaseIsReturned) {
    uint64_t size = 100u;
    uint64_t base = 0x1000;
    AllocatorLimitedRange allocator(base, size);

    EXPECT_EQ(base, allocator.getBase());
}
