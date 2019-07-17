/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_memory_manager_tests.h"

#include "core/helpers/ptr_math.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/command_stream/device_command_stream.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/event/event.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/host_ptr_manager.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/mocks/linux/mock_drm_command_stream_receiver.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"

#include "drm/i915_drm.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "hw_cmds.h"

#include <iostream>
#include <memory>

using namespace NEO;

AllocationProperties createAllocationProperties(size_t size, bool forcePin) {
    MockAllocationProperties properties(size);
    properties.alignment = MemoryConstants::preferredAlignment;
    properties.flags.forcePin = forcePin;
    return properties;
}

typedef Test<DrmMemoryManagerFixture> DrmMemoryManagerTest;
typedef Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation> DrmMemoryManagerWithExplicitExpectationsTest;

TEST_F(DrmMemoryManagerTest, whenCreatingDrmMemoryManagerThenSupportsMultiStorageResourcesFlagIsSetToFalse) {
    EXPECT_FALSE(memoryManager->supportsMultiStorageResources);
}

TEST_F(DrmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    DrmAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, cpuPtr, size, MemoryPool::MemoryNull, 1u, false);
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
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, pinBBisCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, givenNotAllowedForcePinWhenMemoryManagerIsCreatedThenPinBBIsNotCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerTest, pinBBnotCreatedWhenIoctlFailed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;
    auto memoryManager = new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
    mock->ioctl_res = 0;
    delete memoryManager;
}

TEST_F(DrmMemoryManagerTest, pinAfterAllocateWhenAskedAndAllowedAndBigAllocation) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(10 * MemoryConstants::megaByte, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledThenBoIsReturend) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AllowOpenFdOperations.set(1);
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    ASSERT_EQ(allocation->peekInternalHandle(this->memoryManager), static_cast<uint64_t>(1337));

    DebugManager.flags.AllowOpenFdOperations.set(0);
    ASSERT_EQ(allocation->peekInternalHandle(this->memoryManager), 0llu);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmContextIdWhenAllocationIsCreatedThenPinWithPassedDrmContextId) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto drmContextId = memoryManager->getDefaultDrmContextId();
    ASSERT_NE(nullptr, memoryManager->getPinBB());
    EXPECT_NE(0u, drmContextId);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(memoryManager->pinThreshold, true));
    EXPECT_EQ(drmContextId, mock->execBuffer.rsvd1);

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedAndAllowedButSmallAllocation) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(MemoryConstants::pageSize, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(MemoryConstants::pageSize, false)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(MemoryConstants::pageSize, true)));
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

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    allocationData.size = 10 * MB;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, givenSmallAllocationHostPtrAllocationWhenForcePinIsTrueThenBufferObjectIsNotPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenNotAskedButAllowedHostPtr) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, doNotPinAfterAllocateWhenAskedButNotAllowedHostPtr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, unreference) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul);
    ASSERT_NE(nullptr, bo);
    memoryManager->unreference(bo);
}

TEST_F(DrmMemoryManagerTest, UnreferenceNullPtr) {
    memoryManager->unreference(nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerModeInactiveThenGemCloseWorkerIsNotCreated) {
    DrmMemoryManager drmMemoryManger(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    EXPECT_EQ(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerActiveThenGemCloseWorkerIsCreated) {
    DrmMemoryManager drmMemoryManger(gemCloseWorkerMode::gemCloseWorkerActive, false, false, *executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerTest, AllocateThenFree) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, AllocateNewFail) {
    mock->ioctl_expected.total = -1; //don't care

    InjectedFunction method = [this](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
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

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0u});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate3Bytes) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, AllocateUserptrFail) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_EQ(nullptr, ptr);
    mock->ioctl_res = 0;
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

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));
    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr_Nullptr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    void *ptr = nullptr;

    allocationData.hostPtr = nullptr;
    allocationData.size = MemoryConstants::pageSize;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));

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

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_FALSE(bo->peekIsAllocated());
    EXPECT_EQ(ptrT, reinterpret_cast<void *>(bo->peekAddress()));

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptrT);
}

TEST_F(DrmMemoryManagerTest, Allocate_HostPtr_UserptrFail) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, 1024}, ptrT);
    EXPECT_EQ(nullptr, alloc);

    ::alignedFree(ptrT);
    mock->ioctl_res = 0;
}

