/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/fixtures/memory_allocator_multi_device_fixture.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <array>
#include <memory>
#include <vector>

namespace {
using MemoryManagerMultiDeviceSharedHandleTest = MemoryAllocatorMultiDeviceFixture<2>;
using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;
using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;
using DrmMemoryManagerWithExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation>;
using DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation>;
using DrmMemoryManagerUSMHostAllocationTests = Test<DrmMemoryManagerFixture>;

AllocationProperties createAllocationProperties(uint32_t rootDeviceIndex, size_t size, bool forcePin) {
    MockAllocationProperties properties(rootDeviceIndex, size);
    properties.alignment = MemoryConstants::preferredAlignment;
    properties.flags.forcePin = forcePin;
    return properties;
}
} // namespace

TEST_P(MemoryManagerMultiDeviceSharedHandleTest, whenCreatingAllocationFromSharedHandleWithSameHandleAndSameRootDeviceThenSameBOIsUsed) {
    uint32_t handle0 = 0;
    uint32_t rootDeviceIndex0 = 0;
    AllocationProperties properties0{rootDeviceIndex0, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation0 = memoryManager->createGraphicsAllocationFromSharedHandle(handle0, properties0, false, false, true);
    ASSERT_NE(gfxAllocation0, nullptr);
    EXPECT_EQ(rootDeviceIndex0, gfxAllocation0->getRootDeviceIndex());

    uint32_t handle1 = 0;
    uint32_t rootDeviceIndex1 = 0;
    AllocationProperties properties1{rootDeviceIndex1, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(handle1, properties1, false, false, true);
    ASSERT_NE(gfxAllocation1, nullptr);
    EXPECT_EQ(rootDeviceIndex1, gfxAllocation1->getRootDeviceIndex());

    DrmAllocation *drmAllocation0 = static_cast<DrmAllocation *>(gfxAllocation0);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);

    EXPECT_EQ(drmAllocation0->getBO(), drmAllocation1->getBO());

    memoryManager->freeGraphicsMemory(gfxAllocation0);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

TEST_P(MemoryManagerMultiDeviceSharedHandleTest, whenCreatingAllocationFromSharedHandleWithSameHandleAndDifferentRootDeviceThenDifferentBOIsUsed) {
    uint32_t handle0 = 0;
    uint32_t rootDeviceIndex0 = 0;
    AllocationProperties properties0{rootDeviceIndex0, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation0 = memoryManager->createGraphicsAllocationFromSharedHandle(handle0, properties0, false, false, true);
    ASSERT_NE(gfxAllocation0, nullptr);
    EXPECT_EQ(rootDeviceIndex0, gfxAllocation0->getRootDeviceIndex());

    uint32_t handle1 = 0;
    uint32_t rootDeviceIndex1 = 1;
    AllocationProperties properties1{rootDeviceIndex1, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(handle1, properties1, false, false, true);
    ASSERT_NE(gfxAllocation1, nullptr);
    EXPECT_EQ(rootDeviceIndex1, gfxAllocation1->getRootDeviceIndex());

    DrmAllocation *drmAllocation0 = static_cast<DrmAllocation *>(gfxAllocation0);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);

    EXPECT_NE(drmAllocation0->getBO(), drmAllocation1->getBO());

    memoryManager->freeGraphicsMemory(gfxAllocation0);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

TEST_P(MemoryManagerMultiDeviceSharedHandleTest, whenCreatingAllocationFromSharedHandleWithDifferentHandleAndSameRootDeviceThenDifferentBOIsUsed) {
    uint32_t handle0 = 0;
    uint32_t rootDeviceIndex0 = 0;
    AllocationProperties properties0{rootDeviceIndex0, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation0 = memoryManager->createGraphicsAllocationFromSharedHandle(handle0, properties0, false, false, true);
    ASSERT_NE(gfxAllocation0, nullptr);
    EXPECT_EQ(rootDeviceIndex0, gfxAllocation0->getRootDeviceIndex());

    uint32_t handle1 = 1;
    uint32_t rootDeviceIndex1 = 0;
    AllocationProperties properties1{rootDeviceIndex1, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(handle1, properties1, false, false, true);
    ASSERT_NE(gfxAllocation1, nullptr);
    EXPECT_EQ(rootDeviceIndex1, gfxAllocation1->getRootDeviceIndex());

    DrmAllocation *drmAllocation0 = static_cast<DrmAllocation *>(gfxAllocation0);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);

    EXPECT_NE(drmAllocation0->getBO(), drmAllocation1->getBO());

    memoryManager->freeGraphicsMemory(gfxAllocation0);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

TEST_P(MemoryManagerMultiDeviceSharedHandleTest, whenCreatingAllocationFromSharedHandleWithDifferentHandleAndDifferentRootDeviceThenDifferentBOIsUsed) {
    uint32_t handle0 = 0;
    uint32_t rootDeviceIndex0 = 0;
    AllocationProperties properties0{rootDeviceIndex0, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation0 = memoryManager->createGraphicsAllocationFromSharedHandle(handle0, properties0, false, false, true);
    ASSERT_NE(gfxAllocation0, nullptr);
    EXPECT_EQ(rootDeviceIndex0, gfxAllocation0->getRootDeviceIndex());

    uint32_t handle1 = 1;
    uint32_t rootDeviceIndex1 = 1;
    AllocationProperties properties1{rootDeviceIndex1, true, MemoryConstants::pageSize, AllocationType::BUFFER, false, false, mockDeviceBitfield};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(handle1, properties1, false, false, true);
    ASSERT_NE(gfxAllocation1, nullptr);
    EXPECT_EQ(rootDeviceIndex1, gfxAllocation1->getRootDeviceIndex());

    DrmAllocation *drmAllocation0 = static_cast<DrmAllocation *>(gfxAllocation0);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);

    EXPECT_NE(drmAllocation0->getBO(), drmAllocation1->getBO());

    memoryManager->freeGraphicsMemory(gfxAllocation0);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

TEST_F(DrmMemoryManagerTest, givenEnableDirectSubmissionWhenCreateDrmMemoryManagerThenGemCloseWorkerInactive) {
    DebugManagerStateRestore dbgState;
    DebugManager.flags.EnableDirectSubmission.set(1);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    EXPECT_EQ(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerTest, givenDebugVariableWhenCreatingDrmMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore dbgState;

    EXPECT_TRUE(memoryManager->supportsMultiStorageResources);

    {
        DebugManager.flags.EnableMultiStorageResources.set(0);
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        DebugManager.flags.EnableMultiStorageResources.set(1);
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCheckForKmdMigrationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    {
        DebugManager.flags.UseKmdMigration.set(1);

        auto retVal = memoryManager->isKmdMigrationAvailable(rootDeviceIndex);

        EXPECT_TRUE(retVal);
    }
    {
        DebugManager.flags.UseKmdMigration.set(0);

        auto retVal = memoryManager->isKmdMigrationAvailable(rootDeviceIndex);

        EXPECT_FALSE(retVal);
    }

    this->dontTestIoctlInTearDown = true;
}

TEST_F(DrmMemoryManagerTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    const uint32_t rootDeviceIndex = 0u;
    DrmAllocation gfxAllocation(rootDeviceIndex, AllocationType::UNKNOWN, nullptr, cpuPtr, size, static_cast<osHandle>(1u), MemoryPool::MemoryNull);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_EQ(static_cast<OsHandleLinux *>(fragment->osInternalStorage)->bo, gfxAllocation.getBO());
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), rootDeviceIndex});
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressFromGfxPartitionIsUsed) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto addressRange = memoryManager->reserveGpuAddress(MemoryConstants::pageSize, 0);
    auto gmmHelper = memoryManager->getGmmHelper(0);

    EXPECT_LE(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_STANDARD), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_STANDARD), gmmHelper->decanonize(addressRange.address));
    memoryManager->freeGpuAddress(addressRange, 0);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSmallSizeAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenAllocationWithSpecifiedGpuAddressInSystemMemoryIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 0; // pinBB not called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenInjectedFailuresWhenGraphicsMemoryWithGpuVaIsAllocatedThenNullptrIsReturned) {
    mock->ioctl_expected.total = -1; // don't care

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    InjectedFunction method = [&](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(properties);

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, ptr);
        } else {
            EXPECT_NE(nullptr, ptr);
            memoryManager->freeGraphicsMemory(ptr);
        }
    };
    injectFailures(method);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenSizeExceedingThresholdAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenAllocationWithSpecifiedGpuAddressInSystemMemoryIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, memoryManager->pinThreshold + MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1; // pinBB called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);

    memoryManager->injectPinBB(nullptr, rootDeviceIndex); // pinBB not available

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 0; // pinBB not called

    properties.gpuAddress = 0x5000;

    allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x5000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndSizeExceedingThresholdAndGpuAddressSetWhenGraphicsMemoryIsAllocatedThenBufferIsNotPinned) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    auto osContext = device->getDefaultEngine().osContext;

    MockAllocationProperties properties = {rootDeviceIndex, memoryManager->pinThreshold + MemoryConstants::pageSize};
    properties.gpuAddress = 0x2000;
    properties.osContext = osContext;

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 0; // pinBB not called

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    mock->testIoctls();

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenforcePinAllowedWhenMemoryManagerIsCreatedThenPinBbIsCreated) {
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);
}

TEST_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenItIsCreatedThenItIsInitialized) {
    EXPECT_TRUE(memoryManager->isInitialized());
}

TEST_F(DrmMemoryManagerTest, givenDefaultDrmMemoryManagerWhenItIsCreatedAndGfxPartitionInitIsFailedThenItIsNotInitialized) {
    EXPECT_TRUE(memoryManager->isInitialized());

    auto failedInitGfxPartition = std::make_unique<FailedInitGfxPartition>();
    memoryManager->gfxPartitions[0].reset(failedInitGfxPartition.release());
    memoryManager->initialize(gemCloseWorkerMode::gemCloseWorkerInactive);
    EXPECT_FALSE(memoryManager->isInitialized());

    auto mockGfxPartitionBasic = std::make_unique<MockGfxPartitionBasic>();
    memoryManager->overrideGfxPartition(mockGfxPartitionBasic.release());
}

TEST_F(DrmMemoryManagerTest, WhenMemoryManagerIsCreatedThenPinBbIsCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerIsCreatedWhenInvokingReleaseMemResourcesBasedOnGpuDeviceThenPinBbIsRemoved) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    auto drmMemoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
    auto length = drmMemoryManager->pinBBs.size();
    drmMemoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_EQ(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerIsCreatedWhenInvokingCreatMemResourcesBasedOnGpuDeviceThenPinBbIsCreated) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;

    auto drmMemoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    auto rootDeviceBufferObjectOld = drmMemoryManager->pinBBs[rootDeviceIndex];
    EXPECT_NE(nullptr, rootDeviceBufferObjectOld);
    auto length = drmMemoryManager->pinBBs.size();
    auto memoryManagerTest = static_cast<MemoryManager *>(drmMemoryManager.get());
    drmMemoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_EQ(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);

    memoryManagerTest->createDeviceSpecificMemResources(rootDeviceIndex);
    EXPECT_EQ(length, drmMemoryManager->pinBBs.size());
    EXPECT_NE(nullptr, drmMemoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerTest, givenNotAllowedForcePinWhenMemoryManagerIsCreatedThenPinBBIsNotCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    *executionEnvironment));
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerTest, WhenIoctlFailsThenPinBbIsNotCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;
    auto memoryManager = new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    mock->ioctl_res = 0;
    delete memoryManager;
}

