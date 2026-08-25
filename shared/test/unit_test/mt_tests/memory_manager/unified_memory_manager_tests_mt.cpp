/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>

using namespace NEO;

TEST(SvmDeviceAllocationTest, givenGivenSvmAllocsManagerWhenObtainOwnershipCalledThenLockedUniqueLockReturned) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    auto device = deviceFactory->rootDevices[0];
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());

    auto lock = svmManager->obtainOwnership();
    std::thread th1([&] {
        EXPECT_FALSE(svmManager->mtxForIndirectAccess.try_lock());
    });
    th1.join();
    lock.unlock();
    std::thread th2([&] {
        EXPECT_TRUE(svmManager->mtxForIndirectAccess.try_lock());
        svmManager->mtxForIndirectAccess.unlock();
    });
    th2.join();
}

// Parks the first thread that reaches MockMemoryManager::allocInUse() and holds it there until the
// test releases it, pinning a drain partway through a processing pass. Safe because
// freeSVMAllocImpl() calls allocInUse() before taking the container lock, so the parked thread holds
// no lock - a test can query the manager while it waits. allocInUse() also runs before the
// deferAllocInUse check, so parking does not change what it reports.
class AllocInUseParkingLot {
  public:
    AllocInUseParkingLot() = default;
    explicit AllocInUseParkingLot(std::function<bool()> parkWhen) : parkWhen(std::move(parkWhen)) {}

    void parkFirstCallerUntilReleased() {
        if (parked.load()) {
            return;
        }
        if (parkWhen && false == parkWhen()) {
            return;
        }
        if (parked.exchange(true)) {
            return;
        }
        spinUntil([this]() { return released.load(); });
    }

    [[nodiscard]] bool waitUntilParked() {
        return spinUntil([this]() { return parked.load(); });
    }

    void release() {
        released = true;
    }