TEST_F(DrmMemoryManagerTest, givenDrmAllocationWhenHandleFenceCompletionThenCallBufferObjectWait) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.contextDestroy = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{1024});
    memoryManager->handleFenceCompletion(allocation);
    mock->testIoctls();

    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.contextDestroy = static_cast<int>(device->getExecutionEnvironment()->commandStreamReceivers[0].size());
    memoryManager->freeGraphicsMemory(allocation);
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
    mock->ioctl_expected.contextDestroy = 0;
    mock->testIoctls();

    // gpuMemSize > hostMemSize
    gpuMemorySize = hostMemorySize + 1u;
    mock->getContextParamRetValue = gpuMemorySize;
    systemSharedMemorySize = memoryManager->getSystemSharedMemory();
    mock->ioctl_expected.contextGetParam = 2;
    EXPECT_EQ(hostMemorySize, systemSharedMemorySize);
    mock->testIoctls();
    mock->ioctl_expected.contextDestroy = static_cast<int>(device->getExecutionEnvironment()->commandStreamReceivers[0].size());
}

TEST_F(DrmMemoryManagerTest, BoWaitFailure) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, 0ul);
    ASSERT_NE(nullptr, bo);
    mock->ioctl_res = -EIO;
    EXPECT_THROW(bo->wait(-1), std::exception);
    mock->ioctl_res = 1;

    memoryManager->unreference(bo);
    mock->ioctl_res = 0;
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
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                          false,
                                                                                                          true,
                                                                                                          *executionEnvironment));

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
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                          false,
                                                                                                          true,
                                                                                                          *executionEnvironment));

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

TEST_F(DrmMemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagEnabledWhenAllocateMemoryThenAllocationIsMultiOsContextCapable) {
    mock->ioctl_expected.total = -1;
    AllocationProperties properties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER};
    properties.flags.multiOsContextCapable = true;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_TRUE(allocation->isMultiOsContextCapable());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenAllocationPropertiesWithMultiOsContextCapableFlagDisabledWhenAllocateMemoryThenAllocationIsNotMultiOsContextCapable) {
    mock->ioctl_expected.total = -1;
    AllocationProperties properties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER};
    properties.flags.multiOsContextCapable = false;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_FALSE(allocation->isMultiOsContextCapable());
    memoryManager->freeGraphicsMemory(allocation);
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

    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->createGraphicsAllocation(handleStorage, allocationData));

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), ptr);
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
}

TEST_F(DrmMemoryManagerTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocation64kThenNullPtr) {
    allocationData.size = MemoryConstants::pageSize64k;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllcoationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    mock->ioctl_expected.gemUserptr = 3;
    mock->ioctl_expected.gemWait = 3;
    mock->ioctl_expected.gemClose = 3;

    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        ASSERT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekSize());
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr, reinterpret_cast<void *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekAddress()));
        EXPECT_FALSE(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage->bo->peekIsAllocated());
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationThen32BitDrmAllocationIsBeingReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    auto address64bit = allocation->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    auto bo = allocation->getBO();
    EXPECT_GE(bo->peekUnmapSize(), 0u);
    EXPECT_TRUE(allocation->is32BitAllocation());

    EXPECT_EQ(GmmHelper::canonize(memoryManager->getExternalHeapBaseAddress()), allocation->getGpuBaseAddress());

    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_EXTERNAL);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWhenLimitedAllocationEnabledThen32BitDrmAllocationWithGpuAddrDifferentFromCpuAddrIsBeingReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    EXPECT_NE((uint64_t)allocation->getGpuAddress(), (uint64_t)allocation->getUnderlyingBuffer());
    memoryManager->freeGraphicsMemory(allocation);
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
    auto baseAddress = buffer->getGraphicsAllocation()->getGpuBaseAddress();

    EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

    delete buffer;
}