TEST_F(DrmMemoryManagerTest, WhenAskedAndAllowedAndBigAllocationThenPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::megaByte, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledThenBoIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, whenPeekInternalHandleIsCalledAndObtainFdFromHandleFailsThenErrorIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);
    memoryManager->failOnObtainFdFromHandle = true;
    uint64_t handle = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle);
    ASSERT_EQ(ret, -1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, whenCallingPeekInternalHandleSeveralTimesThenSameHandleIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.handleToPrimeFd = 1;
    uint64_t expectedFd = 1337;
    mock->outputFd = static_cast<int32_t>(expectedFd);
    mock->incrementOutputFdAfterCall = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(mock->outputFd, static_cast<int32_t>(expectedFd));
    uint64_t handle0 = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handle0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;

    ret = allocation->peekInternalHandle(this->memoryManager, handle1);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handle2);
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(handle0, expectedFd);
    ASSERT_EQ(handle1, expectedFd);
    ASSERT_EQ(handle2, expectedFd);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, whenPeekInternalHandleWithHandleIdIsCalledThenBoIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.handleToPrimeFd = 1;
    mock->outputFd = 1337;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    uint64_t handle = 0;
    uint32_t handleId = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle);
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(handle, static_cast<uint64_t>(1337));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, whenCallingPeekInternalHandleWithIdSeveralTimesThenSameHandleIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.handleToPrimeFd = 1;
    uint64_t expectedFd = 1337;
    mock->outputFd = static_cast<int32_t>(expectedFd);
    mock->incrementOutputFdAfterCall = true;
    auto allocation = static_cast<DrmAllocation *>(this->memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, 10 * MemoryConstants::pageSize, true)));
    ASSERT_NE(allocation->getBO(), nullptr);

    EXPECT_EQ(mock->outputFd, static_cast<int32_t>(expectedFd));
    uint32_t handleId = 0;
    uint64_t handle0 = 0;
    int ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(mock->outputFd, static_cast<int32_t>(expectedFd));

    uint64_t handle1 = 0;
    uint64_t handle2 = 0;
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle1);
    ASSERT_EQ(ret, 0);
    ret = allocation->peekInternalHandle(this->memoryManager, handleId, handle2);
    ASSERT_EQ(ret, 0);

    ASSERT_EQ(handle0, expectedFd);
    ASSERT_EQ(handle1, expectedFd);
    ASSERT_EQ(handle2, expectedFd);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmContextIdWhenAllocationIsCreatedThenPinWithPassedDrmContextId) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    auto drmContextId = memoryManager->getDefaultDrmContextId(rootDeviceIndex);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    EXPECT_NE(0u, drmContextId);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, memoryManager->pinThreshold, true));
    EXPECT_EQ(drmContextId, mock->execBuffer.getReserved());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, WhenAskedAndAllowedButSmallAllocationThenDoNotPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, WhenNotAskedButAllowedThenDoNotPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, false)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, WhenAskedButNotAllowedThenDoNotPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true)));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
}

// ---- HostPtr
TEST_F(DrmMemoryManagerTest, WhenAskedAndAllowedAndBigAllocationHostPtrThenPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemWait = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    allocationData.size = 10 * MB;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
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
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, WhenNotAskedButAllowedHostPtrThendoNotPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, WhenAskedButNotAllowedHostPtrThenDoNotPinAfterAllocate) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, WhenUnreferenceIsCalledThenCallSucceeds) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);
    memoryManager->unreference(bo, false);
}

TEST_F(DrmMemoryManagerTest, whenPrintBOCreateDestroyResultIsSetAndAllocUserptrIsCalledThenBufferObjectIsCreatedAndDebugInformationIsPrinted) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    testing::internal::CaptureStdout();
    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);

    DebugManager.flags.PrintBOCreateDestroyResult.set(false);
    std::string output = testing::internal::GetCapturedStdout();
    size_t idx = output.find("Created new BO with GEM_USERPTR, handle: BO-");
    size_t expectedValue = 0;
    EXPECT_EQ(expectedValue, idx);

    memoryManager->unreference(bo, false);
}

TEST_F(DrmMemoryManagerTest, GivenNullptrWhenUnreferenceIsCalledThenCallSucceeds) {
    memoryManager->unreference(nullptr, false);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerModeInactiveThenGemCloseWorkerIsNotCreated) {
    DrmMemoryManager drmMemoryManger(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    EXPECT_EQ(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerCreatedWithGemCloseWorkerActiveThenGemCloseWorkerIsCreated) {
    DrmMemoryManager drmMemoryManger(gemCloseWorkerMode::gemCloseWorkerActive, false, false, *executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManger.peekGemCloseWorker());
}

TEST_F(DrmMemoryManagerTest, GivenAllocationWhenClosingSharedHandleThenSucceeds) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->closeSharedHandle(graphicsAllocation);
    EXPECT_EQ(Sharing::nonSharedResource, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, GivenAllocationWhenFreeingThenSucceeds) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize}));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
}

TEST_F(DrmMemoryManagerTest, GivenInjectedFailureWhenAllocatingThenAllocationFails) {
    mock->ioctl_expected.total = -1; // don't care

    InjectedFunction method = [this](size_t failureIndex) {
        auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, ptr);
        } else {
            EXPECT_NE(nullptr, ptr);
            memoryManager->freeGraphicsMemory(ptr);
        }
    };
    injectFailures(method);
}

TEST_F(DrmMemoryManagerTest, GivenZeroBytesWhenAllocatingThenAllocationIsCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 0u});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, GivenThreeBytesWhenAllocatingThenAllocationIsCreated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, ptr);
    EXPECT_NE(nullptr, ptr->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(ptr);
}

TEST_F(DrmMemoryManagerTest, GivenUserptrWhenCreatingAllocationThenFail) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    auto ptr = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    EXPECT_EQ(nullptr, ptr);
    mock->ioctl_res = 0;
}

TEST_F(DrmMemoryManagerTest, GivenNullPtrWhenFreeingThenSucceeds) {
    memoryManager->freeGraphicsMemory(nullptr);
}

TEST_F(DrmMemoryManagerTest, GivenHostPtrWhenCreatingAllocationThenSucceeds) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    void *ptr = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptr);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));
    EXPECT_EQ(Sharing::nonSharedResource, alloc->peekSharedHandle());
    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, GivenNullHostPtrWhenCreatingAllocationThenSucceeds) {
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
    EXPECT_EQ(ptr, reinterpret_cast<void *>(bo->peekAddress()));

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerTest, GivenMisalignedHostPtrWhenCreatingAllocationThenSucceeds) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    void *ptr = ptrOffset(ptrT, 128);

    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(ptr, alloc->getUnderlyingBuffer());

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(ptrT, reinterpret_cast<void *>(bo->peekAddress()));

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptrT);
}

TEST_F(DrmMemoryManagerTest, GivenHostPtrUserptrWhenCreatingAllocationThenFails) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_res = -1;

    void *ptrT = ::alignedMalloc(1024, 4096);
    ASSERT_NE(nullptr, ptrT);

    auto alloc = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, 1024}, ptrT);
    EXPECT_EQ(nullptr, alloc);

    ::alignedFree(ptrT);
    mock->ioctl_res = 0;
}

TEST_F(DrmMemoryManagerTest, givenDrmAllocationWhenHandleFenceCompletionThenCallBufferObjectWait) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.contextDestroy = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024});
    memoryManager->handleFenceCompletion(allocation);
    mock->testIoctls();

    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 2;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerTest2, givenDrmMemoryManagerWhengetSystemSharedMemoryIsCalledThenContextGetParamIsCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        mock->getContextParamRetValue = 16 * MemoryConstants::gigaByte;
        uint64_t mem = memoryManager->getSystemSharedMemory(i);
        mock->ioctl_expected.contextGetParam = 1;
        EXPECT_EQ(mock->recordedGetContextParam.param, static_cast<__u64>(I915_CONTEXT_PARAM_GTT_SIZE));
        EXPECT_GT(mem, 0u);

        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
    }
}

TEST_F(DrmMemoryManagerTest, GivenBitnessWhenGettingMaxApplicationAddressThenCorrectValueIsReturned) {
    uint64_t maxAddr = memoryManager->getMaxApplicationAddress();
    if constexpr (is64bit) {
        EXPECT_EQ(maxAddr, MemoryConstants::max64BitAppAddress);
    } else {
        EXPECT_EQ(maxAddr, MemoryConstants::max32BitAppAddress);
    }
}

TEST(DrmMemoryManagerTest2, WhenGetMinimumSystemSharedMemoryThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        auto hostMemorySize = MemoryConstants::pageSize * (uint64_t)(sysconf(_SC_PHYS_PAGES));
        // gpuMemSize < hostMemSize
        auto gpuMemorySize = hostMemorySize - 1u;

        mock->ioctl_expected.contextGetParam = 1;
        mock->getContextParamRetValue = gpuMemorySize;

        uint64_t systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);

        EXPECT_EQ(gpuMemorySize, systemSharedMemorySize);
        mock->ioctl_expected.contextDestroy = 0;
        mock->ioctl_expected.contextCreate = 0;
        mock->testIoctls();

        // gpuMemSize > hostMemSize
        gpuMemorySize = hostMemorySize + 1u;
        mock->getContextParamRetValue = gpuMemorySize;
        systemSharedMemorySize = memoryManager->getSystemSharedMemory(i);
        mock->ioctl_expected.contextGetParam = 2;
        EXPECT_EQ(hostMemorySize, systemSharedMemorySize);
        mock->testIoctls();

        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset();
    }
}

TEST_F(DrmMemoryManagerTest, GivenBoWaitFailureThenExpectThrow) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    BufferObject *bo = memoryManager->allocUserptr(0, (size_t)1024, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);
    mock->ioctl_res = -EIO;
    EXPECT_THROW(bo->wait(-1), std::exception);
    mock->ioctl_res = 1;

    memoryManager->unreference(bo, false);
    mock->ioctl_res = 0;
}

TEST_F(DrmMemoryManagerTest, WhenNullOsHandleStorageAskedForPopulationThenFilledPointerIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    memoryManager->populateOsHandles(storage, rootDeviceIndex);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesReturnsInvalidHostPointerError) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 3;

    MemoryManager::AllocationStatus result = memoryManager->populateOsHandles(storage, rootDeviceIndex);

    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenReadOnlyPointerCausesPinningFailWithEfaultThenAlocateMemoryForNonSvmHostPtrReturnsNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }

    mock->reset();
    size_t dummySize = 13u;

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 3;
    mock->ioctl_expected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = dummySize;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto gfxPartition = memoryManager->getGfxPartition(device->getRootDeviceIndex());

    auto allocatedPointer = gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, dummySize);
    gfxPartition->freeGpuAddressRange(allocatedPointer, dummySize);

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(nullptr, allocation);
    mock->testIoctls();
    mock->ioctl_res_ext = &mock->NONE;

    // make sure that partition is free
    size_t dummySize2 = 13u;
    auto allocatedPointer2 = gfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, dummySize2);
    EXPECT_EQ(allocatedPointer2, allocatedPointer);
    gfxPartition->freeGpuAddressRange(allocatedPointer, dummySize2);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenHostPtrDoesntCausePinningFailThenAlocateMemoryForNonSvmHostPtrReturnsAllocation) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = 0;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 3;

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);

    mock->testIoctls();
    mock->ioctl_res_ext = &mock->NONE;

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenAllocatingMemoryForNonSvmHostPtrThenAllocatedCorrectly) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = 0;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 3;

    AllocationData allocationData;
    allocationData.size = 13u;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocation->getGpuAddress() - allocation->getAllocationOffset(), mock->execBufferBufferObjects.getOffset());

    mock->testIoctls();
    mock->ioctl_res_ext = &mock->NONE;

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenPinningFailWithErrorDifferentThanEfaultThenPopulateOsHandlesReturnsError) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {1, -1};
    ioctlResExt.no.push_back(2);
    ioctlResExt.no.push_back(3);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = ENOMEM;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 3;

    MemoryManager::AllocationStatus result = memoryManager->populateOsHandles(storage, rootDeviceIndex);

    EXPECT_EQ(MemoryManager::AllocationStatus::Error, result);
    mock->testIoctls();

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, GivenNoInputsWhenOsHandleIsCreatedThenAllBoHandlesAreInitializedAsNullPtrs) {
    OsHandleLinux boHandle;
    EXPECT_EQ(nullptr, boHandle.bo);

    std::unique_ptr<OsHandleLinux> boHandle2(new OsHandleLinux);
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

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerWhenCreatingGraphicsAllocation64kbThenNullPtrIsReturned) {
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, givenRequiresStandardHeapThenStandardHeapIsAcquired) {
    const uint32_t rootDeviceIndex = 0;
    size_t bufferSize = 4096u;
    uint64_t range = memoryManager->acquireGpuRange(bufferSize, rootDeviceIndex, HeapIndex::HEAP_STANDARD);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD)), range);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD)), range);
}

TEST_F(DrmMemoryManagerTest, givenRequiresStandard2MBHeapThenStandard2MBHeapIsAcquired) {
    const uint32_t rootDeviceIndex = 0;
    size_t bufferSize = 4096u;
    uint64_t range = memoryManager->acquireGpuRange(bufferSize, rootDeviceIndex, HeapIndex::HEAP_STANDARD2MB);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD2MB)), range);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD2MB)), range);
}

