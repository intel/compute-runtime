/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace L0 {
namespace ult {

struct SVMAllocsManagerMock : public NEO::SVMAllocsManager {
    using SVMAllocsManager::mtxForIndirectAccess;
    SVMAllocsManagerMock(NEO::MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void makeIndirectAllocationsResident(NEO::CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount) override {
        makeIndirectAllocationsResidentCalledTimes++;
    }
    void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                    NEO::ResidencyContainer &residencyContainer,
                                                    uint32_t requestedTypesMask) override {
        addInternalAllocationsToResidencyContainerCalledTimes++;
        passedContainer = residencyContainer.data();
    }
    uint32_t makeIndirectAllocationsResidentCalledTimes = 0;
    uint32_t addInternalAllocationsToResidencyContainerCalledTimes = 0;
    NEO::GraphicsAllocation **passedContainer = nullptr;
};

using CommandQueueCreateMt = Test<DeviceFixture>;

TEST_F(CommandQueueCreateMt, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmiPackDisabeldThenSVMAllocsMtxIsLocked) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto prevSvmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::unique_lock<std::mutex> lock;
    auto mockSvmAllocsManager = std::make_unique<SVMAllocsManagerMock>(device->getDriverHandle()->getMemoryManager());
    reinterpret_cast<WhiteBox<::L0::DriverHandle> *>(device->getDriverHandle())->svmAllocsManager = mockSvmAllocsManager.get();

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    std::thread th([&] {
        EXPECT_FALSE(mockSvmAllocsManager->mtxForIndirectAccess.try_lock());
    });
    th.join();
    reinterpret_cast<WhiteBox<::L0::DriverHandle> *>(device->getDriverHandle())->svmAllocsManager = prevSvmAllocsManager;
    lock.unlock();
    commandQueue->destroy();
}

struct CommandQueueCsrReuseMtTests : public ::testing::Test {
    void SetUp() override {
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
        if (hwInfo.capabilityTable.defaultEngineType != aub_stream::ENGINE_CCS) {
            GTEST_SKIP();
        }
        debugManager.flags.ContextGroupSize.set(5);
        debugManager.flags.OverrideNumHighPriorityContexts.set(1);
        hwInfo.featureTable.flags.ftrCCSNode = true;
        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        ASSERT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));
        device = driverHandle->devices[0];
    }

    DebugManagerStateRestore dbgRestorer;
    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    L0::Device *device = nullptr;
};

TEST_F(CommandQueueCsrReuseMtTests, givenReleasedCsrWhenCreatingQueuesConcurrentlyThenUseDifferentFreeCsrs) {
    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t initialHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &initialHandle));
    L0::CommandQueue::fromHandle(initialHandle)->destroy();

    auto count = neoDevice->secondaryEngines[aub_stream::ENGINE_CCS].regularEnginesTotal;
    std::vector<ze_command_queue_handle_t> handles(count, nullptr);
    std::vector<ze_result_t> results(count, ZE_RESULT_ERROR_UNKNOWN);
    std::vector<std::thread> threads;
    std::atomic<uint32_t> ready = 0;
    for (uint32_t i = 0; i < count; i++) {
        threads.emplace_back([&, i] {
            ready.fetch_add(1);
            while (ready.load() < count) {
                std::this_thread::yield();
            }
            results[i] = device->createCommandQueue(&desc, &handles[i]);
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, results[i]);
        if (handles[i]) {
            auto queue = L0::CommandQueue::fromHandle(handles[i]);
            EXPECT_EQ(1u, queue->getCsr()->getOwningQueueCount());
            for (uint32_t j = 0; j < i; j++) {
                if (handles[j]) {
                    EXPECT_NE(queue->getCsr(), L0::CommandQueue::fromHandle(handles[j])->getCsr());
                }
            }
        }
    }
    for (auto handle : handles) {
        if (handle) {
            L0::CommandQueue::fromHandle(handle)->destroy();
        }
    }
}

} // namespace ult
} // namespace L0