TEST_F(DrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFromHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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

    auto baseAddress = buffer->getGraphicsAllocation()->getGpuBaseAddress();
    EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto allocationCpuPtr = drmAllocation->getUnderlyingBuffer();
    auto allocationPageOffset = ptrDiff(allocationCpuPtr, alignDown(allocationCpuPtr, MemoryConstants::pageSize));

    auto allocationGpuPtr = drmAllocation->getGpuAddress();
    auto allocationGpuOffset = ptrDiff(allocationGpuPtr, alignDown(allocationGpuPtr, MemoryConstants::pageSize));

    auto bufferObject = drmAllocation->getBO();

    EXPECT_NE(0u, bufferObject->peekUnmapSize());
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

            auto baseAddress = buffer->getGraphicsAllocation()->getGpuBaseAddress();
            EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

            auto drmAllocation = static_cast<DrmAllocation *>(buffer->getGraphicsAllocation());

            EXPECT_TRUE(drmAllocation->is32BitAllocation());

            auto allocationCpuPtr = drmAllocation->getUnderlyingBuffer();
            auto allocationPageOffset = ptrDiff(allocationCpuPtr, alignDown(allocationCpuPtr, MemoryConstants::pageSize));
            auto bufferObject = drmAllocation->getBO();

            EXPECT_NE(0u, bufferObject->peekUnmapSize());

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

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenLimitedRangeAllocatorSetThenHeapSizeAndEndAddrCorrectlySetForGivenGpuRange) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    uint64_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    auto gpuAddressLimitedRange = memoryManager->gfxPartition.heapAllocate(HeapIndex::HEAP_STANDARD, sizeBig);
    EXPECT_LT(memoryManager->gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange);
    EXPECT_GT(memoryManager->gfxPartition.getHeapLimit(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange + sizeBig);
    EXPECT_EQ(memoryManager->gfxPartition.getHeapMinimalAddress(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange);

    auto gpuInternal32BitAlloc = memoryManager->gfxPartition.heapAllocate(internalHeapIndex, sizeBig);
    EXPECT_LT(memoryManager->gfxPartition.getHeapBase(internalHeapIndex), gpuInternal32BitAlloc);
    EXPECT_GT(memoryManager->gfxPartition.getHeapLimit(internalHeapIndex), gpuInternal32BitAlloc + sizeBig);
    EXPECT_EQ(memoryManager->gfxPartition.getHeapMinimalAddress(internalHeapIndex), gpuInternal32BitAlloc);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForAllocationWithAlignmentThenCorrectAllocatorTypeSelected) {
    // if limitedRangeAllocator is enabled by default on the platform, only limitedRangeAllocator case will be tested.
    auto limitedRange = memoryManager->isLimitedRange();
    if (limitedRange) {
        mock->ioctl_expected.gemUserptr = 1;
        mock->ioctl_expected.gemWait = 1;
        mock->ioctl_expected.gemClose = 1;
    } else {
        mock->ioctl_expected.gemUserptr = 2;
        mock->ioctl_expected.gemWait = 2;
        mock->ioctl_expected.gemClose = 2;
    }

    TestedDrmMemoryManager::AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    DrmAllocation *allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    auto bo = allocation->getBO();

    // if limitedRangeAllocator is enabled by default on the platform, expect the allocator type
    // is always INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE
    if (limitedRange) {
        EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, bo->peekAllocationType());
        memoryManager->freeGraphicsMemory(allocation);
    } else {
        //allocation type should be UNKNOWN_ALLOCATOR
        EXPECT_EQ(UNKNOWN_ALLOCATOR, bo->peekAllocationType());

        memoryManager->freeGraphicsMemory(allocation);

        memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
        allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

        bo = allocation->getBO();
        //make sure allocation type is set for limitedRangeAllocation
        EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, bo->peekAllocationType());
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForAllocationWithAlignmentAndLimitedRangeAllocatorSetAndAcquireGpuRangeFailsThenNullIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    TestedDrmMemoryManager::AllocationData allocationData;

    // emulate GPU address space exhaust
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    memoryManager->gfxPartition.heapInit(HeapIndex::HEAP_STANDARD, 0x0, 0x10000);

    // set size to something bigger than allowed space
    allocationData.size = 0x20000;
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWithHostPtrAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    void *host_ptr = reinterpret_cast<void *>(0x1000);
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, host_ptr, GraphicsAllocation::AllocationType::BUFFER);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::BUFFER);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternal32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP);

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

    size_t size = 2 * GB;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        ptr,
        retVal);

    size_t size2 = 4 * GB - MemoryConstants::pageSize; // Keep size aligned

    auto buffer2 = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size2,
        nullptr,
        retVal);

    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(nullptr, buffer2);

    if (buffer) {
        auto bufferPtr = buffer->getGraphicsAllocation()->getGpuAddress();

        EXPECT_TRUE(buffer->getGraphicsAllocation()->is32BitAllocation());
        auto baseAddress = buffer->getGraphicsAllocation()->getGpuBaseAddress();
        EXPECT_LT(ptrDiff(bufferPtr, baseAddress), MemoryConstants::max32BitAddress);
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

    size_t size = 2 * GB;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    size_t size2 = 4 * GB - MemoryConstants::pageSize; // Keep size aligned

    auto buffer2 = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size2,
        ptr,
        retVal);

    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(nullptr, buffer2);

    if (buffer) {
        auto bufferPtr = buffer->getGraphicsAllocation()->getGpuAddress();

        EXPECT_TRUE(buffer->getGraphicsAllocation()->is32BitAllocation());
        auto baseAddress = buffer->getGraphicsAllocation()->getGpuBaseAddress();
        EXPECT_LT(ptrDiff(bufferPtr, baseAddress), MemoryConstants::max32BitAddress);
    }

    delete buffer;
}