TEST_F(DrmMemoryManagerTest, GivenShareableEnabledWhenAskedToCreateGraphicsAllocationThenValidAllocationIsReturnedAndStandard64KBHeapIsUsed) {
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemClose = 1;

    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.shareable = true;
    auto allocation = memoryManager->allocateMemoryByKMD(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getGpuAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(allocation->getRootDeviceIndex())->getHeapBase(HeapIndex::HEAP_STANDARD64KB)), allocation->getGpuAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(allocation->getRootDeviceIndex())->getHeapLimit(HeapIndex::HEAP_STANDARD64KB)), allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllocationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    mock->ioctl_expected.gemUserptr = 3;
    mock->ioctl_expected.gemWait = 3;
    mock->ioctl_expected.gemClose = 3;

    auto ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, size}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    if (memoryManager->isLimitedRange(rootDeviceIndex)) {
        ASSERT_EQ(6u, hostPtrManager->getFragmentCount());
    } else {
        ASSERT_EQ(3u, hostPtrManager->getFragmentCount());
    }

    auto reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        auto osHandle = static_cast<OsHandleLinux *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage);
        ASSERT_NE(nullptr, osHandle->bo);
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize, osHandle->bo->peekSize());
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr, reinterpret_cast<void *>(osHandle->bo->peekAddress()));
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);

    if (memoryManager->isLimitedRange(rootDeviceIndex)) {
        EXPECT_EQ(3u, hostPtrManager->getFragmentCount());
    } else {
        EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    }
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationThen32BitDrmAllocationIsBeingReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    auto address64bit = allocation->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(allocation->is32BitAllocation());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), allocation->isAllocatedInLocalMemoryPool())), allocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWhenLimitedAllocationEnabledThen32BitDrmAllocationWithGpuAddrDifferentFromCpuAddrIsBeingReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_GE(allocation->getUnderlyingBufferSize(), size);

    EXPECT_NE((uint64_t)allocation->getGpuAddress(), (uint64_t)allocation->getUnderlyingBuffer());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenLimitedRangeAllocatorSetThenHeapSizeAndEndAddrCorrectlySetForGivenGpuRange) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    uint64_t sizeBig = 4 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    auto gpuAddressLimitedRange = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_STANDARD, sizeBig);
    EXPECT_LT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange);
    EXPECT_GT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange + sizeBig);
    EXPECT_EQ(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapMinimalAddress(HeapIndex::HEAP_STANDARD), gpuAddressLimitedRange);

    auto gpuInternal32BitAlloc = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, sizeBig);
    EXPECT_LT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY), gpuInternal32BitAlloc);
    EXPECT_GT(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY), gpuInternal32BitAlloc + sizeBig);
    EXPECT_EQ(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapMinimalAddress(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY), gpuInternal32BitAlloc);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForAllocationWithAlignmentAndLimitedRangeAllocatorSetAndAcquireGpuRangeFailsThenNullIsReturned) {
    mock->ioctl_expected.gemUserptr = 0;
    mock->ioctl_expected.gemClose = 0;

    AllocationData allocationData;

    // emulate GPU address space exhaust
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::HEAP_STANDARD, 0x0, 0x10000);

    // set size to something bigger than allowed space
    allocationData.size = 0x20000;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationWithHostPtrAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    void *hostPtr = reinterpret_cast<void *>(0x1000);
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, hostPtr, AllocationType::BUFFER);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedFor32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->setForce32BitAllocations(true);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::BUFFER);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternal32BitAllocationAndAllocUserptrFailsThenFails) {
    mock->ioctl_expected.gemUserptr = 1;

    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    auto size = 10u;
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, AllocationType::INTERNAL_HEAP);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, GivenExhaustedInternalHeapWhenAllocate32BitIsCalledThenNullIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.Force32bitAddressing.set(true);
    memoryManager->setForce32BitAllocations(true);

    size_t size = MemoryConstants::pageSize64k;
    auto alloc = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, size);
    EXPECT_NE(0llu, alloc);

    size_t allocationSize = 4 * GB;
    auto graphicsAllocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, allocationSize, nullptr, AllocationType::INTERNAL_HEAP);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenSetForceUserptrAlignmentWhenGetUserptrAlignmentThenForcedValueIsReturned) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.ForceUserptrAlignment.set(123456);

    EXPECT_EQ(123456 * MemoryConstants::kiloByte, memoryManager->getUserptrAlignment());
}

TEST_F(DrmMemoryManagerTest, whenGetUserptrAlignmentThenDefaultValueIsReturned) {
    EXPECT_EQ(MemoryConstants::allocationAlignment, memoryManager->getUserptrAlignment());
}

TEST_F(DrmMemoryManagerTest, GivenMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenProperIoctlsAreCalledAndUnmapSizeIsNonZero) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D; // tiled
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto imageGraphicsAllocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_NE(0u, imageGraphicsAllocation->getGpuAddress());
    EXPECT_EQ(nullptr, imageGraphicsAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imgInfo.size, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(imgInfo.rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    memoryManager->freeGraphicsMemory(imageGraphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenCreateGraphicsAllocationFromSharedHandleIsCalledThenAcquireGpuAddressFromStandardHeap64KB) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, graphicsAllocation->getMemoryPool());
    EXPECT_EQ(this->mock->inputFd, static_cast<int32_t>(handle));

    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD2MB)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD2MB)), gpuAddress);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(this->mock->outputHandle, static_cast<uint32_t>(bo->peekHandle()));
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());
    EXPECT_EQ(alignUp(size, 2 * MemoryConstants::megaByte), bo->peekUnmapSize());

    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledAndRootDeviceIndexIsSpecifiedThenGraphicsAllocationIsReturnedWithCorrectRootDeviceIndex) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, false, 0u);
    ASSERT_TRUE(properties.subDevicesBitfield.none());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(rootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    EXPECT_EQ(handle, graphicsAllocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndOsHandleWhenAllocationFailsThenReturnNullPtr) {
    osHandle handle = 1u;

    InjectedFunction method = [this, &handle](size_t failureIndex) {
        AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);

        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
        if (MemoryManagement::nonfailingAllocation == failureIndex) {
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

        AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);

        graphicsAllocations[i] = memoryManager->createGraphicsAllocationFromSharedHandle(handles[i], properties, false, false, true);
        // Clang-tidy false positive WA
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
        EXPECT_NE(0llu, bo->peekAddress());
        EXPECT_EQ(expectedRefCount, bo->getRefCount());
        EXPECT_EQ(size, bo->peekSize());

        EXPECT_EQ(handles[i], graphicsAllocations[i]->peekSharedHandle());
    }

    for (const auto &it : graphicsAllocations) {
        // Clang-tidy false positive WA
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

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, true, false, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_TRUE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, lseekCalledCount);
    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getExternalHeapBaseAddress(graphicsAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool())), drmAllocation->getGpuBaseAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, given32BitAddressingWhenBufferFromSharedHandleIsCreatedAndDoesntRequireBitnessThenItIsNot32BitAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->setForce32BitAllocations(true);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, lseekCalledCount);

    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenBufferFromSharedHandleIsCreatedThenItIsLimitedRangeAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);

    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());
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
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, true, false, true);
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    EXPECT_FALSE(graphicsAllocation->is32BitAllocation());
    EXPECT_EQ(1, lseekCalledCount);
    EXPECT_EQ(0llu, drmAllocation->getGpuBaseAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenSharedHandleWhenAllocationIsCreatedAndIoctlPrimeFdToHandleFailsThenNullPtrIsReturned) {
    mock->ioctl_expected.primeFdToHandle = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &this->ioctlResExt;

    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    EXPECT_EQ(nullptr, graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatShareTheSameBufferObjectWhenTheyAreMadeResidentThenOnlyOneBoIsPassedToExec) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false, true);
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false, true);

    auto currentResidentSize = testedCsr->getResidencyAllocations().size();
    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);

    EXPECT_EQ(currentResidentSize + 2, testedCsr->getResidencyAllocations().size());
    currentResidentSize = testedCsr->getResidencyAllocations().size();

    testedCsr->processResidency(testedCsr->getResidencyAllocations(), 0u);

    EXPECT_EQ(currentResidentSize - 1, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmMemoryManagerTest, givenTwoGraphicsAllocationsThatDoesnShareTheSameBufferObjectWhenTheyAreMadeResidentThenTwoBoIsPassedToExec) {
    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    mock->ioctl_expected.primeFdToHandle = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemWait = 2;

    osHandle sharedHandle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false, true);
    mock->outputHandle++;
    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false, true);

    auto currentResidentSize = testedCsr->getResidencyAllocations().size();
    testedCsr->makeResident(*graphicsAllocation);
    testedCsr->makeResident(*graphicsAllocation2);

    EXPECT_EQ(currentResidentSize + 2u, testedCsr->getResidencyAllocations().size());

    testedCsr->processResidency(testedCsr->getResidencyAllocations(), 0u);

    EXPECT_EQ(currentResidentSize + 2u, testedCsr->residency.size());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromNTHandle(reinterpret_cast<void *>(1), 0, AllocationType::SHARED_IMAGE);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledThenReturnPtr) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemSetDomain = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
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

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_EQ(allocation->getUnderlyingBuffer(), ptr);

    // check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ(0u, mock->setDomainWriteDomain);

    memoryManager->unlockResource(allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationWithoutCpuPtrThenReturnLockedPtrAndSetCpuDomain) {
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemMmapOffset = 1;
    mock->ioctl_expected.gemSetDomain = 0;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.size = 4096u;
    imgInfo.rowPitch = 512u;

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    // check DRM_IOCTL_I915_GEM_MMAP_OFFSET input params
    EXPECT_EQ((uint32_t)drmAllocation->getBO()->peekHandle(), mock->mmapOffsetHandle);
    EXPECT_EQ(0u, mock->mmapOffsetPad);
    EXPECT_EQ(static_cast<uint32_t>(I915_MMAP_OFFSET_WC), mock->mmapOffsetFlags);

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
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, nullptr, nullptr, 0, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenLockUnlockIsCalledButFailsOnIoctlMmapThenReturnNullPtr) {
    mock->ioctl_expected.gemMmapOffset = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    BufferObject bo(mock, 3, 1, 0, 1);
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenUnlockResourceIsCalledOnAllocationInLocalMemoryThenRedirectToUnlockResourceInLocalMemory) {
    struct DrmMemoryManagerToTestUnlockResource : public DrmMemoryManager {
        using DrmMemoryManager::unlockResourceImpl;
        DrmMemoryManagerToTestUnlockResource(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
            : DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        }
        void unlockBufferObject(BufferObject *bo) override {
            unlockResourceInLocalMemoryImplParam.bo = bo;
            unlockResourceInLocalMemoryImplParam.called = true;
        }
        struct unlockResourceInLocalMemoryImplParamType {
            BufferObject *bo = nullptr;
            bool called = false;
        } unlockResourceInLocalMemoryImplParam;
    };

    DrmMemoryManagerToTestUnlockResource drmMemoryManager(*executionEnvironment, true, MemoryConstants::pageSize);

    DrmMockCustom drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 3, 1, 0, 1) {}
    };
    auto bo = new BufferObjectMock(&drmMock);
    auto drmAllocation = new DrmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::LocalMemory);

    drmMemoryManager.unlockResourceImpl(*drmAllocation);
    EXPECT_TRUE(drmMemoryManager.unlockResourceInLocalMemoryImplParam.called);
    EXPECT_EQ(bo, drmMemoryManager.unlockResourceInLocalMemoryImplParam.bo);

    drmMemoryManager.freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationWithoutBufferObjectThenReturnFalse) {
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, nullptr, nullptr, 0, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    EXPECT_EQ(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledButFailsOnIoctlSetDomainThenReturnFalse) {
    mock->ioctl_expected.gemSetDomain = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    DrmMockCustom drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 3, 1, 0, 1) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, false);
    EXPECT_FALSE(success);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetDomainCpuIsCalledOnAllocationThenReturnSetWriteDomain) {
    mock->ioctl_expected.gemSetDomain = 1;

    DrmMockCustom drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    struct BufferObjectMock : public BufferObject {
        BufferObjectMock(Drm *drm) : BufferObject(drm, 3, 1, 0, 1) {}
    };
    BufferObjectMock bo(&drmMock);
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, &bo, nullptr, 0u, static_cast<osHandle>(0u), MemoryPool::MemoryNull);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto success = memoryManager->setDomainCpu(drmAllocation, true);
    EXPECT_TRUE(success);

    // check DRM_IOCTL_I915_GEM_SET_DOMAIN input params
    EXPECT_EQ((uint32_t)drmAllocation.getBO()->peekHandle(), mock->setDomainHandle);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainReadDomains);
    EXPECT_EQ((uint32_t)I915_GEM_DOMAIN_CPU, mock->setDomainWriteDomain);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 123, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
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
    auto ptr = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_EXTERNAL, allocationSize);
    size_t smallAllocationSize = MemoryConstants::pageSize;
    memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_EXTERNAL, smallAllocationSize);

    // now free first allocation , this will move it to chunks
    memoryManager->getGfxPartition(rootDeviceIndex)->heapFree(HeapIndex::HEAP_EXTERNAL, ptr, allocationSize);

    // now ask for 3 pages, this will give ptr from chunks
    size_t pages3size = 3 * MemoryConstants::pageSize;

    void *hostPtr = reinterpret_cast<void *>(0x1000);
    DrmAllocation *graphicsAlloaction = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, pages3size, hostPtr, AllocationType::BUFFER);

    auto bo = graphicsAlloaction->getBO();
    EXPECT_EQ(pages3size, bo->peekSize());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_EQ(gmmHelper->canonize(ptr), graphicsAlloaction->getGpuAddress());

    memoryManager->freeGraphicsMemory(graphicsAlloaction);
}

