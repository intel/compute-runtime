/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_control.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <atomic>
#include <memory>
#include <thread>

namespace NEO {
extern bool disableBindDefaultInTests;
}
using namespace NEO;

struct DrmMemoryManagerMtTest : ::testing::Test {

    VariableBackup<bool> disableBindBackup{&disableBindDefaultInTests, false};

    struct MockDrm : public Drm {
        using Drm::setupIoctlHelper;

        MockDrm(int fd, RootDeviceEnvironment &rootDeviceEnvironment)
            : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {}

        int ioctl(DrmIoctl request, void *arg) override {
            if (request == DrmIoctl::primeFdToHandle) {
                static_cast<PrimeHandle *>(arg)->handle = uniqueHandles ? nextHandle.fetch_add(1u) : fixedHandle;
            }
            return 0;
        }

        uint32_t fixedHandle = 10u;
        bool uniqueHandles = false;
        std::atomic<uint32_t> nextHandle{1u};
    };
};

TEST_F(DrmMemoryManagerMtTest, givenDrmMemoryManagerWhenSharedAllocationIsCreatedFromMultipleThreadsThenSingleBoIsReused) {
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
    class MockDrmFdHandleClose : public Drm {
      public:
        using Drm::setupIoctlHelper;
        MockDrmFdHandleClose(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, ""), rootDeviceEnvironment) {
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
    auto mock = new MockDrmFdHandleClose(0, *executionEnvironment.rootDeviceEnvironments[0]);
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

HWTEST2_F(DrmMemoryManagerMtTest, givenMultipleUllsLightEnginesWhenFreeGraphicsMemoryCalledConcurrentlyThenNoLockInversion, IsAtMostXeCore) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto mock = new MockDrm(0, *executionEnvironment.rootDeviceEnvironments[0]);
    mock->uniqueHandles = true;
    mock->setupIoctlHelper(defaultHwInfo->platform.eProductFamily);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    executionEnvironment.memoryManager = std::make_unique<TestedDrmMemoryManager>(executionEnvironment);
    auto *memoryManager = static_cast<TestedDrmMemoryManager *>(executionEnvironment.memoryManager.get());

    auto *ctx0 = OsContext::create(executionEnvironment.rootDeviceEnvironments[0]->osInterface.get(), 0u, 0u,
                                   EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    auto *ctx1 = OsContext::create(executionEnvironment.rootDeviceEnvironments[0]->osInterface.get(), 0u, 1u,
                                   EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));
    ctx0->incRefInternal();
    ctx1->incRefInternal();
    ctx0->setDirectSubmissionActive();
    ctx1->setDirectSubmissionActive();

    MockCommandStreamReceiver csr0(executionEnvironment, 0u, DeviceBitfield(1u));
    MockCommandStreamReceiver csr1(executionEnvironment, 0u, DeviceBitfield(1u));

    memoryManager->allRegisteredEngines[0].push_back(EngineControl{&csr0, ctx0});
    memoryManager->allRegisteredEngines[0].push_back(EngineControl{&csr1, ctx1});

    AllocationProperties props(0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    TestedDrmMemoryManager::OsHandleData osHandle0{1u};
    TestedDrmMemoryManager::OsHandleData osHandle1{2u};
    auto *alloc0 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle0, props, false, false, true, nullptr);
    auto *alloc1 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle1, props, false, false, true, nullptr);
    ASSERT_NE(nullptr, alloc0);
    ASSERT_NE(nullptr, alloc1);

    std::atomic<int> ready{0};
    auto freeAlloc = [&](GraphicsAllocation *alloc) {
        ++ready;
        while (ready.load() < 2) {
        }
        memoryManager->freeGraphicsMemory(alloc);
    };

    std::thread t0(freeAlloc, alloc0);
    std::thread t1(freeAlloc, alloc1);
    t0.join();
    t1.join();
}