TEST_F(DrmMemoryManagerTest, givenDrmBufferWhenItIsQueriedForInternalAllocationThenBoIsReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AllowOpenFdOperations.set(1);
    mock->ioctl_expected.total = -1;
    mock->outputFd = 1337;
    MockContext context;
    context.setMemoryManager(memoryManager);

    size_t size = 1u;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    uint64_t handle = 0llu;

    retVal = clGetMemObjectInfo(buffer, CL_MEM_ALLOCATION_HANDLE_INTEL, sizeof(handle), &handle, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(static_cast<uint64_t>(1337), handle);

    clReleaseMemObject(buffer);
}

TEST_F(DrmMemoryManagerTest, Given32BitDeviceWithMemoryManagerWhenInternalHeapIsExhaustedAndNewAllocationsIsMadeThenNullIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.Force32bitAddressing.set(true);
    memoryManager->setForce32BitAllocations(true);
    std::unique_ptr<Device> pDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    size_t size = MemoryConstants::pageSize64k;
    auto alloc = memoryManager->gfxPartition.heapAllocate(internalHeapIndex, size);
    EXPECT_NE(0llu, alloc);

    size_t allocationSize = 4 * GB;
    auto graphicsAllocation = memoryManager->allocate32BitGraphicsMemory(allocationSize, nullptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP);
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

    TestedDrmMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageGraphicsAllocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_NE(0u, imageGraphicsAllocation->getGpuAddress());
    EXPECT_EQ(nullptr, imageGraphicsAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    EXPECT_EQ(imgInfo.size, drmAllocation->getBO()->peekUnmapSize());

    if (memoryManager->isLimitedRange()) {
        EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, drmAllocation->getBO()->peekAllocationType());
    } else {
        EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());
    }

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imgInfo.size, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_Y;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(imgInfo.rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    memoryManager->freeGraphicsMemory(imageGraphicsAllocation);

    if (!memoryManager->isLimitedRange()) {
        EXPECT_EQ(1, mmapMockCallCount);
        EXPECT_EQ(1, munmapMockCallCount);
    }
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
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
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
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
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
        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, dstImage.get());
        } else {
            EXPECT_EQ(nullptr, dstImage.get());
        }
    };

    injectFailures(method);
    mock->reset();
    mock->ioctl_expected.contextDestroy = static_cast<int>(device->getExecutionEnvironment()->commandStreamReceivers[0].size());
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

    MockContext context(device);
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto data = alignedMalloc(64u * 64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
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

    alignedFree(data);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenMemoryAllocatedForImageThenUnmapSizeCorrectlySetWhenLimitedRangeAllocationUsedOrNotUsed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);

    // if limitedRangeAllocator is enabled, gpuRange is acquired and it should be
    // set as unmapsize for freeing in the furture.
    auto limitedRange = memoryManager->isLimitedRange();
    if (!limitedRange) {
        EXPECT_EQ(0u, drmAllocation->getBO()->peekUnmapSize());
    } else {
        EXPECT_NE(0u, drmAllocation->getBO()->peekUnmapSize());
    }

    alignedFree(data);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_NONE;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
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

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_NONE;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhen1DarrayImageIsBeingCreatedFromHostPtrThenTilingIsNotCalled) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

    MockContext context;
    context.setMemoryManager(memoryManager);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(&context, flags, surfaceFormat, &imageDesc, data, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation();
    ASSERT_NE(nullptr, imageGraphicsAllocation);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    __u32 tilingMode = I915_TILING_NONE;
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

TEST_F(DrmMemoryManagerTest, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    cl_image_desc imgDesc = {};
    imgDesc.image_width = MemoryConstants::pageSize;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.image_width * surfaceFormat->ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.image_width * imgDesc.image_height * 4, MemoryConstants::pageSize);
    bool copyRequired = Image::isCopyRequired(imgInfo, hostPtr);
    EXPECT_FALSE(copyRequired);

    TestedDrmMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;

    auto imageAllocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_EQ(hostPtr, imageAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(imageAllocation);
    alignedFree(hostPtr);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_EQ(bo->peekUnmapSize(), size);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenOsHandleWithNonTiledObjectWhenCreateFromSharedHandleIsCalledThenNonTiledGmmIsCreatedAndSetInAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemGetTiling = 1;
    mock->getTilingModeOut = I915_TILING_NONE;

    osHandle handle = 1u;
    uint32_t boHandle = 2u;
    mock->outputHandle = boHandle;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    const SurfaceFormatInfo *gmmSurfaceFormat = nullptr;
    ImageInfo imgInfo = {0};

    imgInfo.imgDesc = &imgDesc;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat);
    imgInfo.surfaceFormat = gmmSurfaceFormat;
    imgInfo.plane = GMM_PLANE_Y;

    AllocationProperties properties(false, imgInfo, GraphicsAllocation::AllocationType::SHARED_IMAGE);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.TiledY);
    EXPECT_EQ(TilingMode::NON_TILED, imgInfo.tilingMode);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenOsHandleWithTileYObjectWhenCreateFromSharedHandleIsCalledThenTileYGmmIsCreatedAndSetInAllocation) {
    auto &hwHelper = HwHelper::get(GmmHelper::getInstance()->getHardwareInfo()->platform.eRenderCoreFamily);

    if (hwHelper.supportsYTiling()) {
        mock->ioctl_expected.primeFdToHandle = 1;
        mock->ioctl_expected.gemWait = 1;
        mock->ioctl_expected.gemClose = 1;
        mock->ioctl_expected.gemGetTiling = 1;
        mock->getTilingModeOut = I915_TILING_Y;

        osHandle handle = 1u;
        uint32_t boHandle = 2u;
        mock->outputHandle = boHandle;

        cl_mem_flags flags = CL_MEM_READ_ONLY;
        cl_image_desc imgDesc = {};
        cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
        const SurfaceFormatInfo *gmmSurfaceFormat = nullptr;
        ImageInfo imgInfo = {0};

        imgInfo.imgDesc = &imgDesc;
        imgDesc.image_width = 4;
        imgDesc.image_height = 4;
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat);
        imgInfo.surfaceFormat = gmmSurfaceFormat;
        imgInfo.plane = GMM_PLANE_Y;

        AllocationProperties properties(false, imgInfo, GraphicsAllocation::AllocationType::SHARED_IMAGE);

        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
        ASSERT_NE(nullptr, graphicsAllocation);
        EXPECT_EQ(boHandle, mock->getTilingHandleIn);
        EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, graphicsAllocation->getAllocationType());

        auto gmm = graphicsAllocation->getDefaultGmm();
        ASSERT_NE(nullptr, gmm);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.TiledY);
        EXPECT_EQ(TilingMode::TILE_Y, imgInfo.tilingMode);

        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenAllocationFailsThenReturnNullPtr) {
    osHandle handle = 1u;

    InjectedFunction method = [this, &handle](size_t failureIndex) {
        AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, graphicsAllocation);
            memoryManager->freeGraphicsMemory(graphicsAllocation);
        } else {
            EXPECT_EQ(nullptr, graphicsAllocation);
        }
    };

    injectFailures(method);
    mock->reset();
    mock->ioctl_expected.contextDestroy = static_cast<int>(device->getExecutionEnvironment()->commandStreamReceivers[0].size());
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

        AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

        graphicsAllocations[i] = memoryManager->createGraphicsAllocationFromSharedHandle(handles[i], properties, false);
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
        EXPECT_NE(0llu, bo->peekAddress());
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