TEST_F(DrmMemoryManagerTest, givenSharedAllocationWithSmallerThenRealSizeWhenCreateIsCalledThenRealSizeIsUsed) {
    unsigned int realSize = 64 * 1024;
    lseekReturn = realSize;
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    osHandle sharedHandle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(sharedHandle, properties, false, false, true);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(realSize, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)sharedHandle);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(realSize, bo->peekSize());
    EXPECT_EQ(1, lseekCalledCount);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    ASSERT_NE(nullptr, drmAllocation->getDriverAllocatedCpuPtr());
    EXPECT_EQ(drmAllocation->getDriverAllocatedCpuPtr(), drmAllocation->getUnderlyingBuffer());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

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
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::BUFFER));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    memoryManager->freeGraphicsMemory(drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenLimitedRangeAllocatorWhenAskedForInternalAllocationWithNoPointerAndHugeBufferSizeThenAllocationFromInternalHeapFailed) {
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    auto bufferSize = 128 * MemoryConstants::megaByte + 4 * MemoryConstants::pageSize;
    void *ptr = nullptr;
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::INTERNAL_HEAP));
    ASSERT_EQ(nullptr, drmAllocation);
}

TEST_F(DrmMemoryManagerTest, givenMemoryManagerWhenAskedForInternalAllocationWithPointerThenAllocationFromInternalHeapIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    auto bufferSize = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x100000);
    auto drmAllocation = static_cast<DrmAllocation *>(memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, bufferSize, ptr, AllocationType::INTERNAL_HEAP));
    ASSERT_NE(nullptr, drmAllocation);

    EXPECT_NE(nullptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(ptr, drmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(bufferSize, drmAllocation->getUnderlyingBufferSize());

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto gpuPtr = drmAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    auto heapBase = gmmHelper->canonize(memoryManager->getInternalHeapBaseAddress(drmAllocation->getRootDeviceIndex(), drmAllocation->isAllocatedInLocalMemoryPool()));
    auto heapSize = 4 * GB;

    EXPECT_GE(gpuPtr, heapBase);
    EXPECT_LE(gpuPtr, heapBase + heapSize);

    EXPECT_EQ(drmAllocation->getGpuBaseAddress(), heapBase);

    memoryManager->freeGraphicsMemory(drmAllocation);
}

using DrmMemoryManagerUSMHostAllocationTests = Test<DrmMemoryManagerFixture>;

TEST_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToFalseThenNewHostPointerIsUsedAndAllocationIsCreatedSuccesfully) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16384;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);
    memoryManager->freeGraphicsMemoryImpl(alloc);
}

TEST_F(DrmMemoryManagerUSMHostAllocationTests, givenCallToAllocateGraphicsMemoryWithAlignmentWithIsHostUsmAllocationSetToTrueThenGpuAddressIsNotFromGfxPartition) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 16384;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::SVM_CPU;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(reinterpret_cast<uint64_t>(alloc->getUnderlyingBuffer()), alloc->getGpuAddress());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

TEST_F(DrmMemoryManagerUSMHostAllocationTests, givenMmapPtrWhenFreeGraphicsMemoryImplThenPtrIsDeallocated) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    const size_t size = 16384;
    AllocationData allocationData;
    allocationData.size = size;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);

    auto ptr = memoryManager->mmapFunction(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    static_cast<DrmAllocation *>(alloc)->setMmapPtr(ptr);
    static_cast<DrmAllocation *>(alloc)->setMmapSize(size);

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

TEST_F(DrmMemoryManagerUSMHostAllocationTests,
       givenCallToallocateGraphicsMemoryWithAlignmentWithisHostUSMAllocationSetToTrueThenTheExistingHostPointerIsUsedAndAllocationIsCreatedSuccesfully) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    AllocationData allocationData;

    size_t allocSize = 16384;
    void *hostPtr = alignedMalloc(allocSize, 0);

    allocationData.size = allocSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtr;
    NEO::GraphicsAllocation *alloc = memoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(hostPtr, alloc->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemoryImpl(alloc);
    alignedFree(hostPtr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, memoryManager->getAlignedMallocRestrictions());
}

TEST_F(DrmMemoryManagerBasic, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledGemCloseWorkerWhenMemoryManagerIsCreatedThenNoGemCloseWorker) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableGemCloseWorker.set(0u);

    TestedDrmMemoryManager memoryManager(true, true, true, executionEnvironment);

    EXPECT_EQ(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledGemCloseWorkerWhenMemoryManagerIsCreatedThenGemCloseWorker) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableGemCloseWorker.set(1u);

    TestedDrmMemoryManager memoryManager(true, true, true, executionEnvironment);

    EXPECT_NE(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenDefaultGemCloseWorkerWhenMemoryManagerIsCreatedThenGemCloseWorker) {
    MemoryManagerCreate<DrmMemoryManager> memoryManager(false, false, gemCloseWorkerMode::gemCloseWorkerActive, false, false, executionEnvironment);

    EXPECT_NE(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(true);
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.EnableDeferredDeleter.set(false);
    DrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenWorkerToCloseWhenCommonCleanupIsCalledThenClosingIsBlocking) {
    MockDrmMemoryManager memoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    memoryManager.gemCloseWorker.reset(new MockDrmGemCloseWorker(memoryManager));
    auto pWorker = static_cast<MockDrmGemCloseWorker *>(memoryManager.gemCloseWorker.get());

    memoryManager.commonCleanup();
    EXPECT_TRUE(pWorker->wasBlocking);
}

TEST_F(DrmMemoryManagerBasic, givenDefaultDrmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenInternalHeapBaseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    auto heapBase = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY);
    EXPECT_EQ(heapBase, memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, true));
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
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationAndForcePinWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(
        false,
        false,
        true,
        executionEnvironment));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, false));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr);
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

    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, ptr, AllocationType::BUFFER);

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
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle, properties, false, false, true);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPinBBAllocationFailsThenUnrecoverableIsCalled) {
    this->mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    this->mock->reset();
    this->mock->ioctl_res = -1;
    this->mock->ioctl_expected.gemUserptr = 1;
    EXPECT_THROW(
        {
            std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false,
                                                                                             false,
                                                                                             true,
                                                                                             *executionEnvironment));
            EXPECT_NE(nullptr, memoryManager.get());
        },
        std::exception);
    this->mock->ioctl_res = 0;
    this->mock->ioctl_expected.contextDestroy = 0;
    this->mock->testIoctls();
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledThenHostMemoryIsValidated) {

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1; // for pinning - host memory validation

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;
    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledWithFirstFragmentAlreadyAllocatedThenNewBosAreValidated) {

    class PinBufferObject : public BufferObject {
      public:
        PinBufferObject(Drm *drm) : BufferObject(drm, 3, 1, 0, 1) {
        }

        int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
            for (size_t i = 0; i < numberOfBos; i++) {
                pinnedBoArray[i] = boToPin[i];
            }
            numberOfBosPinned = numberOfBos;
            return 0;
        }
        BufferObject *pinnedBoArray[5];
        size_t numberOfBosPinned;
    };

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    PinBufferObject *pinBB = new PinBufferObject(this->mock);
    memoryManager->injectPinBB(pinBB, rootDeviceIndex);

    mock->reset();
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 0; // pinning for host memory validation is mocked

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[1].osHandleStorage)->bo, pinBB->pinnedBoArray[0]);
    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[2].osHandleStorage)->bo, pinBB->pinnedBoArray[1]);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenValidateHostPtrMemoryEnabledWhenHostPtrAllocationIsCreatedWithoutForcingPinThenBufferObjectIsPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 2;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    size_t size = 10 * MB;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenValidHostPointerIsPassedToPopulateThenSuccessIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    auto result = memoryManager->populateOsHandles(storage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledWhenSmallAllocationIsCreatedThenBufferObjectIsPinned) {
    mock->ioctl_expected.gemUserptr = 2;  // 1 pinBB, 1 small allocation
    mock->ioctl_expected.execbuffer2 = 1; // pinning
    mock->ioctl_expected.gemWait = 1;     // in freeGraphicsAllocation
    mock->ioctl_expected.gemClose = 2;    // 1 pinBB, 1 small allocation

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning but pinning is used for host memory validation
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectGpuAddressWithinDeviceGpuAddressSpace) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    auto bo = memoryManager->pinBBs[rootDeviceIndex];

    ASSERT_NE(nullptr, bo);

    EXPECT_LT(bo->peekAddress(), defaultHwInfo->capabilityTable.gpuAddressSpace);
}

TEST_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectWrittenWithMIBBENDAndNOOP) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    EXPECT_NE(0ul, memoryManager->memoryForPinBBs.size());
    ASSERT_NE(nullptr, memoryManager->memoryForPinBBs[rootDeviceIndex]);

    uint32_t *buffer = reinterpret_cast<uint32_t *>(memoryManager->memoryForPinBBs[rootDeviceIndex]);
    uint32_t bbEnd = 0x05000000;
    EXPECT_EQ(bbEnd, buffer[0]);
    EXPECT_EQ(0ul, buffer[1]);
}

TEST_F(DrmMemoryManagerTest, givenForcePinAllowedAndNoPinBBInMemoryManagerWhenAllocationWithForcePinFlagTrueIsCreatedThenAllocationIsNotPinned) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_res = -1;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    mock->ioctl_res = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true));
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
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrThenObjectAlignedSizeIsUsedByAllocUserPtrWhenBiggerSizeAllocatedInHeap) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 4 * MB + 16 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x10000000);
    auto allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.hostPtr = reinterpret_cast<const void *>(0x20000000);
    auto allocation1 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    memoryManager->freeGraphicsMemory(allocation0);

    allocationData.size = 4 * MB + 12 * 1024;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x30000000);
    allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(static_cast<uint64_t>(allocation0->getBO()->peekSize()), 4 * MB + 12 * 1024);

    memoryManager->freeGraphicsMemory(allocation0);
    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledButAllocationFailedThenNullPtrReturned) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    allocationData.size = 64 * GB;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrFailsThenNullPtrReturnedAndAllocationIsNotRegistered) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);
    MockAllocationProperties properties(0u, 64 * GB);

    auto ptr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryInPreferredPool(properties, ptr));
    EXPECT_EQ(memoryManager->getSysMemAllocs().size(), 0u);
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
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 4 * 1024;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationDisabledWhenAllocationIsCreatedThenBufferObjectIsNotPinned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, false, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 10 * MB; // bigger than threshold
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesMarksFragmentsToFree) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
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

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesDoesNotStoreTheFragments) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctl_res_ext = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[1].cpuPtr, rootDeviceIndex}));
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[2].cpuPtr, rootDeviceIndex}));

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctl_res_ext = &mock->NONE;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenPopulateOsHandlesSucceedsThenFragmentIsStoredInHostPtrManager) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager->registeredEngines) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    EXPECT_NE(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[0].cpuPtr, device->getRootDeviceIndex()}));

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCleanOsHandlesDeletesHandleDataThenOsHandleStorageAndResidencyIsSetToNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);
    auto maxOsContextCount = 1u;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[1].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[1].fragmentSize = 4096;

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].residency);
    }
}

