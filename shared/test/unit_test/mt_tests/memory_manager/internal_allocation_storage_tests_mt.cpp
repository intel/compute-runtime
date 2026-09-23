/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

using namespace NEO;

struct InternalAllocationStorageMtTest : public MemoryAllocatorFixture,
                                         public ::testing::Test {
    void SetUp() override {
        MemoryAllocatorFixture::setUp();
        storage = csr->getInternalAllocationStorage();
    }

    void TearDown() override {
        MemoryAllocatorFixture::tearDown();
    }
    InternalAllocationStorage *storage;
};

class WaitAtDeletionAllocation : public MockGraphicsAllocation {
  public:
    WaitAtDeletionAllocation(void *buffer, size_t sizeIn)
        : MockGraphicsAllocation(buffer, sizeIn) {
        inDestructor = false;
    }

    std::mutex mutex;
    std::atomic<bool> inDestructor;
    ~WaitAtDeletionAllocation() override {
        inDestructor = true;
        std::lock_guard<std::mutex> lock(mutex);
    }
};

TEST_F(InternalAllocationStorageMtTest, givenAllocationListWhenTwoThreadsCleanConcurrentlyThenBothThreadsCanAccessTheList) {
    auto allocation1 = new WaitAtDeletionAllocation(nullptr, 0);
    allocation1->updateTaskCount(1, csr->getOsContext().getContextId());
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation1), TEMPORARY_ALLOCATION);

    std::unique_lock<std::mutex> allocationDeletionLock(allocation1->mutex);

    auto allocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    allocation2->updateTaskCount(2, csr->getOsContext().getContextId());
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation2), TEMPORARY_ALLOCATION);

    std::atomic<bool> thread2CanRun{false};

    std::thread thread1([&] {
        storage->cleanAllocationList(1, TEMPORARY_ALLOCATION);
    });

    std::thread thread2([&] {
        while (!thread2CanRun.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        storage->cleanAllocationList(2, TEMPORARY_ALLOCATION);
    });

    while (!allocation1->inDestructor) {
        ;
    }
    thread2CanRun.store(true, std::memory_order_release);
    allocationDeletionLock.unlock();

    thread1.join();
    thread2.join();

    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(InternalAllocationStorageMtTest, givenCompletedGpuTaskWhileCsrIsOwnedWhenAnotherThreadCleansThenAllocationSurvivesUntilCpuReleasesCsr) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::externalHostPtr});
    ASSERT_NE(nullptr, allocation);
    auto submissionLock = csr->obtainUniqueOwnership();
    csr->makeResident(*allocation);
    const auto submissionTaskCount = allocation->getTaskCount(csr->getOsContext().getContextId());
    *csr->getTagAddress() = submissionTaskCount;
    storage->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);

    std::thread cleanupThread([&] {
        storage->cleanAllocationList(submissionTaskCount, TEMPORARY_ALLOCATION);
    });
    cleanupThread.join();
    ASSERT_EQ(allocation, storage->getTemporaryAllocations().peekHead());

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);
    submissionLock.unlock();
    storage->cleanAllocationList(submissionTaskCount, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(storage->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(InternalAllocationStorageMtTest, givenAllocationUsedByTwoCsrsWhenOneIsBusyThenCleanupSkipsItAndReleasesLocksBeforeLaterDeletion) {
    MockCommandStreamReceiver secondCsr(*executionEnvironment, csr->getRootDeviceIndex(), csr->getOsContext().getDeviceBitfield());
    auto secondContext = memoryManager->createAndRegisterOsContext(&secondCsr, EngineDescriptorHelper::getDefaultDescriptor());
    secondCsr.setupContext(*secondContext);
    memoryManager->callBaseAllocInUse = true;

    auto unrelatedAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, unrelatedAllocation);

    struct DeletionCheckingAllocation : MockGraphicsAllocation {
        DeletionCheckingAllocation(uint32_t rootDeviceIndex, size_t contextCount)
            : MockGraphicsAllocation(rootDeviceIndex, 1u, AllocationType::externalHostPtr, nullptr, uint64_t{0}, uint64_t{0}, size_t{0}, MemoryPool::memoryNull, contextCount) {}
        ~DeletionCheckingAllocation() override { onDelete(); }
        std::function<void()> onDelete;
    };

    auto allocation = new DeletionCheckingAllocation(csr->getRootDeviceIndex(), static_cast<size_t>(secondContext->getContextId()) + 1u);
    bool deleted = false;
    bool firstCsrLockedDuringDeletion = false;
    bool secondCsrLockedDuringDeletion = false;
    allocation->onDelete = [&] {
        std::thread observer([&] {
            auto firstLock = csr->tryObtainUniqueOwnership();
            auto secondLock = secondCsr.tryObtainUniqueOwnership();
            firstCsrLockedDuringDeletion = !firstLock.owns_lock();
            secondCsrLockedDuringDeletion = !secondLock.owns_lock();
        });
        observer.join();
        deleted = true;
    };
    constexpr TaskCountType completedTaskCount = 1u;
    allocation->updateTaskCount(completedTaskCount, secondContext->getContextId());
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION, completedTaskCount);
    *csr->getTagAddress() = completedTaskCount;
    *secondCsr.getTagAddress() = completedTaskCount;

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(unrelatedAllocation), TEMPORARY_ALLOCATION, completedTaskCount);

    auto secondSubmissionLock = secondCsr.obtainUniqueOwnership();
    std::thread cleanupThread([&] {
        storage->cleanAllocationList(completedTaskCount, TEMPORARY_ALLOCATION);
    });
    cleanupThread.join();
    EXPECT_FALSE(deleted);
    EXPECT_EQ(allocation, storage->getTemporaryAllocations().peekHead());
    EXPECT_EQ(allocation, storage->getTemporaryAllocations().peekTail());

    std::thread lockObserver([&] {
        auto firstLock = csr->tryObtainUniqueOwnership();
        EXPECT_TRUE(firstLock.owns_lock());
    });
    lockObserver.join();
    secondSubmissionLock.unlock();

    *secondCsr.getTagAddress() = completedTaskCount - 1;
    storage->cleanAllocationList(completedTaskCount, TEMPORARY_ALLOCATION);
    EXPECT_FALSE(deleted);
    EXPECT_EQ(allocation, storage->getTemporaryAllocations().peekHead());

    *secondCsr.getTagAddress() = completedTaskCount;
    storage->cleanAllocationList(completedTaskCount, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(firstCsrLockedDuringDeletion);
    EXPECT_FALSE(secondCsrLockedDuringDeletion);
    EXPECT_TRUE(storage->getTemporaryAllocations().peekIsEmpty());
    if (!deleted) {
        allocation->onDelete = [] {};
    }
}