TEST_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleAndBitnessRequiredIsCreatedThenItis32BitAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;

    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
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
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, lseekCalledCount);

    if (memoryManager->isLimitedRange()) {
        EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, drmAllocation->getBO()->peekAllocationType());
    } else {
        EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());
    }

    memoryManager->freeGraphicsMemory(graphicsAllocation);

    if (!memoryManager->isLimitedRange()) {
        EXPECT_EQ(1, mmapMockCallCount);
        EXPECT_EQ(1, munmapMockCallCount);
    }
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenBufferFromSharedHandleIsCreatedThenItIsLimitedRangeAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
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
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, lseekCalledCount);

    if (memoryManager->isLimitedRange()) {
        EXPECT_EQ(INTERNAL_ALLOCATOR_WITH_DYNAMIC_BITRANGE, drmAllocation->getBO()->peekAllocationType());
    } else {
        EXPECT_EQ(MMAP_ALLOCATOR, drmAllocation->getBO()->peekAllocationType());
    }

    memoryManager->freeGraphicsMemory(graphicsAllocation);

    if (!memoryManager->isLimitedRange()) {
        EXPECT_EQ(1, mmapMockCallCount);
        EXPECT_EQ(1, munmapMockCallCount);
    }
}

TEST_F(DrmMemoryManagerTest, givenSharedHandleWhenAllocationIsCreatedAndIoctlPrimeFdToHandleFailsThenNullPtrIsReturned) {
    mock->ioctl_expected.primeFdToHandle = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &this->ioctlResExt;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
    EXPECT_EQ(nullptr, graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatShareTheSameBufferObjectWhenTheyAreMadeResidentThenOnlyOneBoIsPassedToExec) {
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);

    executionEnvironment->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->osInterface->get()->setDrm(mock.get());
    auto testedCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*executionEnvironment);
    device->resetCommandStreamReceiver(testedCsr);

    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);
    EXPECT_EQ(2u, testedCsr->getResidencyAllocations().size());

    testedCsr->processResidency(testedCsr->getResidencyAllocations());

    EXPECT_EQ(1u, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatDoesnShareTheSameBufferObjectWhenTheyAreMadeResidentThenTwoBoIsPassedToExec) {
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);
    mock->outputHandle++;
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);

    executionEnvironment->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->osInterface->get()->setDrm(mock.get());
    auto testedCsr = new TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME>(*executionEnvironment);
    device->resetCommandStreamReceiver(testedCsr);

    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);
    EXPECT_EQ(2u, testedCsr->getResidencyAllocations().size());

    testedCsr->processResidency(testedCsr->getResidencyAllocations());

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

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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

    TestedDrmMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto allocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

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
    DrmAllocation drmAllocation(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, 1u, false);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledButFailsOnIoctlMmapThenReturnNullPtr) {
    mock->ioctl_expected.gemMmap = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    DrmMockCustom drmMock;
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 1, true) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(GraphicsAllocation::AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, 1u, false);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationWithoutBufferObjectThenReturnFalse) {
    DrmAllocation drmAllocation(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, 1u, false);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledButFailsOnIoctlSetDomainThenReturnFalse) {
    mock->ioctl_expected.gemSetDomain = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    DrmMockCustom drmMock;
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 1, true) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(GraphicsAllocation::AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, 1u, false);
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
    DrmAllocation drmAllocation(GraphicsAllocation::AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, 1u, false);
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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    allocation->setDefaultGmm(gmm);

    auto mockGmmRes = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager->mapAuxGpuVA(allocation));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, given32BitAllocatorWithHeapAllocatorWhenLargerFragmentIsReusedThenOnlyUnmapSizeIsLargerWhileSizeStaysTheSame) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    DebugManagerStateRestore dbgFlagsKeeper;
    memoryManager->setForce32BitAllocations(true);

    size_t allocationSize = 4 * MemoryConstants::pageSize;
    auto ptr = memoryManager->gfxPartition.heapAllocate(HeapIndex::HEAP_EXTERNAL, allocationSize);
    size_t smallAllocationSize = MemoryConstants::pageSize;
    memoryManager->gfxPartition.heapAllocate(HeapIndex::HEAP_EXTERNAL, smallAllocationSize);

    //now free first allocation , this will move it to chunks
    memoryManager->gfxPartition.heapFree(HeapIndex::HEAP_EXTERNAL, ptr, allocationSize);

    //now ask for 3 pages, this will give ptr from chunks
    size_t pages3size = 3 * MemoryConstants::pageSize;

    void *host_ptr = reinterpret_cast<void *>(0x1000);
    DrmAllocation *graphicsAlloaction = memoryManager->allocate32BitGraphicsMemory(pages3size, host_ptr, GraphicsAllocation::AllocationType::BUFFER);

    auto bo = graphicsAlloaction->getBO();
    EXPECT_EQ(allocationSize, bo->peekUnmapSize());
    EXPECT_EQ(pages3size, bo->peekSize());
    EXPECT_EQ(GmmHelper::canonize(ptr), graphicsAlloaction->getGpuAddress());

    memoryManager->freeGraphicsMemory(graphicsAlloaction);
}