TEST_F(DrmMemoryManagerBasic, ifLimitedRangeAllocatorAvailableWhenAskedForAllocationThenLimitedRangePointerIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(0xFFFFFFFFF);

    size_t size = 100u;
    auto ptr = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(HeapIndex::HEAP_STANDARD, size);
    auto address64bit = ptrDiff(ptr, memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD));

    EXPECT_LT(address64bit, defaultHwInfo->capabilityTable.gpuAddressSpace);

    EXPECT_LT(0u, address64bit);

    memoryManager->getGfxPartition(rootDeviceIndex)->heapFree(HeapIndex::HEAP_STANDARD, ptr, size);
}

TEST_F(DrmMemoryManagerBasic, givenSpecificAddressSpaceWhenInitializingMemoryManagerThenSetCorrectHeaps) {
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->capabilityTable.gpuAddressSpace = maxNBitValue(48);
    TestedDrmMemoryManager memoryManager(false, false, false, executionEnvironment);

    auto gfxPartition = memoryManager.getGfxPartition(rootDeviceIndex);
    auto limit = gfxPartition->getHeapLimit(HeapIndex::HEAP_SVM);

    EXPECT_EQ(maxNBitValue(48 - 1), limit);
}

TEST_F(DrmMemoryManagerBasic, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableHostPtrTracking.set(false);

    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->forceLimitedRangeAllocator(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
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
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType types[] = {AllocationType::IMAGE,
                              AllocationType::SHARED_RESOURCE_COPY};

    for (auto type : types) {
        allocData.type = type;
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
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerTest, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitDrmAllocationWithFrontWindowGpuVaIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, size,
                                         NEO::AllocationType::DEBUG_MODULE_AREA,
                                         false,
                                         device->getDeviceBitfield()};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    HeapIndex heap = HeapAssigner::mapInternalWindowIndex(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool()));
    EXPECT_TRUE(heap == HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW || heap == HeapIndex::HEAP_INTERNAL_FRONT_WINDOW);

    auto gmmHelper = device->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(heap));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    auto internalHeapBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(internalHeapBase, moduleDebugArea->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

struct DrmAllocationTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmAllocationTests, givenAllocationTypeWhenPassedToDrmAllocationConstructorThenAllocationTypeIsStored) {
    DrmAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::MemoryNull};
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, allocation.getAllocationType());

    DrmAllocation allocation2{0, AllocationType::UNKNOWN, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::MemoryNull};
    EXPECT_EQ(AllocationType::UNKNOWN, allocation2.getAllocationType());
}

TEST_F(DrmAllocationTests, givenMemoryPoolWhenPassedToDrmAllocationConstructorThenMemoryPoolIsStored) {
    DrmAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::System64KBPages};
    EXPECT_EQ(MemoryPool::System64KBPages, allocation.getMemoryPool());

    DrmAllocation allocation2{0, AllocationType::UNKNOWN, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::SystemCpuInaccessible};
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, allocation2.getMemoryPool());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    constexpr size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size, 0u);
    EXPECT_EQ(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenObtainFdFromHandleIsCalledThenProperFdHandleIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = 1337;
        mock->ioctl_expected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(mock->inputHandle, static_cast<uint32_t>(boHandle));
        EXPECT_EQ(mock->inputFlags, DRM_CLOEXEC | DRM_RDWR);
        EXPECT_EQ(1337, fdHandle);
    }
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenFailingToObtainFdFromHandleThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = -1;
        mock->ioctl_res = -1;
        mock->ioctl_expected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(-1, fdHandle);
    }
}

TEST_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedThenAllocateMemoryAndReserveGpuVa) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::SVM_CPU;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(AllocationType::SVM_CPU, allocation->getAllocationType());

    EXPECT_EQ(allocationData.size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());

    EXPECT_NE(0llu, allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    auto bo = allocation->getBO();
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(0llu, bo->peekAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD)), bo->peekAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD)), bo->peekAddress());

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), alignUp(allocation->getReservedAddressPtr(), allocationData.alignment));
    EXPECT_EQ(alignUp(allocationData.size, allocationData.alignment) + allocationData.alignment, allocation->getReservedAddressSize());

    EXPECT_GT(allocation->getReservedAddressSize(), bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedButFailsToReserveGpuVaThenNullAllocationIsReturned) {
    mock->ioctl_expected.gemUserptr = 0;
    mock->ioctl_expected.gemWait = 0;
    mock->ioctl_expected.gemClose = 0;

    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::HEAP_STANDARD, 0, 0);

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::SVM_CPU;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndReleaseGpuRangeIsCalledThenGpuAddressIsDecanonized) {
    constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * GB) : 0;
    auto hwInfo = defaultHwInfo.get();
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->init(hwInfo->capabilityTable.gpuAddressSpace, reservedCpuAddressRangeSize, 0, 1);
    auto size = 2 * MemoryConstants::megaByte;
    auto gpuAddress = mockGfxPartition->heapAllocate(HeapIndex::HEAP_STANDARD, size);
    auto gmmHelper = device->getGmmHelper();
    auto gpuAddressCanonized = gmmHelper->canonize(gpuAddress);
    EXPECT_LE(gpuAddress, gpuAddressCanonized);

    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    memoryManager->releaseGpuRange(reinterpret_cast<void *>(gpuAddressCanonized), size, 0);

    auto mockGfxPartitionBasic = std::make_unique<MockGfxPartitionBasic>();
    memoryManager->overrideGfxPartition(mockGfxPartitionBasic.release());
}

TEST(DrmMemoryManagerFreeGraphicsMemoryCallSequenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledThenUnreferenceBufferObjectIsCalledFirstWithSynchronousDestroySetToTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield};
    auto allocation = memoryManger.allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(allocation, nullptr);

    memoryManger.freeGraphicsMemory(allocation);

    EXPECT_EQ(EngineLimits::maxHandleCount, memoryManger.unreferenceCalled);
    for (size_t i = 0; i < EngineLimits::maxHandleCount; ++i) {
        EXPECT_TRUE(memoryManger.unreferenceParamsPassed[i].synchronousDestroy);
    }
    EXPECT_EQ(1u, memoryManger.releaseGpuRangeCalled);
    EXPECT_EQ(1u, memoryManger.alignedFreeWrapperCalled);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithNoReuseSharedAllocationThenAllocationsSuccedAndAddressesAreDifferent) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    osHandle handle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(handle, properties, false, false, false);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManger.createGraphicsAllocationFromSharedHandle(handle, properties, false, false, false);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_NE(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManger.freeGraphicsMemory(allocation2);
    memoryManger.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithReuseSharedAllocationThenAllocationsSuccedAndAddressesAreTheSame) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    osHandle handle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManger.createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_EQ(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManger.freeGraphicsMemory(allocation2);
    memoryManger.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledForSharedAllocationThenUnreferenceBufferObjectIsCalledWithSynchronousDestroySetToFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManger(executionEnvironment);

    osHandle handle = 1u;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
    auto allocation = memoryManger.createGraphicsAllocationFromSharedHandle(handle, properties, false, false, true);
    ASSERT_NE(nullptr, allocation);

    memoryManger.freeGraphicsMemory(allocation);

    EXPECT_EQ(1 + EngineLimits::maxHandleCount - 1, memoryManger.unreferenceCalled);
    EXPECT_FALSE(memoryManger.unreferenceParamsPassed[0].synchronousDestroy);
    for (size_t i = 1; i < EngineLimits::maxHandleCount - 1; ++i) {
        EXPECT_TRUE(memoryManger.unreferenceParamsPassed[i].synchronousDestroy);
    }
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldBeRegisteredThenBoHasBindExtHandleAdded) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        drm.classHandles.push_back(i);
    }

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::DEBUG_CONTEXT_SAVE_AREA, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::ContextSaveArea, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::MaxSize;

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::DEBUG_SBA_TRACKING_BUFFER, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::SbaTrackingBuffer, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::MaxSize;

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::Isa, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::MaxSize;

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::DEBUG_MODULE_AREA, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::ModuleHeapDebugArea, drm.registeredClass);
    }

    drm.registeredClass = DrmResourceClass::MaxSize;

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::BUFFER_HOST_MEMORY, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
        EXPECT_EQ(DrmResourceClass::MaxSize, drm.registeredClass);
    }
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldNotBeRegisteredThenNoBindHandleCreated) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.registeredClass = DrmResourceClass::MaxSize;

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        drm.classHandles.push_back(i);
    }

    {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(AllocationType::KERNEL_ISA_INTERNAL, MemoryPool::System4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
    }
    EXPECT_EQ(DrmResourceClass::MaxSize, drm.registeredClass);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationNotEnabledWhenRegisteringBindExtHandleThenHandleIsNotAddedToBo) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_EQ(0u, drm.classHandles.size());

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::DEBUG_CONTEXT_SAVE_AREA, MemoryPool::System4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(0u, bo.bindExtHandles.size());
    EXPECT_EQ(DrmResourceClass::MaxSize, drm.registeredClass);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeAndDisabledRegistrationInDrmWhenAllocatingThenRegisterBoBindExtHandleIsNotCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    EXPECT_FALSE(mockDrm->resourceRegistrationEnabled());

    mockDrm->registeredDataSize = 0;

    MockDrmAllocation allocation(AllocationType::DEBUG_CONTEXT_SAVE_AREA, MemoryPool::System4KBPages);

    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_FALSE(allocation.registerBOBindExtHandleCalled);
    EXPECT_EQ(DrmResourceClass::MaxSize, mockDrm->registeredClass);
}

TEST(DrmMemoryManager, givenResourceRegistrationEnabledAndAllocTypeToCaptureWhenRegisteringAllocationInOsThenItIsMarkedForCapture) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    // mock resource registration enabling by storing class handles
    mockDrm->classHandles.push_back(1);

    MockBufferObject bo(mockDrm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::SCRATCH_SURFACE, MemoryPool::System4KBPages);
    allocation.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_TRUE(allocation.markedForCapture);

    MockDrmAllocation allocation2(AllocationType::BUFFER, MemoryPool::System4KBPages);
    allocation2.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation2);

    EXPECT_FALSE(allocation2.markedForCapture);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenAllocatingThenAllocationIsRegistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        mockDrm->classHandles.push_back(i);
    }

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_EQ(DrmResourceClass::SbaTrackingBuffer, mockDrm->registeredClass);

    EXPECT_EQ(sizeof(uint64_t), mockDrm->registeredDataSize);
    uint64_t *data = reinterpret_cast<uint64_t *>(mockDrm->registeredData);
    EXPECT_EQ(properties.gpuAddress, *data);

    memoryManager->freeGraphicsMemory(sbaAllocation);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenFreeingThenRegisteredHandlesAreUnregistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        mockDrm->classHandles.push_back(i);
    }

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(0u, mockDrm->unregisterCalledCount);

    memoryManager->freeGraphicsMemory(sbaAllocation);

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, mockDrm->unregisteredHandle);
    EXPECT_EQ(1u, mockDrm->unregisterCalledCount);
}

TEST(DrmMemoryManager, givenEnabledResourceRegistrationWhenSshIsAllocatedThenItIsMarkedForCapture) {
    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mockDrm, 0u);
    executionEnvironment->memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        mockDrm->classHandles.push_back(i);
    }
    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

    CommandContainer cmdContainer;
    cmdContainer.initialize(device.get(), nullptr, true);

    auto *ssh = cmdContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto bo = static_cast<DrmAllocation *>(ssh->getGraphicsAllocation())->getBO();

    ASSERT_NE(nullptr, bo);
    EXPECT_TRUE(bo->isMarkedForCapture());
}

