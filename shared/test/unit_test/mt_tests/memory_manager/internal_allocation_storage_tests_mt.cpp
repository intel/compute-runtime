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
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

#include <atomic>
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
