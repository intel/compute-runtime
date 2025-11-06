/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

namespace L0 {
namespace ult {

struct MultiDeviceQueryPeerAccessDrmFixture {
    void setUp() {
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);

        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; ++i) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new OSInterface());
            auto drm = new DrmMock{*executionEnvironment->rootDeviceEnvironments[i]};
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
        }

        driverHandle = std::make_unique<Mock<DriverHandle>>();
        driverHandle->importFdHandleResult = reinterpret_cast<void *>(0x1234);

        for (auto &device : driverHandle->devices) {
            delete device;
        }
        driverHandle->devices.clear();

        auto devices = NEO::DeviceFactory::createDevices(*executionEnvironment);
        ze_result_t res = driverHandle->initialize(std::move(devices));
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ASSERT_EQ(numRootDevices, driverHandle->devices.size());
        device0 = driverHandle->devices[0];
        device1 = driverHandle->devices[1];
        ASSERT_NE(nullptr, device0);
        ASSERT_NE(nullptr, device1);
    }

    void tearDown() {}

    struct TempAlloc {
        NEO::Device &neoDevice;
        void *handlePtr = nullptr;
        uint64_t handle = std::numeric_limits<uint64_t>::max();

        TempAlloc(NEO::Device &device) : neoDevice(device) {}
        ~TempAlloc() {
            if (handlePtr) {
                DeviceImp::freeMemoryAllocation(neoDevice, handlePtr);
            }
        }
    };

    uint32_t numRootDevices = 2u;
    L0::Device *device0 = nullptr;
    L0::Device *device1 = nullptr;

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<DriverHandle>> driverHandle;
};

using MultipleDeviceQueryPeerAccessDrmTests = Test<MultiDeviceQueryPeerAccessDrmFixture>;

TEST_F(MultipleDeviceQueryPeerAccessDrmTests, givenTwoDevicesWhenCanAccessPeerIsCalledManyTimesFromMultiThreadsInBothWaysThenPeerAccessIsQueriedOnlyOnce) {
    std::atomic<uint32_t> queryCalled = 0;

    auto queryPeerAccess = [&queryCalled](NEO::Device &device, NEO::Device &peerDevice, void **handlePtr, uint64_t *handle) -> bool {
        queryCalled++;
        return DeviceImp::queryPeerAccess(device, peerDevice, handlePtr, handle);
    };

    std::atomic_bool started = false;
    constexpr int numThreads = 8;
    constexpr int iterationCount = 20;
    std::vector<std::thread> threads;

    auto threadBody = [&](int threadId) {
        while (!started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        auto device = device0;
        auto peerDevice = device1;

        if (threadId & 1) {
            device = device1;
            peerDevice = device0;
        }
        for (auto i = 0; i < iterationCount; i++) {
            bool canAccess = device->getNEODevice()->canAccessPeer(queryPeerAccess, DeviceImp::freeMemoryAllocation, peerDevice->getNEODevice());
            EXPECT_TRUE(canAccess);
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadBody, i);
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(1u, queryCalled);
}
} // namespace ult
} // namespace L0