/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/os_interface.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "unit_tests/mocks/linux/mock_drm_memory_manager.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>

using namespace NEO;
using namespace std;

TEST(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSharedAllocationIsCreatedFromMultipleThreadsThenSingleBoIsReused) {
    class MockDrm : public Drm {
      public:
        MockDrm(int fd) : Drm(fd) {}

        int ioctl(unsigned long request, void *arg) override {
            if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
                auto *primeToHandleParams = (drm_prime_handle *)arg;
                primeToHandleParams->handle = 10;
            }
            return 0;
        }
    };
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto mock = make_unique<MockDrm>(0);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(mock.get());
    auto memoryManager = make_unique<TestedDrmMemoryManager>(executionEnvironment);

    osHandle handle = 3;
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    thread threads[maxThreads];
    atomic<size_t> index(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
        EXPECT_NE(nullptr, createdAllocations[indexFree]);
    };

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(createFunction);
    }

    while (index < maxThreads) {
        EXPECT_GE(1u, memoryManager->sharingBufferObjects.size());
    }

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
        memoryManager->freeGraphicsMemory(createdAllocations[i]);
    }
}

TEST(DrmMemoryManagerTest, givenMultipleThreadsWhenSharedAllocationIsCreatedThenPrimeFdToHandleDoesNotRaceWithClose) {
    class MockDrm : public Drm {
      public:
        MockDrm(int fd) : Drm(fd) {
            primeFdHandle = 1;
            closeHandle = 1;
        }
        atomic<int> primeFdHandle;
        atomic<int> closeHandle;

        int ioctl(unsigned long request, void *arg) override {
            if (request == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
                auto *primeToHandleParams = (drm_prime_handle *)arg;
                primeToHandleParams->handle = primeFdHandle;

                // PrimeFdHandle should not be lower than closeHandle
                // GemClose shouldn't be executed concurrently with primtFdToHandle
                EXPECT_EQ(closeHandle.load(), primeFdHandle.load());
            }

            else if (request == DRM_IOCTL_GEM_CLOSE) {
                closeHandle++;
                this_thread::yield();

                primeFdHandle.store(closeHandle.load());
            }

            return 0;
        }
    };

    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto mock = make_unique<MockDrm>(0);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->setDrm(mock.get());
    auto memoryManager = make_unique<TestedDrmMemoryManager>(executionEnvironment);

    osHandle handle = 3;
    constexpr size_t maxThreads = 10;

    GraphicsAllocation *createdAllocations[maxThreads];
    thread threads[maxThreads];
    atomic<size_t> index(0);

    auto createFunction = [&]() {
        size_t indexFree = index++;
        AllocationProperties properties(0, false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
        createdAllocations[indexFree] = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false);
        EXPECT_NE(nullptr, createdAllocations[indexFree]);

        this_thread::yield();

        memoryManager->freeGraphicsMemory(createdAllocations[indexFree]);
    };

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i] = std::thread(createFunction);
    }

    for (size_t i = 0; i < maxThreads; i++) {
        threads[i].join();
    }
}