TEST_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * 1024;
    lseekReturn = realSize;
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    osHandle sharedHandle = 1u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(realSize, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)sharedHandle);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_EQ(bo->peekUnmapSize(), realSize);
    EXPECT_NE(0llu, bo->peekAddress());
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
    auto buffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{bufferSize});

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
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();

    auto heapBase = GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress());
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    auto bo = drmAllocation->getBO();
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_INTERNAL);
    EXPECT_EQ(bo->peekUnmapSize(), bufferSize);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    ASSERT_NE(nullptr, drmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_EQ(drmAllocation->getDriverAllocatedCpuPtr(), drmAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();

    auto heapBase = GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress());
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    auto bo = drmAllocation->getBO();
    EXPECT_TRUE(bo->peekIsAllocated());
    EXPECT_EQ(bo->peekAllocationType(), StorageAllocatorType::BIT32_ALLOCATOR_INTERNAL);
    EXPECT_EQ(bo->peekUnmapSize(), bufferSize);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForExternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, GraphicsAllocation::AllocationType::BUFFER));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerAndHugeBufferSizeThenAllocationFromInternalHeapFailed) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = 128 * MemoryConstants::megaByte + 4 * MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_EQ(nullptr, drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x100000);
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(bufferSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(ptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();

    auto heapBase = GmmHelper::canonize(memoryManager->getInternalHeapBaseAddress());
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

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
    this->ioctlResExt = {mock->ioctl_cnt.total + 2, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    //first let's create normal buffer
    auto bufferSize = MemoryConstants::pageSize;
    auto buffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{bufferSize});

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
    auto size = 4 * GB;
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

TEST_F(DrmMemoryManagerBasic, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(DrmMemoryManagerBasic, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(DrmMemoryManagerBasic, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(DrmMemoryManagerBasic, givenDefaultDrmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenInternalHeapBaseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    auto heapBase = memoryManager->gfxPartition.getHeapBase(internalHeapIndex);
    EXPECT_EQ(heapBase, memoryManager->getInternalHeapBaseAddress());
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithEnabledHostMemoryValidationWhenFeatureIsQueriedThenTrueIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_TRUE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithDisabledHostMemoryValidationWhenFeatureIsQueriedThenFalseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_FALSE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationAndForcePinWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->getPinBB());
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(
        false,
        false,
        true,
        executionEnvironment));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(MemoryConstants::pageSize, false));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    executionEnvironment->osInterface->get()->setDrm(Drm::get(0));
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));

    memoryManager->setForce32BitAllocations(true);

    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    auto osHandle = 1u;
    AllocationProperties properties(false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, DISABLED_givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPinBBAllocationFailsThenUnrecoverableIsCalled) {
    this->mock->ioctl_res = -1;
    this->mock->ioctl_expected.gemUserptr = 1;
    EXPECT_THROW(
        {
            std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false,
                                                                                                   false,
                                                                                                   true,
                                                                                                   *executionEnvironment));
            EXPECT_NE(nullptr, testedMemoryManager.get());
        },
        std::exception);
    this->mock->ioctl_res = 0;
    this->mock->ioctl_expected.contextDestroy = 0;
    this->mock->testIoctls();
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledThenHostMemoryIsValidated) {

    std::unique_ptr<DrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
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

        int pin(BufferObject *const boToPin[], size_t numberOfBos, uint32_t drmContextId) override {
            for (size_t i = 0; i < numberOfBos; i++) {
                pinnedBoArray[i] = boToPin[i];
            }
            numberOfBosPinned = numberOfBos;
            return 0;
        }
        BufferObject *pinnedBoArray[5];
        size_t numberOfBosPinned;
    };

    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    ASSERT_NE(nullptr, testedMemoryManager.get());
    ASSERT_NE(nullptr, testedMemoryManager->getPinBB());

    PinBufferObject *pinBB = new PinBufferObject(this->mock.get());
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

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    size_t size = 10 * MB;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenValidHostPointerIsPassedToPopulateThenSuccessIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                          false,
                                                                                                          true,
                                                                                                          *executionEnvironment));

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

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->getPinBB());

    // one page is too small for early pinning but pinning is used for host memory validation
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, givenForcePinAllowedAndNoPinBBInMemoryManagerWhenAllocationWithForcePinFlagTrueIsCreatedThenAllocationIsNotPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_res = -1;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->getPinBB());
    mock->ioctl_res = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(MemoryConstants::pageSize, true));
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenNullptrOrZeroSizeWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsNotCreated) {
    allocationData.size = 0;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 100;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 0;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x12345);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()) + allocation->getAllocationOffset());
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrObjectAlignedSizeIsUsedByAllocUserPtrWhenBiggerSizeAllocatedInHeap) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 4 * MB + 16 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x10000000);
    auto allocation0 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    allocationData.hostPtr = reinterpret_cast<const void *>(0x20000000);
    auto allocation1 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    memoryManager->freeGraphicsMemory(allocation0);

    allocationData.size = 4 * MB + 12 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x30000000);
    allocation0 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ((uint64_t)(allocation0->getBO()->peekUnmapSize()), 4 * MB + 16 * 1024);
    EXPECT_EQ((uint64_t)(allocation0->getBO()->peekSize()), 4 * MB + 12 * 1024);

    memoryManager->freeGraphicsMemory(allocation0);
    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledButAllocationFailedThenNullPtrReturned) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 64 * GB;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithHostPtrIsPassedAndWhenAllocUserptrFailsThenFails) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    mock->ioctl_expected.gemUserptr = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    allocationData.size = 10;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1000);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationEnabledWhenAllocationIsCreatedThenBufferObjectIsPinnedOnlyOnce) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationDisabledWhenAllocationIsCreatedThenBufferObjectIsNotPinned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, false, *executionEnvironment));
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 10 * MB; // bigger than threshold
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesMarksFragmentsToFree) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
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
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesDoesNotStoreTheFragments) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
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
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenPopulateOsHandlesSucceedsThenFragmentIsStoredInHostPtrManager) {
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
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
    std::unique_ptr<TestedDrmMemoryManager> testedMemoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
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

TEST_F(DrmMemoryManagerBasic, ifLimitedRangeAllocatorAvailableWhenAskedForAllocationThenLimitedRangePointerIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    size_t size = 100u;
    auto ptr = memoryManager->gfxPartition.heapAllocate(HeapIndex::HEAP_STANDARD, size);
    auto address64bit = ptrDiff(ptr, memoryManager->gfxPartition.getHeapBase(HeapIndex::HEAP_STANDARD));

    EXPECT_LT(address64bit, platformDevices[0]->capabilityTable.gpuAddressSpace);

    EXPECT_LT(0u, address64bit);

    memoryManager->gfxPartition.heapFree(HeapIndex::HEAP_STANDARD, ptr, size);
}

TEST_F(DrmMemoryManagerBasic, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableHostPtrTracking.set(false);

    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()) + allocation->getAllocationOffset());
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenImageOrSharedResourceCopyWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    GraphicsAllocation::AllocationType types[] = {GraphicsAllocation::AllocationType::IMAGE,
                                                  GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY};

    for (uint32_t i = 0; i < arrayCount(types); i++) {
        allocData.type = types[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
    }
}