TEST(DrmMemoryManager, givenNullBoWhenRegisteringBindExtHandleThenEarlyReturn) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = std::make_unique<DrmMockResources>(*executionEnvironment->rootDeviceEnvironments[0]);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        mockDrm->classHandles.push_back(i);
    }

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    MockDrmAllocation gfxAllocation(AllocationType::DEBUG_SBA_TRACKING_BUFFER, MemoryPool::MemoryNull);

    gfxAllocation.registerBOBindExtHandle(mockDrm.get());
    EXPECT_EQ(1u, gfxAllocation.registeredBoBindHandles.size());
    gfxAllocation.freeRegisteredBOBindExtHandles(mockDrm.get());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationIsRegisteredThenBosAreMarkedForCaptureAndRequireImmediateBinding) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);
    // mock resource registration enabling by storing class handles
    drm.classHandles.push_back(1);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::DEBUG_CONTEXT_SAVE_AREA, MemoryPool::System4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);

    EXPECT_TRUE(bo.isMarkedForCapture());
    EXPECT_TRUE(bo.isImmediateBindingRequired());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndTileInstancedIsaWhenIsaIsRegisteredThenCookieIsAddedToBoHandle) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        drm.classHandles.push_back(i);
    }

    drm.registeredClass = DrmResourceClass::MaxSize;

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::KERNEL_ISA, MemoryPool::LocalMemory);
    allocation.storageInfo.tileInstanced = true;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
    EXPECT_EQ(drm.currentCookie - 1, bo.bindExtHandles[1]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSingleInstanceIsaWhenIsaIsRegisteredThenCookieIsNotAddedToBoHandle) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        drm.classHandles.push_back(i);
    }

    drm.registeredClass = DrmResourceClass::MaxSize;

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(1u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenIsaIsRegisteredThenDeviceBitfieldIsPassedAsPayload) {
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::MaxSize); i++) {
        drm.classHandles.push_back(i);
    }

    drm.registeredClass = DrmResourceClass::MaxSize;

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::KERNEL_ISA, MemoryPool::LocalMemory);
    allocation.storageInfo.subDeviceBitfield = 1 << 3;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    EXPECT_EQ(sizeof(uint32_t), drm.registeredDataSize);
    uint32_t *data = reinterpret_cast<uint32_t *>(drm.registeredData);
    EXPECT_EQ(static_cast<uint32_t>(allocation.storageInfo.subDeviceBitfield.to_ulong()), *data);
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCacheRegionIsCalledForDefaultRegionThenReturnTrue) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);

    EXPECT_TRUE(allocation.setCacheRegion(&drm, CacheRegion::Default));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheInfoIsNotAvailableThenCacheRegionIsNotSet) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::Region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenDefaultCacheInfoIsAvailableThenCacheRegionIsNotSet) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.setupCacheInfo(*defaultHwInfo.get());

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::Region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsNotSetThenReturnFalse) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.cacheInfo.reset(new MockCacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32));

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);

    EXPECT_FALSE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::None));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenReturnTrue) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();
    drm.cacheInfo.reset(new MockCacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32));

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);

    if ((HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getNumCacheRegions() == 0) &&
        HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::Region1));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::Region1));
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenSetRegionInBufferObject) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();
    drm.cacheInfo.reset(new MockCacheInfo(drm, 32 * MemoryConstants::kiloByte, 2, 32));

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    if ((HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getNumCacheRegions() == 0) &&
        HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::Region1));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::Region1));

        for (auto bo : allocation.bufferObjects) {
            if (bo != nullptr) {
                EXPECT_EQ(CacheRegion::Region1, bo->peekCacheRegion());
            }
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenBufferObjectIsCreatedThenApplyDefaultCachePolicy) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::WriteBack, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.setCachePolicy(CachePolicy::Uncached);

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::Uncached, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetMemAdviseWithCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    EXPECT_EQ(CachePolicy::WriteBack, bo.peekCachePolicy());

    MemAdviseFlags memAdviseFlags{};
    EXPECT_TRUE(memAdviseFlags.cached_memory);

    for (auto cached : {true, false, true}) {
        memAdviseFlags.cached_memory = cached;

        EXPECT_TRUE(allocation.setMemAdvise(&drm, memAdviseFlags));

        EXPECT_EQ(cached ? CachePolicy::WriteBack : CachePolicy::Uncached, bo.peekCachePolicy());

        EXPECT_EQ(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
    }
}

TEST_F(DrmAllocationTests, givenBoWhenMarkingForCaptureThenBosAreMarked) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::SCRATCH_SURFACE, MemoryPool::System4KBPages);
    allocation.markForCapture();

    allocation.bufferObjects[0] = &bo;
    allocation.markForCapture();

    EXPECT_TRUE(bo.isMarkedForCapture());
}

TEST_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithCacheRegionThenSetRegionInBufferObject) {
    if (HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getNumCacheRegions() == 0) {
        GTEST_SKIP();
    }
    mock->ioctl_expected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->cacheInfo.reset(new MockCacheInfo(*drm, 32 * MemoryConstants::kiloByte, 2, 32));

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = ptr;
    storage.fragmentStorageData[0].fragmentSize = 1;
    storage.fragmentCount = 1;

    memoryManager->populateOsHandles(storage, rootDeviceIndex);

    auto allocation = std::make_unique<DrmAllocation>(rootDeviceIndex, AllocationType::BUFFER_HOST_MEMORY,
                                                      nullptr, ptr, castToUint64(ptr), size, MemoryPool::System4KBPages);
    allocation->fragmentsStorage = storage;

    allocation->setCacheAdvice(drm, 1024, CacheRegion::Region1);

    for (uint32_t i = 0; i < storage.fragmentCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_EQ(CacheRegion::Region1, bo->peekCacheRegion());
    }

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull) {
    mock->ioctl_expected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    allocationData.size = size;
    allocationData.hostPtr = ptr;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    EXPECT_EQ(allocation, nullptr);
}

HWTEST_F(DrmMemoryManagerTest, givenDrmAllocationWithWithAlignmentFromUserptrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull) {
    mock->ioctl_expected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto size = MemoryConstants::pageSize;
    allocationData.size = size;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->createAllocWithAlignmentFromUserptr(allocationData, size, 0, 0, 0x1000));
    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenAllocateGraphicsMemoryWithPropertiesCalledWithDebugSurfaceTypeThenDebugSurfaceIsCreated) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b1011};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    EXPECT_EQ(3u, debugSurface->getNumGmms());

    auto &bos = debugSurface->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_NE(nullptr, bos[3]);

    EXPECT_EQ(debugSurface->getGpuAddress(), bos[0]->peekAddress());
    EXPECT_EQ(debugSurface->getGpuAddress(), bos[1]->peekAddress());
    EXPECT_EQ(debugSurface->getGpuAddress(), bos[3]->peekAddress());

    auto sipType = SipKernel::getSipKernelType(*device);
    SipKernel::initSipKernel(sipType, *device);

    auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*device).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = debugSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();

    EXPECT_TRUE(memoryZeroed(mem, size));

    memoryManager->freeGraphicsMemory(debugSurface);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenAffinityMaskDeviceWithBitfieldIndex1SetWhenAllocatingDebugSurfaceThenSingleAllocationWithOneBoIsCreated) {
    NEO::DebugManager.flags.CreateMultipleSubDevices.set(2);

    AffinityMaskHelper affinityMask;
    affinityMask.enableGenericSubDevice(1);

    executionEnvironment->rootDeviceEnvironments[0]->deviceAffinityMask = affinityMask;
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA, false, false, 0b0010};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    EXPECT_EQ(1u, debugSurface->getNumGmms());

    auto &bos = debugSurface->getBOs();
    auto bo = debugSurface->getBO();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(nullptr, bos[1]);
    EXPECT_EQ(bos[0], bo);

    EXPECT_EQ(debugSurface->getGpuAddress(), bos[0]->peekAddress());

    auto sipType = SipKernel::getSipKernelType(*device);
    SipKernel::initSipKernel(sipType, *device);

    auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*device).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = debugSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();

    EXPECT_TRUE(memoryZeroed(mem, size));

    memoryManager->freeGraphicsMemory(debugSurface);
}

TEST_F(DrmMemoryManagerTest, whenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize2Mb, false, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD2MB},
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(DrmMemoryManagerTest, whenDebugFlagToNotFreeResourcesIsSpecifiedThenFreeIsNotDoingAnything) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DoNotFreeResources.set(true);
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    size_t sizeIn = 1024llu;
    uint64_t gpuAddress = 0x1337llu;
    DrmAllocation stackDrmAllocation(0u, AllocationType::BUFFER, nullptr, nullptr, gpuAddress, sizeIn, MemoryPool::System64KBPages);
    memoryManager.freeGraphicsMemoryImpl(&stackDrmAllocation);
}

TEST_F(DrmMemoryManagerTest, given2MbPagesDisabledWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);

    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

TEST_F(DrmMemoryManagerTest, givenCustomAlignmentWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};

    {
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(MemoryConstants::megaByte);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {MemoryConstants::pageSize2Mb, false, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD2MB},
            {MemoryConstants::megaByte, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }

    {
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::pageSize2Mb);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {2 * MemoryConstants::pageSize2Mb, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD2MB},
            {MemoryConstants::pageSize2Mb, false, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD2MB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::HEAP_STANDARD64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }
}

TEST_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    TestedDrmMemoryManager memoryManager(true, false, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex), 0.95);
}

TEST_F(DrmMemoryManagerTest, givenDrmManagerWithoutLocalMemoryWhenGettingGlobalMemoryPercentThenCorrectValueIsReturned) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    uint32_t rootDeviceIndex = 0u;
    EXPECT_EQ(memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex), 0.8);
}

struct DrmMemoryManagerToTestLockInLocalMemory : public TestedDrmMemoryManager {
    using TestedDrmMemoryManager::lockResourceImpl;
    DrmMemoryManagerToTestLockInLocalMemory(ExecutionEnvironment &executionEnvironment)
        : TestedDrmMemoryManager(true, false, false, executionEnvironment) {}

    void *lockBufferObject(BufferObject *bo) override {
        lockedLocalMemory.reset(new uint8_t[bo->peekSize()]);
        return lockedLocalMemory.get();
    }
    std::unique_ptr<uint8_t[]> lockedLocalMemory;
};

TEST_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenLockResourceIsCalledOnWriteCombinedAllocationThenReturnPtrAlignedTo64Kb) {
    DrmMemoryManagerToTestLockInLocalMemory memoryManager(*executionEnvironment);
    BufferObject bo(mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(0, AllocationType::WRITE_COMBINED, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager.lockResourceImpl(drmAllocation);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(ptr));

    memoryManager.unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryAndCpuPtrWhenCopyMemoryToAllocationThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);
    allocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenNullDefaultAllocWhenCreateGraphicsAllocationFromExistingStorageThenDoNotImportHandle) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctl_expected.primeFdToHandle = 0;

    MockAllocationProperties properties(0u, 1u);
    MultiGraphicsAllocation allocation(0u);
    auto alloc = memoryManager.createGraphicsAllocationFromExistingStorage(properties, nullptr, allocation);

    EXPECT_NE(alloc, nullptr);

    memoryManager.freeGraphicsMemory(alloc);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    DrmAllocation drmAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager.lockBufferObject(drmAllocation.getBO());
    EXPECT_EQ(nullptr, ptr);

    memoryManager.unlockBufferObject(drmAllocation.getBO());
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenFreeGraphicsMemoryIsCalledOnAllocationWithNullBufferObjectThenEarlyReturn) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto drmAllocation = new DrmAllocation(0, AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, drmAllocation);

    memoryManager.freeGraphicsMemoryImpl(drmAllocation);
}

TEST(DrmMemoryManagerSimpleTest, WhenDrmIsCreatedThenQueryPageFaultSupportIsCalled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = std::unique_ptr<Drm>(Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]));

    EXPECT_TRUE(static_cast<DrmMock *>(drm.get())->queryPageFaultSupportCalled);
}

using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnAllocationInLocalMemoryThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingImportedMemoryThenCloseSharedHandleIsNotCalled) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation, true);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 0u);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingNonImportedMemoryThenCloseSharedHandleIsCalled) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 1u);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * GB, memoryManager->getLocalMemorySize(rootDeviceIndex, 0xF));
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemAdviseIsCalledThenUpdateCachePolicyInBufferObject) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(0, AllocationType::UNIFIED_SHARED_MEMORY, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    for (auto isCached : {false, true}) {
        MemAdviseFlags flags{};
        flags.cached_memory = isCached;

        EXPECT_TRUE(memoryManager.setMemAdvise(&drmAllocation, flags, rootDeviceIndex));
        EXPECT_EQ(isCached ? CachePolicy::WriteBack : CachePolicy::Uncached, bo.peekCachePolicy());
    }
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemPrefetchIsCalledThenReturnTrue) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(AllocationType::UNIFIED_SHARED_MEMORY, MemoryPool::LocalMemory);
    drmAllocation.bufferObjects[0] = &bo;

    memoryManager.registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager.registeredEngines) {
        engine.osContext->incRefInternal();
    }

    EXPECT_TRUE(memoryManager.setMemPrefetch(&drmAllocation, 0, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemPrefetchFailsToBindBufferObjectThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(mock, 3, 1, 1024, 0);

    MockDrmAllocation drmAllocation(AllocationType::UNIFIED_SHARED_MEMORY, MemoryPool::LocalMemory);
    drmAllocation.bufferObjects[0] = &bo;

    memoryManager.registeredEngines = EngineControlContainer{this->device->allEngines};
    for (auto engine : memoryManager.registeredEngines) {
        engine.osContext->incRefInternal();
    }

    drmAllocation.bindBOsRetValue = -1;

    EXPECT_FALSE(memoryManager.setMemPrefetch(&drmAllocation, 0, rootDeviceIndex));

    EXPECT_TRUE(drmAllocation.bindBOsCalled);
}

TEST_F(DrmMemoryManagerTest, givenPageFaultIsUnSupportedWhenCallingBindBoOnBufferAllocationThenAllocationShouldNotPageFaultAndExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    std::vector<BufferObject *> bufferObjects;
    allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true);

    EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
    EXPECT_FALSE(bo.isExplicitResidencyRequired());
}

