/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace NEO;

TEST(DrmMemoryOperationsHandlerBindMtTest, givenCompressedAllocationWhenThreadsDecompressItAtOnceThenEveryCallSucceedsAndBindsAreIssuedOnce) {
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get());
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto drm = new DrmMockWithCaptureHelper(rootDeviceEnvironment);
    drm->setBindAvailable();
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    rootDeviceEnvironment.memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandlerBind>(rootDeviceEnvironment, 0u);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0u));
    auto operationsHandler = rootDeviceEnvironment.memoryOperationsInterface.get();

    MockBufferObject bo(0u, drm);
    bo.setSize(MemoryConstants::pageSize);
    DrmAllocation allocation(0u, 1u, AllocationType::buffer, &bo, nullptr, 0x10000, MemoryConstants::pageSize, MemoryPool::localMemory);
    allocation.setGmm(new MockGmm(device->getGmmHelper()), 0u);
    allocation.getDefaultGmm()->setCompressionEnabled(true);

    auto bindsPerDecompression = static_cast<uint32_t>(device->getAllEngines()[0].osContext->getDeviceBitfield().count());
    auto vmBindCalledBefore = drm->getMockIoctlHelper()->vmBindCalled;

    constexpr uint32_t numThreads = 8u;
    std::atomic<bool> start{false};
    std::atomic<uint32_t> succeeded{0u};
    std::vector<std::thread> threads;
    for (uint32_t i = 0u; i < numThreads; i++) {
        threads.emplace_back([&] {
            while (!start) {
                std::this_thread::yield();
            }
            if (operationsHandler->decompress(device.get(), allocation) == MemoryOperationsStatus::success) {
                succeeded++;
            }
        });
    }
    start = true;
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(numThreads, succeeded.load());
    EXPECT_EQ(vmBindCalledBefore + bindsPerDecompression, drm->getMockIoctlHelper()->vmBindCalled);
    EXPECT_FALSE(allocation.isCompressionEnabled());

    delete allocation.getDefaultGmm();
    allocation.setGmm(nullptr, 0u);
}
