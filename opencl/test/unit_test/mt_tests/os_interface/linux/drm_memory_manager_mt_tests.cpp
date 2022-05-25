/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>

using namespace NEO;

TEST(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSharedAllocationIsCreatedFromMultipleThreadsThenSingleBoIsReused) {
    class MockDrm : public Drm {
      public:
        using Drm::setupIoctlHelper;
        MockDrm(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {}

        int ioctl(DrmIoctl request, void *arg) override {
            if (request == DrmIoctl::PrimeFdToHandle) {
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
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);

    osHandle handle = 3;
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    std::thread threads[maxThreads];
    std::atomic<size_t> index(0);
    std::atomic<size_t> allocateCount(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
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

TEST(DrmMemoryManagerTest, givenMultipleThreadsWhenSharedAllocationIsCreatedThenPrimeFdToHandleDoesNotRaceWithClose) {
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
            if (request == DrmIoctl::PrimeFdToHandle) {
                auto *primeToHandleParams = static_cast<PrimeHandle *>(arg);
                primeToHandleParams->handle = primeFdHandle;

                // PrimeFdHandle should not be lower than closeHandle
                // GemClose shouldn't be executed concurrently with primtFdToHandle
                EXPECT_EQ(closeHandle.load(), primeFdHandle.load());
            }

            else if (request == DrmIoctl::GemClose) {
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
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);

    osHandle handle = 3;
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    std::thread threads[maxThreads];
    std::atomic<size_t> index(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
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