TEST_F(DrmMemoryManagerBasic, givenLocalMemoryDisabledWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    const bool localMemoryEnabled = false;
    TestedDrmMemoryManager memoryManager(localMemoryEnabled, false, false, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(DrmAllocationTest, givenAllocationTypeWhenPassedToDrmAllocationConstructorThenAllocationTypeIsStored) {
    DrmAllocation allocation{GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::MemoryNull, false};
    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, allocation.getAllocationType());

    DrmAllocation allocation2{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::MemoryNull, false};
    EXPECT_EQ(GraphicsAllocation::AllocationType::UNKNOWN, allocation2.getAllocationType());
}

TEST(DrmAllocationTest, givenMemoryPoolWhenPassedToDrmAllocationConstructorThenMemoryPoolIsStored) {
    DrmAllocation allocation{GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::System64KBPages, false};
    EXPECT_EQ(MemoryPool::System64KBPages, allocation.getMemoryPool());

    DrmAllocation allocation2{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::SystemCpuInaccessible, false};
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation2.getMemoryPool());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    constexpr size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size);
    EXPECT_NE(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, whenObtainFdFromHandleIsCalledThenProperFdHandleIsReturned) {
    int boHandle = 3;
    this->mock->outputFd = 1337;
    this->mock->ioctl_expected.handleToPrimeFd = 1;
    auto fdHandle = memoryManager->obtainFdFromHandle(boHandle);
    EXPECT_EQ(this->mock->inputHandle, static_cast<uint32_t>(boHandle));
    EXPECT_EQ(this->mock->inputFlags, DRM_CLOEXEC | DRM_RDWR);
    EXPECT_EQ(1337, fdHandle);
}
