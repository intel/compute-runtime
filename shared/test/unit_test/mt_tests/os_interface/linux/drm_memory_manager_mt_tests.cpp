/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace NEO {
extern bool disableBindDefaultInTests;
}
using namespace NEO;

struct DrmMemoryManagerMtTest : ::testing::Test {

    VariableBackup<bool> disableBindBackup{&disableBindDefaultInTests, false};
};

TEST_F(DrmMemoryManagerMtTest, givenDrmMemoryManagerWhenSharedAllocationIsCreatedFromMultipleThreadsThenSingleBoIsReused) {
    class MockDrm : public Drm {
      public:
        using Drm::setupIoctlHelper;
        MockDrm(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {}

        int ioctl(DrmIoctl request, void *arg) override {
            if (request == DrmIoctl::primeFdToHandle) {
                auto *primeToHandleParams = static_cast<PrimeHandle *>(arg);
                primeToHandleParams->handle = 10;
            }
            return 0;
        }
    };
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto mock = new MockDrm(0, *executionEnvironment.rootDeviceEnvironments[0]);
    mock->setupIoctlHelper(defaultHwInfo->platform.eProductFamily);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{3u};
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    std::thread threads[maxThreads];
    std::atomic<size_t> index(0);
    std::atomic<size_t> allocateCount(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        EXPECT_NE(nullptr, createdAllocations[indexFree]);
        EXPECT_GE(1u, memoryManager->peekSharedBosSize());
        allocateCount++;
    };

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(createFunction);
    }

    while (allocateCount < maxThreads) {
        EXPECT_GE(1u, memoryManager->peekSharedBosSize());
    }

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
        memoryManager->freeGraphicsMemory(createdAllocations[i]);
    }
}
TEST_F(DrmMemoryManagerMtTest, givenMultipleThreadsWhenSharedAllocationIsCreatedThenPrimeFdToHandleDoesNotRaceWithClose) {
    class MockDrm : public Drm {
      public:
        using Drm::setupIoctlHelper;
        MockDrm(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {
            primeFdHandle = 1;
            closeHandle = 1;
        }
        std::atomic<int> primeFdHandle;
        std::atomic<int> closeHandle;

        int ioctl(DrmIoctl request, void *arg) override {
            if (request == DrmIoctl::primeFdToHandle) {
                auto *primeToHandleParams = static_cast<PrimeHandle *>(arg);
                primeToHandleParams->handle = primeFdHandle;

                // PrimeFdHandle should not be lower than closeHandle
                // GemClose shouldn't be executed concurrently with primtFdToHandle
                EXPECT_EQ(closeHandle.load(), primeFdHandle.load());
            }

            else if (request == DrmIoctl::gemClose) {
                closeHandle++;
                std::this_thread::yield();

                primeFdHandle.store(closeHandle.load());
            }

            return 0;
        }
    };

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto mock = new MockDrm(0, *executionEnvironment.rootDeviceEnvironments[0]);
    mock->setupIoctlHelper(defaultHwInfo->platform.eProductFamily);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{3u};
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    std::thread threads[maxThreads];
    std::atomic<size_t> index(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        EXPECT_NE(nullptr, createdAllocations[indexFree]);

        std::this_thread::yield();

        memoryManager->freeGraphicsMemory(createdAllocations[indexFree]);
    };

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(createFunction);
    }

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
    }
}

TEST_F(DrmMemoryManagerMtTest, givenLocalMemoryReservationsFromMultipleThreadsWhenOnlySomeFitThenExactlyThoseSucceed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);

    constexpr size_t maxThreads = 10;
    constexpr size_t fittingReservations = 4;
    constexpr size_t allocationSize = MemoryConstants::pageSize64k;
    memoryManager->cachedMaxLocalMemory[0] = fittingReservations * allocationSize;

    std::vector<std::unique_ptr<DrmAllocation>> allocations;
    for (size_t i = 0; i < maxThreads; i++) {
        allocations.push_back(std::make_unique<DrmAllocation>(0u, 1u, AllocationType::buffer, nullptr, nullptr, 0u, allocationSize, MemoryPool::localMemory));
    }

    std::atomic<size_t> successfulReservations{0};
    std::vector<std::thread> threads;
    for (size_t i = 0; i < maxThreads; i++) {
        threads.emplace_back([&, i]() {
            if (memoryManager->reserveLocalMemory(*allocations[i])) {
                successfulReservations++;
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(fittingReservations, successfulReservations.load());
    EXPECT_EQ(fittingReservations * allocationSize, memoryManager->getUsedLocalMemorySize(0));

    for (auto &allocation : allocations) {
        memoryManager->releaseLocalMemoryReservation(*allocation);
    }
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(0));
}
