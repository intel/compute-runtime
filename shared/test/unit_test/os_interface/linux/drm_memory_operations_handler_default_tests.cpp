/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

struct MockDrmMemoryOperationsHandlerDefault : public DrmMemoryOperationsHandlerDefault {
    using BaseClass = DrmMemoryOperationsHandlerDefault;
    using DrmMemoryOperationsHandlerDefault::DrmMemoryOperationsHandlerDefault;
    using DrmMemoryOperationsHandlerDefault::residency;
    ADDMETHOD(makeResidentWithinOsContext, MemoryOperationsStatus, true, MemoryOperationsStatus::success, (OsContext * osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock), (osContext, gfxAllocations, evictable, forcePagingFence, acquireLock));
    ADDMETHOD(flushDummyExec, MemoryOperationsStatus, true, MemoryOperationsStatus::success, (Device * device, ArrayRef<GraphicsAllocation *> gfxAllocations), (device, gfxAllocations));
    ADDMETHOD(evictWithinOsContext, MemoryOperationsStatus, true, MemoryOperationsStatus::success, (OsContext * osContext, GraphicsAllocation &gfxAllocation), (osContext, gfxAllocation));
};
struct DrmMemoryOperationsHandlerBaseTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->calculateMaxOsContextCount();
        mock = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        mock->setBindAvailable();

        drmMemoryOperationsHandler = std::make_unique<MockDrmMemoryOperationsHandlerDefault>(*executionEnvironment->rootDeviceEnvironments[0].get(), 0);
    }
    void initializeAllocation(int numBos) {
        if (!drmAllocation) {
            for (auto i = 0; i < numBos; i++) {
                mockBos.push_back(new MockBufferObject(0, mock, 3, 0, 0, 1));
            }
            drmAllocation = new MockDrmAllocation(AllocationType::unknown, MemoryPool::localMemory, mockBos);
            for (auto i = 0; i < numBos; i++) {
                drmAllocation->storageInfo.memoryBanks[i] = 1;
            }
            allocationPtr = drmAllocation;
        }
    }
    void TearDown() override {
        for (auto i = 0u; i < mockBos.size(); i++) {
            delete mockBos[i];
        }
        mockBos.clear();
        delete drmAllocation;
        delete mock;
        delete executionEnvironment;
    }

    ExecutionEnvironment *executionEnvironment;
    DrmMock *mock;
    BufferObjects mockBos;
    MockDrmAllocation *drmAllocation = nullptr;
    GraphicsAllocation *allocationPtr = nullptr;
    std::unique_ptr<MockDrmMemoryOperationsHandlerDefault> drmMemoryOperationsHandler;
};

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenMakingAllocationResidentThenAllocationIsResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::success);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingResidentAllocationThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *allocationPtr), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocationPtr), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenLockingAllocationThenAllocationIsResident) {
    initializeAllocation(2);
    EXPECT_EQ(2u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(drmAllocation->isLockedMemory());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationWithMultipleBOsThenAllocationIsNotResident) {
    initializeAllocation(2);
    EXPECT_EQ(2u, drmAllocation->storageInfo.getNumBanks());

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingLockedAllocationWithChunkedThenAllocationIsNotResident) {
    initializeAllocation(1);
    EXPECT_EQ(1u, drmAllocation->storageInfo.getNumBanks());
    drmAllocation->storageInfo.isChunked = true;

    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);

    EXPECT_EQ(drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), drmAllocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmAllocation->isLockedMemory());

    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, *drmAllocation), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *drmAllocation), MemoryOperationsStatus::memoryNotFound);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_FALSE(drmAllocation->isLockedMemory());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenOperationsHandlerWhenCallingMakeResidentWithDummyExecNotNeededThenFlushDummyExecNotCalled) {
    drmMemoryOperationsHandler->flushDummyExecCallBase = false;
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;

    initializeAllocation(1);
    drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), false, false);
    EXPECT_EQ(drmMemoryOperationsHandler->flushDummyExecCalled, 0u);
}
TEST_F(DrmMemoryOperationsHandlerBaseTest, givenOperationsHandlerWhenMakeResidentWithinOsContextReturnFailhenFlushDummyExecNotCalled) {
    drmMemoryOperationsHandler->flushDummyExecCallBase = false;
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;
    drmMemoryOperationsHandler->makeResidentWithinOsContextResult = MemoryOperationsStatus::failed;

    initializeAllocation(1);
    drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), true, false);
    EXPECT_EQ(drmMemoryOperationsHandler->flushDummyExecCalled, 0u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenOperationsHandlerWhenCallingMakeResidentWithDummyExecNeededThenFlushDummyExecCalled) {
    drmMemoryOperationsHandler->flushDummyExecCallBase = false;
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;

    initializeAllocation(1);
    drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), true, false);
    EXPECT_EQ(drmMemoryOperationsHandler->flushDummyExecCalled, 1u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenOperationsHandlerWhenFlushReturnedOOMThenEvictCalled) {
    drmMemoryOperationsHandler->flushDummyExecCallBase = false;
    drmMemoryOperationsHandler->flushDummyExecResult = MemoryOperationsStatus::outOfMemory;
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;

    initializeAllocation(1);
    drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), true, false);
    EXPECT_EQ(drmMemoryOperationsHandler->evictWithinOsContextCalled, 1u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenOperationsHandlerWhenFlushReturnedSuccessThenEvictNotcalled) {
    drmMemoryOperationsHandler->flushDummyExecCallBase = false;
    drmMemoryOperationsHandler->flushDummyExecResult = MemoryOperationsStatus::success;
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;

    initializeAllocation(1);
    drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1), true, false);
    EXPECT_EQ(drmMemoryOperationsHandler->evictWithinOsContextCalled, 0u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenRegularAllocationWhenDecompressingAllocationThenUnsupportIsReturned) {
    initializeAllocation(1);
    EXPECT_EQ(drmMemoryOperationsHandler->decompress(nullptr, *allocationPtr), MemoryOperationsStatus::unsupported);
}