TEST_F(DrmMemoryManagerTest, givenPageFaultIsSupportedAndKmdMigrationEnabledForBuffersWhenCallingBindBoOnBufferAllocationThenAllocationShouldPageFaultAndExplicitResidencyIsNotRequired) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableRecoverablePageFaults.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drm.pageFaultSupported);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    MockBufferObject bo(&drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto useKmdMigrationForBuffers : {-1, 0, 1}) {
        DebugManager.flags.UseKmdMigrationForBuffers.set(useKmdMigrationForBuffers);

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true);

        if (useKmdMigrationForBuffers > 0) {
            EXPECT_TRUE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_FALSE(bo.isExplicitResidencyRequired());
        } else {
            EXPECT_FALSE(allocation.shouldAllocationPageFault(&drm));
            EXPECT_TRUE(bo.isExplicitResidencyRequired());
        }
    }
}

TEST_F(DrmMemoryManagerTest, givenPageFaultIsSupportedWhenCallingBindBoOnAllocationThatShouldPageFaultThenExplicitResidencyIsNotRequired) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.pageFaultSupported = true;

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    uint32_t vmHandleId = 0;

    struct MockDrmAllocationToTestPageFault : MockDrmAllocation {
        MockDrmAllocationToTestPageFault() : MockDrmAllocation(AllocationType::BUFFER, MemoryPool::LocalMemory){};
        bool shouldAllocationPageFault(const Drm *drm) override {
            return shouldPageFault;
        }
        bool shouldPageFault = false;
    };

    for (auto shouldAllocationPageFault : {false, true}) {
        MockBufferObject bo(&drm, 3, 0, 0, 1);
        MockDrmAllocationToTestPageFault allocation;
        allocation.bufferObjects[0] = &bo;
        allocation.shouldPageFault = shouldAllocationPageFault;

        std::vector<BufferObject *> bufferObjects;
        allocation.bindBO(&bo, &osContext, vmHandleId, &bufferObjects, true);

        EXPECT_EQ(shouldAllocationPageFault, allocation.shouldAllocationPageFault(&drm));
        EXPECT_EQ(!shouldAllocationPageFault, bo.isExplicitResidencyRequired());
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithoutMemoryInfoThenNullBufferObjectIsReturned) {
    mock->memoryInfo.reset(nullptr);

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0), nullptr, AllocationType::BUFFER, gpuAddress, size, MemoryBanks::MainBank, 1, -1));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithZeroSizeThenNullBufferObjectIsReturned) {
    auto gpuAddress = 0x1234u;
    auto size = 0u;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0), nullptr, AllocationType::BUFFER, gpuAddress, size, MemoryBanks::MainBank, 1, -1));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseKmdMigrationForBuffersWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferWithSeveralMemoryBanksThenCreateGemObjectWithMultipleRegions) {
    DebugManager.flags.UseKmdMigrationForBuffers.set(1);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(allocData.storageInfo.memoryBanks, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->banks);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndOneHandleAndCommandBufferTypeThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::COMMAND_BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndOneHandleThenThenNoPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b01;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, true};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_NE(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenMemoryAllocationWithNoSetPairAndTwoHandlesThenPairHandleIsPassed) {
    VariableBackup<bool> backupSetPairCallParent{&mock->getSetPairAvailableCall.callParent, false};
    VariableBackup<bool> backupSetPairReturnValue{&mock->getSetPairAvailableCall.returnValue, false};

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_EQ(-1, static_cast<MockedMemoryInfo *>(mock->getMemoryInfo())->pairHandlePassed);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUseSystemMemoryFlagWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{rootDeviceIndex, false, size, AllocationType::SVM_GPU, false, mockDeviceBitfield};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

    EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    memoryManager->setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    memoryManager->setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithSameGpuVirtualAddressAndSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 3 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
        EXPECT_NE(0u, allocation->getGpuAddress());
        EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto &bos = drmAllocation->getBOs();
        auto boAddress = drmAllocation->getGpuAddress();
        for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
            auto bo = bos[handleId];
            ASSERT_NE(nullptr, bo);
            auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
            EXPECT_EQ(boAddress, bo->peekAddress());
            EXPECT_EQ(boSize, bo->peekSize());
            EXPECT_EQ(boSize, 3 * MemoryConstants::pageSize64k);
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithLargeBufferWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithDifferentGpuVirtualAddressesAndPartialSizes) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(EngineLimits::maxHandleCount, allocation->getNumGmms());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < EngineLimits::maxHandleCount; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(boSize, bo->peekSize());
        EXPECT_EQ(boSize, handleId == 0 || handleId == 1 ? 5 * MemoryConstants::pageSize64k : 4 * MemoryConstants::pageSize64k);
        boAddress += boSize;
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationWithKernelIsaWhenAllocationInDevicePoolAndDeviceBitfieldWithHolesThenCorrectAllocationCreated) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::KERNEL_ISA;
    allocData.storageInfo.memoryBanks = 0b1011;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto kernelIsaAllocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status));

    EXPECT_NE(nullptr, kernelIsaAllocation);

    auto gpuAddress = kernelIsaAllocation->getGpuAddress();
    auto &bos = kernelIsaAllocation->getBOs();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
    EXPECT_NE(nullptr, bos[1]);
    EXPECT_EQ(gpuAddress, bos[1]->peekAddress());

    EXPECT_EQ(nullptr, bos[2]);

    EXPECT_NE(nullptr, bos[3]);
    EXPECT_EQ(gpuAddress, bos[3]->peekAddress());

    auto &storageInfo = kernelIsaAllocation->storageInfo;
    EXPECT_EQ(0b1011u, storageInfo.memoryBanks.to_ulong());

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
}

struct DrmMemoryManagerLocalMemoryAlignmentTest : DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest {
    std::unique_ptr<TestedDrmMemoryManager> createMemoryManager() {
        return std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);
    }

    bool isAllocationWithinHeap(MemoryManager &memoryManager, const GraphicsAllocation &allocation, HeapIndex heap) {
        const auto allocationStart = allocation.getGpuAddress();
        const auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
        const auto gmmHelper = device->getGmmHelper();
        const auto heapStart = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapBase(heap));
        const auto heapEnd = gmmHelper->canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapLimit(heap));
        return heapStart <= allocationStart && allocationEnd <= heapEnd;
    }

    const uint32_t rootDeviceIndex = 1u;
    DebugManagerStateRestore restore{};
};

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationLessThen2MbThenUse64kbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationBiggerThan2MbThenUse2MbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenExtendedHeapPreferredAnd2MbAlignmentAllowedWhenAllocatingAllocationBiggerThenUseExtendedHeap) {
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTENDED) == 0) {
        GTEST_SKIP();
    }

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.resource48Bit = false;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationBiggerThanTheAlignmentThenAlignProperly) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // size==2MB, use 2MB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::megaByte);
        allocationData.size = 2 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 2 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size > 2MB, use 2MB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(16 * MemoryConstants::megaByte);
        allocationData.size = 16 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 16 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size < 2MB, use 64KB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(8 * MemoryConstants::pageSize64k);
        allocationData.size = 8 * MemoryConstants::pageSize64k;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 8 * MemoryConstants::pageSize64k));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationLessThanTheAlignmentThenIgnoreCustomAlignment) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 3 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // Too small allocation, fallback to 2MB heap
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // Too small allocation, fallback to 2MB heap
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    EXPECT_EQ(RESOURCE_BUFFER, gmm->resourceParams.Type);
    EXPECT_EQ(sizeAligned, gmm->resourceParams.BaseWidth64);
    EXPECT_NE(nullptr, gmm->gmmResourceInfo->peekHandle());
    EXPECT_NE(0u, gmm->gmmResourceInfo->getHAlign());

    auto gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0u, gpuAddress);

    auto heap = HeapIndex::HEAP_STANDARD64KB;
    if (memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        heap = HeapIndex::HEAP_EXTENDED;
    }

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(heap)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenPatIndexProgrammingAllowedWhenCreatingAllocationThenSetValidPatIndex) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    ASSERT_NE(nullptr, drmAllocation->getBO());

    auto isVmBindPatIndexProgrammingSupported = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->isVmBindPatIndexProgrammingSupported();

    EXPECT_EQ(isVmBindPatIndexProgrammingSupported, mock->isVmBindPatIndexProgrammingSupported());

    if (isVmBindPatIndexProgrammingSupported) {
        auto expectedIndex = mock->getPatIndex(allocation->getDefaultGmm(), allocation->getAllocationType(), CacheRegion::Default, CachePolicy::WriteBack, false);

        EXPECT_NE(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
        EXPECT_EQ(expectedIndex, drmAllocation->getBO()->peekPatIndex());
    } else {
        EXPECT_EQ(CommonConstants::unsupportedPatIndex, drmAllocation->getBO()->peekPatIndex());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForImageThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::IMAGE;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAligned = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize64k);
    EXPECT_NE(0u, gpuAddress);

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_STANDARD64KB)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD64KB)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(allocData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocData.storageInfo.pageTablesVisibility, allocation->storageInfo.pageTablesVisibility);
    EXPECT_EQ(allocData.storageInfo.cloningOfPageTables, allocation->storageInfo.cloningOfPageTables);
    EXPECT_EQ(allocData.storageInfo.tileInstanced, allocation->storageInfo.tileInstanced);
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenNotSetUseSystemMemoryWhenGraphicsAllocatioInDevicePoolIsAllocatednForKernelIsaThenLocalMemoryAllocationIsReturnedFromInternalHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};
    for (uint32_t i = 0; i < 2; i++) {
        allocData.type = isaTypes[i];
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
        EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

        auto gpuAddress = allocation->getGpuAddress();
        auto gmmHelper = device->getGmmHelper();
        EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
        EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
        EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), allocation->getGpuBaseAddress());
        EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
        EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

        auto drmAllocation = static_cast<DrmAllocation *>(allocation);
        auto bo = drmAllocation->getBO();
        EXPECT_NE(nullptr, bo);
        EXPECT_EQ(gpuAddress, bo->peekAddress());
        EXPECT_EQ(sizeAligned, bo->peekSize());

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenUnsupportedTypeWhenAllocatingInDevicePoolThenRetryInNonDevicePoolStatusAndNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType unsupportedTypes[] = {AllocationType::SHARED_RESOURCE_COPY};

    for (auto unsupportedType : unsupportedTypes) {
        allocData.type = unsupportedType;

        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenOversizedAllocationWhenGraphicsAllocationInDevicePoolIsAllocatedThenAllocationAndBufferObjectHaveRequestedSize) {
    auto heap = HeapIndex::HEAP_STANDARD64KB;
    if (memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        heap = HeapIndex::HEAP_EXTENDED;
    }
    auto largerSize = 6 * MemoryConstants::megaByte;

    auto gpuAddress0 = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocateWithCustomAlignment(heap, largerSize, MemoryConstants::pageSize2Mb);
    EXPECT_NE(0u, gpuAddress0);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    auto gpuAddress1 = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(heap, largerSize);
    EXPECT_NE(0u, gpuAddress1);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    auto gpuAddress2 = memoryManager->getGfxPartition(rootDeviceIndex)->heapAllocate(heap, largerSize);
    EXPECT_NE(0u, gpuAddress2);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    memoryManager->getGfxPartition(rootDeviceIndex)->heapFree(heap, gpuAddress1, largerSize);

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = 5 * MemoryConstants::megaByte;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    memoryManager->getGfxPartition(rootDeviceIndex)->heapFree(heap, gpuAddress2, largerSize);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(largerSize, allocation->getReservedAddressSize());
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenAllocationsThatAreAlignedToPowerOf2InSizeAndAreGreaterThen8GBThenTheyAreAlignedToPreviousPowerOfTwoForGpuVirtualAddress) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        GTEST_SKIP();
    }

    auto size = 8 * MemoryConstants::gigaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % size == 0u);

    size = 8 * MemoryConstants::gigaByte + MemoryConstants::pageSize64k;
    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocData.size, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2Mb == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenDebugVariableToDisableAddressAlignmentWhenAllocationsAreMadeTheyAreAlignedTo2MB) {
    if (!memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseHighAlignmentForHeapExtended.set(0);

    auto size = 16 * MemoryConstants::megaByte;

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = size;
    allocData.type = AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    EXPECT_TRUE(allocation->getGpuAddress() % MemoryConstants::pageSize2Mb == 0u);

    size = 32 * MemoryConstants::megaByte;
    allocData.size = size;
    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation2);
    EXPECT_EQ(allocData.size, allocation2->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation2)->getBO()->peekSize());
    EXPECT_TRUE(allocation2->getGpuAddress() % MemoryConstants::pageSize2Mb == 0u);

    memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocation2);
}