  private:
    // Both waits are bounded: a test that never reaches the state it is waiting for has to fail an
    // expectation rather than hang the run, and the parked thread has to resume either way so that it
    // stays joinable.
    template <typename PredicateT>
    static bool spinUntil(PredicateT &&predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (false == predicate()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    std::function<bool()> parkWhen;
    std::atomic_bool parked = false;
    std::atomic_bool released = false;
};

struct SvmDeferredFreeQueueTest : public ::testing::Test {
    void SetUp() override {
        deviceFactory = std::make_unique<UltDeviceFactory>(1, 1);
        device = deviceFactory->rootDevices[0];
        memoryManager = static_cast<MockMemoryManager *>(device->getMemoryManager());
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager);

        rootDeviceIndices.pushUnique(device->getRootDeviceIndex());
        deviceBitfields.insert({device->getRootDeviceIndex(), device->getDeviceBitfield()});
    }

    void allocateAndQueueForDeferredFree(size_t count) {
        UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
        unifiedMemoryProperties.device = device;

        memoryManager->deferAllocInUse = true;
        for (size_t i = 0; i < count; i++) {
            auto ptr = svmManager->createUnifiedMemoryAllocation(4096, unifiedMemoryProperties);
            ASSERT_NE(nullptr, ptr);
            queuedPointers.push_back(ptr);
            svmManager->freeSVMAllocDefer(ptr);
        }
        ASSERT_EQ(count, svmManager->getNumDeferFreeAllocs());
    }

    void letDeferredFreeEntriesBeFreed() {
        memoryManager->deferAllocInUse = false;
    }

    void expectEveryQueuedPointerFreedOnce(uint32_t freeGraphicsMemoryCalledBefore) {
        for (auto ptr : queuedPointers) {
            EXPECT_EQ(nullptr, svmManager->getSVMAlloc(ptr));
        }
        EXPECT_EQ(queuedPointers.size(), memoryManager->freeGraphicsMemoryCalled.load() - freeGraphicsMemoryCalledBefore);
    }

    std::unique_ptr<UltDeviceFactory> deviceFactory;
    MockDevice *device = nullptr;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    RootDeviceIndicesContainer rootDeviceIndices;
    std::map<uint32_t, DeviceBitfield> deviceBitfields;
    std::vector<void *> queuedPointers;
};

TEST_F(SvmDeferredFreeQueueTest, givenSecondThreadDrainingWhileFirstThreadIsMidPassThenEachQueuedAllocationIsFreedOnce) {
    constexpr size_t queuedAllocationsCount = 8;
    allocateAndQueueForDeferredFree(queuedAllocationsCount);
    if (HasFatalFailure()) {
        return;
    }
    letDeferredFreeEntriesBeFreed();

    const auto freeGraphicsMemoryCalledBeforeDrain = memoryManager->freeGraphicsMemoryCalled.load();

    AllocInUseParkingLot parkingLot;
    memoryManager->allocInUseCallback = [&]() { parkingLot.parkFirstCallerUntilReleased(); };

    std::thread parkedDrain([&]() { svmManager->freeSVMAllocDeferImpl(); });
    EXPECT_TRUE(parkingLot.waitUntilParked());

    svmManager->freeSVMAllocDeferImpl();

    parkingLot.release();
    parkedDrain.join();
    memoryManager->allocInUseCallback = nullptr;

    EXPECT_EQ(0ul, svmManager->getNumDeferFreeAllocs());
    expectEveryQueuedPointerFreedOnce(freeGraphicsMemoryCalledBeforeDrain);
}

TEST_F(SvmDeferredFreeQueueTest, givenDrainRequeueingEntriesWhenAnotherThreadSamplesOutstandingCountThenQueuedAndInFlightAreBothCounted) {
    constexpr size_t queuedAllocationsCount = 4;
    allocateAndQueueForDeferredFree(queuedAllocationsCount);
    if (HasFatalFailure()) {
        return;
    }

    auto anyEntryAlreadyRequeued = [&]() { return svmManager->getNumClaimableDeferFreeAllocs() > 0; };
    AllocInUseParkingLot parkingLot(anyEntryAlreadyRequeued);
    memoryManager->allocInUseCallback = [&]() { parkingLot.parkFirstCallerUntilReleased(); };

    std::thread requeueingDrain([&]() { svmManager->freeSVMAllocDeferImpl(); });
    EXPECT_TRUE(parkingLot.waitUntilParked());

    const auto outstandingMidPass = svmManager->getNumDeferFreeAllocs();
    const auto claimableMidPass = svmManager->getNumClaimableDeferFreeAllocs();

    parkingLot.release();
    requeueingDrain.join();
    memoryManager->allocInUseCallback = nullptr;

    EXPECT_EQ(queuedAllocationsCount, outstandingMidPass);
    EXPECT_GE(claimableMidPass, 1ul);
    EXPECT_LT(claimableMidPass, queuedAllocationsCount);
    EXPECT_EQ(queuedAllocationsCount, svmManager->getNumDeferFreeAllocs());

    const auto freeGraphicsMemoryCalledBeforeDrain = memoryManager->freeGraphicsMemoryCalled.load();
    letDeferredFreeEntriesBeFreed();
    svmManager->freeSVMAllocDeferImpl();

    EXPECT_EQ(0ul, svmManager->getNumDeferFreeAllocs());
    expectEveryQueuedPointerFreedOnce(freeGraphicsMemoryCalledBeforeDrain);
}

TEST_F(SvmDeferredFreeQueueTest, givenEntriesClaimedByOneThreadWhenAnotherThreadDrainsAllBlockingThenItWaitsForTheClaimedEntries) {
    constexpr size_t queuedAllocationsCount = 8;
    allocateAndQueueForDeferredFree(queuedAllocationsCount);
    if (HasFatalFailure()) {
        return;
    }
    letDeferredFreeEntriesBeFreed();

    const auto freeGraphicsMemoryCalledBeforeDrain = memoryManager->freeGraphicsMemoryCalled.load();

    AllocInUseParkingLot parkingLot;
    memoryManager->allocInUseCallback = [&]() { parkingLot.parkFirstCallerUntilReleased(); };

    std::thread claimingDrain([&]() { svmManager->freeSVMAllocDeferImpl(); });
    EXPECT_TRUE(parkingLot.waitUntilParked());

    const auto claimableWhileClaimed = svmManager->getNumClaimableDeferFreeAllocs();
    const auto outstandingWhileClaimed = svmManager->getNumDeferFreeAllocs();
    parkingLot.release();

    EXPECT_EQ(0ul, claimableWhileClaimed);
    EXPECT_EQ(queuedAllocationsCount, outstandingWhileClaimed);

    svmManager->drainAllDeferFreeAllocsBlocking();

    expectEveryQueuedPointerFreedOnce(freeGraphicsMemoryCalledBeforeDrain);

    claimingDrain.join();
    memoryManager->allocInUseCallback = nullptr;
}