struct DrmMemoryOperationsHandlerBaseTestFlushDummyExec : public DrmMemoryOperationsHandlerBaseTest {
    using BaseClass = DrmMemoryOperationsHandlerBaseTest;
    void SetUp() override {
        BaseClass::SetUp();
        drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = false;
        drmMemoryOperationsHandler->evictWithinOsContextCallBase = false;
        auto mockExecutionEnvironment = std::make_unique<MockExecutionEnvironment>();

        auto rootDeviceEnvironment = mockExecutionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));

        auto memoryManager = std::make_unique<MockDrmMemoryManager>(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *mockExecutionEnvironment);
        memoryManager->emitPinningRequestForBoContainerCallBase = false;
        pMemManager = memoryManager.get();
        mockExecutionEnvironment->memoryManager.reset(memoryManager.release());
        device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(mockExecutionEnvironment.release(), 0u));
        initializeAllocation(1);
    }

    void TearDown() override {
        BaseClass::TearDown();
    }
    MockDrmMemoryManager *pMemManager;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockCommandStreamReceiver> commandStreamReceiver;
};

TEST_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenOperationsHandlerWhenEmitPiningRequestCalledThenEmitPinRequestCalled) {
    pMemManager->emitPinningRequestForBoContainerResult = SubmissionStatus::outOfMemory;
    drmMemoryOperationsHandler->flushDummyExec(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1));
    EXPECT_EQ(pMemManager->emitPinningRequestForBoContainerCalled, 1u);
}

TEST_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenOperationsHandlerWhenEmitPiningRequestReturnFailThenOutOfMemoryReturned) {
    pMemManager->emitPinningRequestForBoContainerResult = SubmissionStatus::outOfMemory;
    auto ret = drmMemoryOperationsHandler->flushDummyExec(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1));
    EXPECT_EQ(ret, MemoryOperationsStatus::outOfMemory);
}

TEST_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenOperationsHandlerWhenEmitPiningRequestReturnSuccessThenSuccessReturned) {
    pMemManager->emitPinningRequestForBoContainerResult = SubmissionStatus::success;
    auto ret = drmMemoryOperationsHandler->flushDummyExec(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1));
    EXPECT_EQ(ret, MemoryOperationsStatus::success);
}

TEST_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenOperationsHandlerWhenEmitPiningRequestReturnOOMThenOOMReturned) {
    pMemManager->emitPinningRequestForBoContainerResult = SubmissionStatus::outOfMemory;
    auto ret = drmMemoryOperationsHandler->flushDummyExec(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1));
    EXPECT_EQ(ret, MemoryOperationsStatus::outOfMemory);
}