struct DrmMemoryManagerToTestCopyMemoryToAllocationBanks : public DrmMemoryManager {
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks(ExecutionEnvironment &executionEnvironment, size_t lockableLocalMemorySize)
        : DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            if (static_cast<uint32_t>(bo->peekHandle()) < lockedLocalMemory.size()) {
                lockedLocalMemory[bo->peekHandle()].reset(new uint8_t[lockedLocalMemorySize]);
                return lockedLocalMemory[bo->peekHandle()].get();
            }
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    size_t lockedLocalMemorySize = 0;
};

TEST(DrmMemoryManagerCopyMemoryToAllocationBanksTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnSpecificMemoryBanksThenAllocationIsFilledWithCorrectDataOnSpecificBanks) {
    uint8_t sourceData[64]{};
    size_t offset = 3;
    size_t sourceAllocationSize = sizeof(sourceData);
    size_t destinationAllocationSize = sourceAllocationSize + offset;
    MockExecutionEnvironment executionEnvironment;
    auto drm = new DrmMock(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    DrmMemoryManagerToTestCopyMemoryToAllocationBanks drmMemoryManger(executionEnvironment, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    MockDrmAllocation mockAllocation(AllocationType::WORK_PARTITION_SURFACE, MemoryPool::LocalMemory);

    mockAllocation.storageInfo.memoryBanks = 0b1111;
    DeviceBitfield memoryBanksToCopy = 0b1010;
    mockAllocation.bufferObjects.clear();

    for (auto index = 0u; index < 4; index++) {
        drmMemoryManger.lockedLocalMemory[index].reset();
        mockAllocation.bufferObjects.push_back(new BufferObject(drm, 3, index, sourceAllocationSize, 3));
    }

    auto ret = drmMemoryManger.copyMemoryToAllocationBanks(&mockAllocation, offset, dataToCopy.data(), dataToCopy.size(), memoryBanksToCopy);
    EXPECT_TRUE(ret);

    EXPECT_EQ(nullptr, drmMemoryManger.lockedLocalMemory[0].get());
    ASSERT_NE(nullptr, drmMemoryManger.lockedLocalMemory[1].get());
    EXPECT_EQ(nullptr, drmMemoryManger.lockedLocalMemory[2].get());
    ASSERT_NE(nullptr, drmMemoryManger.lockedLocalMemory[3].get());

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory[1].get(), offset), dataToCopy.data(), dataToCopy.size()));
    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory[3].get(), offset), dataToCopy.data(), dataToCopy.size()));
    for (auto index = 0u; index < 4; index++) {
        delete mockAllocation.bufferObjects[index];
    }
}

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectSucceedsThenReturnTrueAndCorrectOffset) {
    mock->ioctl_expected.gemMmapOffset = 1;
    BufferObject bo(mock, 3, 1, 1024, 0);
    mock->mmapOffsetExpected = 21;

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(21u, offset);
}

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectFailsThenReturnFalse) {
    mock->ioctl_expected.gemMmapOffset = 2;
    BufferObject bo(mock, 3, 1, 1024, 0);
    mock->failOnMmapOffset = true;

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_FALSE(ret);
}

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForLocalMemoryThenApplyCorrectFlags) {
    mock->ioctl_expected.gemMmapOffset = 5;
    BufferObject bo(mock, 3, 1, 1024, 0);

    uint64_t offset = 0;
    auto ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, 0, offset);

    EXPECT_TRUE(ret);
    EXPECT_EQ(4u, mock->mmapOffsetFlags);

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_FALSE(ret);
        EXPECT_EQ(flags, mock->mmapOffsetFlags);
    }
}

TEST_F(DrmMemoryManagerTest, givenDrmWhenRetrieveMmapOffsetForBufferObjectIsCalledForSystemMemoryThenApplyCorrectFlags) {
    mock->ioctl_expected.gemMmapOffset = 4;
    BufferObject bo(mock, 3, 1, 1024, 0);

    uint64_t offset = 0;
    bool ret = false;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_TRUE(ret);
        EXPECT_EQ(flags, mock->mmapOffsetFlags);
    }

    mock->failOnMmapOffset = true;

    for (uint64_t flags : {I915_MMAP_OFFSET_WC, I915_MMAP_OFFSET_WB}) {
        ret = memoryManager->retrieveMmapOffsetForBufferObject(rootDeviceIndex, bo, flags, offset);

        EXPECT_FALSE(ret);
        EXPECT_EQ(flags, mock->mmapOffsetFlags);
    }
}

TEST_F(DrmMemoryManagerTest, GivenEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    AllocationType validAllocations[] = {
        AllocationType::COMMAND_BUFFER,
        AllocationType::RING_BUFFER,
        AllocationType::SEMAPHORE_BUFFER,
        AllocationType::TAG_BUFFER};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(validAllocations[i]));
    }
}

TEST_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeWhenCheckingAllocationEligbleForCompletionFenceThenReturnFalse) {
    AllocationType invalidAllocations[] = {
        AllocationType::BUFFER_HOST_MEMORY,
        AllocationType::CONSTANT_SURFACE,
        AllocationType::FILL_PATTERN,
        AllocationType::GLOBAL_SURFACE};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_FALSE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

TEST_F(DrmMemoryManagerTest, GivenNotEligbleAllocationTypeAndDebugFlagOverridingWhenCheckingAllocationEligbleForCompletionFenceThenReturnTrue) {
    DebugManagerStateRestore dbgState;
    DebugManager.flags.UseDrmCompletionFenceForAllAllocations.set(1);

    AllocationType invalidAllocations[] = {
        AllocationType::BUFFER_HOST_MEMORY,
        AllocationType::CONSTANT_SURFACE,
        AllocationType::FILL_PATTERN,
        AllocationType::GLOBAL_SURFACE};

    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(memoryManager->allocationTypeForCompletionFence(invalidAllocations[i]));
    }
}

TEST_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndEligbleAllocationThenCallWaitUserFence) {
    mock->ioctl_expected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::COMMAND_BUFFER});
    auto engine = memoryManager->getRegisteredEngines()[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    uint64_t expectedFenceAddress = castToUint64(const_cast<uint32_t *>(engine.commandStreamReceiver->getTagAddress())) + Drm::completionFenceOffset;
    constexpr uint64_t expectedValue = 2;

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(1u, mock->waitUserFenceCall.called);
    EXPECT_EQ(expectedFenceAddress, mock->waitUserFenceCall.address);
    EXPECT_EQ(expectedValue, mock->waitUserFenceCall.value);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfNotUsedAndEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctl_expected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::COMMAND_BUFFER});

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionOfUsedAndNotEligbleAllocationThenDoNotCallWaitUserFence) {
    mock->ioctl_expected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::GLOBAL_SURFACE});
    auto engine = memoryManager->getRegisteredEngines()[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    memoryManager->handleFenceCompletion(allocation);

    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_F(DrmMemoryManagerTest, givenCompletionFenceEnabledWhenHandlingCompletionAndTagAddressIsNullThenDoNotCallWaitUserFence) {
    mock->ioctl_expected.total = -1;

    VariableBackup<bool> backupFenceSupported{&mock->completionFenceSupported, true};
    VariableBackup<bool> backupVmBindCallParent{&mock->isVmBindAvailableCall.callParent, false};
    VariableBackup<bool> backupVmBindReturnValue{&mock->isVmBindAvailableCall.returnValue, true};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1024, AllocationType::COMMAND_BUFFER});
    auto engine = memoryManager->getRegisteredEngines()[0];
    allocation->updateTaskCount(2, engine.osContext->getContextId());

    auto testCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(engine.commandStreamReceiver);
    auto backupTagAddress = testCsr->tagAddress;
    testCsr->tagAddress = nullptr;

    memoryManager->handleFenceCompletion(allocation);
    EXPECT_EQ(0u, mock->waitUserFenceCall.called);

    testCsr->tagAddress = backupTagAddress;

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenMultiSubDevicesBitfieldWhenAllocatingSbaTrackingBufferThenCorrectMultiHostAllocationReturned) {
    mock->ioctl_expected.total = -1;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false, false,
                                         0b0011};

    const uint64_t gpuAddresses[] = {0, 0x12340000};

    for (auto gpuAddress : gpuAddresses) {
        properties.gpuAddress = gpuAddress;

        auto sbaBuffer = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));

        EXPECT_NE(nullptr, sbaBuffer);

        EXPECT_EQ(MemoryPool::System4KBPages, sbaBuffer->getMemoryPool());
        EXPECT_EQ(2u, sbaBuffer->getNumGmms());

        EXPECT_NE(nullptr, sbaBuffer->getUnderlyingBuffer());
        EXPECT_EQ(MemoryConstants::pageSize, sbaBuffer->getUnderlyingBufferSize());

        auto &bos = sbaBuffer->getBOs();

        EXPECT_NE(nullptr, bos[0]);
        EXPECT_NE(nullptr, bos[1]);

        if (gpuAddress != 0) {
            EXPECT_EQ(gpuAddress, sbaBuffer->getGpuAddress());

            EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
            EXPECT_EQ(gpuAddress, bos[1]->peekAddress());
            EXPECT_EQ(0u, sbaBuffer->getReservedAddressPtr());
        } else {
            EXPECT_EQ(bos[0]->peekAddress(), bos[1]->peekAddress());
            EXPECT_NE(nullptr, sbaBuffer->getReservedAddressPtr());
            EXPECT_NE(0u, sbaBuffer->getGpuAddress());
        }

        EXPECT_EQ(nullptr, bos[2]);
        EXPECT_EQ(nullptr, bos[3]);

        memoryManager->freeGraphicsMemory(sbaBuffer);
    }
}

TEST_F(DrmMemoryManagerTest, givenSingleSubDevicesBitfieldWhenAllocatingSbaTrackingBufferThenSingleHostAllocationReturned) {
    mock->ioctl_expected.total = -1;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                         NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER,
                                         false, false,
                                         0b0001};

    const uint64_t gpuAddresses[] = {0, 0x12340000};

    for (auto gpuAddress : gpuAddresses) {
        properties.gpuAddress = gpuAddress;

        auto sbaBuffer = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties));

        EXPECT_NE(nullptr, sbaBuffer);

        EXPECT_EQ(MemoryPool::System4KBPages, sbaBuffer->getMemoryPool());
        EXPECT_EQ(1u, sbaBuffer->getNumGmms());

        EXPECT_NE(nullptr, sbaBuffer->getUnderlyingBuffer());
        EXPECT_EQ(MemoryConstants::pageSize, sbaBuffer->getUnderlyingBufferSize());

        auto &bos = sbaBuffer->getBOs();

        EXPECT_NE(nullptr, bos[0]);
        EXPECT_EQ(nullptr, bos[1]);
        EXPECT_EQ(nullptr, bos[2]);
        EXPECT_EQ(nullptr, bos[3]);

        if (gpuAddress != 0) {
            EXPECT_EQ(gpuAddress, sbaBuffer->getGpuAddress());
            EXPECT_EQ(gpuAddress, bos[0]->peekAddress());
        } else {
            EXPECT_NE(0u, sbaBuffer->getGpuAddress());
        }

        memoryManager->freeGraphicsMemory(sbaBuffer);
    }
}