TEST_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenUsmHostPtrFragmentsEnabledAndResidentAllocationsSharingFragmentWhenFlushingDummyExecThenEveryBufferObjectIsPinnedOnce) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmHostPtrFragments.set(1);
    MockBufferObject firstFragmentBo(0, mock, 3, 0, 0, 1);
    MockBufferObject sharedFragmentBo(0, mock, 3, 0, 0, 1);
    MockBufferObject secondFragmentBo(0, mock, 3, 0, 0, 1);
    OsHandleLinux firstFragmentHandle;
    OsHandleLinux sharedFragmentHandle;
    OsHandleLinux secondFragmentHandle;
    firstFragmentHandle.bo = &firstFragmentBo;
    sharedFragmentHandle.bo = &sharedFragmentBo;
    secondFragmentHandle.bo = &secondFragmentBo;
    MockDrmAllocation firstAllocation(0u, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
    firstAllocation.fragmentsStorage.fragmentCount = 2;
    firstAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage = &firstFragmentHandle;
    firstAllocation.fragmentsStorage.fragmentStorageData[1].osHandleStorage = &sharedFragmentHandle;
    MockDrmAllocation secondAllocation(0u, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
    secondAllocation.fragmentsStorage.fragmentCount = 2;
    secondAllocation.fragmentsStorage.fragmentStorageData[0].osHandleStorage = &sharedFragmentHandle;
    secondAllocation.fragmentsStorage.fragmentStorageData[1].osHandleStorage = &secondFragmentHandle;
    drmMemoryOperationsHandler->residency = {drmAllocation, &firstAllocation, &secondAllocation};

    EXPECT_EQ(MemoryOperationsStatus::success, drmMemoryOperationsHandler->flushDummyExec(device.get(), ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)));

    const auto &pinnedBos = pMemManager->emitPinningRequestForBoContainerBos;
    EXPECT_EQ(4u, pinnedBos.size());
    for (BufferObject *bo : {mockBos[0], static_cast<BufferObject *>(&firstFragmentBo), static_cast<BufferObject *>(&sharedFragmentBo), static_cast<BufferObject *>(&secondFragmentBo)}) {
        EXPECT_EQ(1, std::count(pinnedBos.begin(), pinnedBos.end(), bo));
    }

    drmMemoryOperationsHandler->residency.clear();
    firstAllocation.fragmentsStorage.fragmentCount = 0;
    secondAllocation.fragmentsStorage.fragmentCount = 0;
}

HWTEST2_F(DrmMemoryOperationsHandlerBaseTestFlushDummyExec, givenDirectSubmissionLightWhenFreeGraphicsMemoryThenStopDirectSubmission, IsXeLpg) {
    drmMemoryOperationsHandler->makeResidentWithinOsContextCallBase = true;
    drmMemoryOperationsHandler->evictWithinOsContextCallBase = true;
    auto allocation = pMemManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0u, MemoryConstants::pageSize});
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocation, 1), false, false), MemoryOperationsStatus::success);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(std::find(drmMemoryOperationsHandler->residency.begin(), drmMemoryOperationsHandler->residency.end(), allocation) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, *allocation), MemoryOperationsStatus::success);
    pMemManager->getRegisteredEngines(0u)[0].osContext->setDirectSubmissionActive();

    pMemManager->freeGraphicsMemoryImpl(allocation);

    EXPECT_TRUE(static_cast<UltCommandStreamReceiver<FamilyType> *>(pMemManager->getRegisteredEngines(0u)[0].commandStreamReceiver)->stopDirectSubmissionCalled);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenSingleBankAllocationWithEmptyBufferObjectSlotsWhenLockingAndEvictingThenAllocationLockedStateIsUpdated) {
    MockBufferObject bo(0, mock, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    ASSERT_LT(1u, allocation.getBOs().size());
    ASSERT_EQ(nullptr, allocation.getBOs()[1]);
    GraphicsAllocation *allocationPtr = &allocation;

    EXPECT_EQ(MemoryOperationsStatus::success, drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)));
    EXPECT_TRUE(allocation.isLockedMemory());

    EXPECT_EQ(MemoryOperationsStatus::success, drmMemoryOperationsHandler->evict(nullptr, allocation));
    EXPECT_FALSE(allocation.isLockedMemory());
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, givenMultiBankAllocationWithEmptyBufferObjectSlotsWhenLockingAndEvictingThenAllocationLockedStateIsUpdated) {
    MockBufferObject firstBankBo(0, mock, 3, 0, 0, 1);
    MockBufferObject secondBankBo(0, mock, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.storageInfo.memoryBanks = 0b11;
    allocation.bufferObjects[0] = &firstBankBo;
    allocation.bufferObjects[1] = &secondBankBo;
    ASSERT_EQ(2u, allocation.storageInfo.getNumBanks());
    ASSERT_LT(2u, allocation.getBOs().size());
    ASSERT_EQ(nullptr, allocation.getBOs()[2]);
    GraphicsAllocation *allocationPtr = &allocation;

    EXPECT_EQ(MemoryOperationsStatus::success, drmMemoryOperationsHandler->lock(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)));
    EXPECT_TRUE(allocation.isLockedMemory());

    EXPECT_EQ(MemoryOperationsStatus::success, drmMemoryOperationsHandler->evict(nullptr, allocation));
    EXPECT_FALSE(allocation.isLockedMemory());
}
