/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

struct MemoryReservationMock : public MockWddmMemoryManager {
    using MemoryManager::freeGpuAddress;
    using MemoryManager::reserveGpuAddress;
    MemoryReservationMock(NEO::ExecutionEnvironment &executionEnvironment) : MockWddmMemoryManager(executionEnvironment) {}
};

class WddmMemManagerFixture {
  public:
    void setUp() {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
        auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        wddm->callBaseMapGpuVa = false;
        memManager = std::unique_ptr<MemoryReservationMock>(new MemoryReservationMock(*executionEnvironment));
    }
    void tearDown() {
    }
    std::unique_ptr<MemoryReservationMock> memManager;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

typedef ::Test<WddmMemManagerFixture> WddmMemoryReservationTests;

TEST_F(WddmMemoryReservationTests, givenWddmMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressFromGfxPartitionIsUsed) {
    RootDeviceIndicesContainer rootDevices;
    rootDevices.push_back(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto addressRange = memManager->reserveGpuAddress(nullptr, MemoryConstants::pageSize64k, rootDevices, &rootDeviceIndexReserved);
    auto gmmHelper = memManager->getGmmHelper(0);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_STANDARD64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_STANDARD64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);
}

TEST(WddmMemoryReservationFailTest, givenWddmMemoryManagerWhenGpuAddressReservationIsAttemptedThenNullPtrAndSizeReturned) {
    std::unique_ptr<MemoryReservationMock> memManager;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;

    executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->callBaseMapGpuVa = false;
    wddm->failReserveGpuVirtualAddress = true;
    memManager = std::unique_ptr<MemoryReservationMock>(new MemoryReservationMock(*executionEnvironment));

    RootDeviceIndicesContainer rootDevices;
    rootDevices.push_back(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto addressRange = memManager->reserveGpuAddress(nullptr, MemoryConstants::pageSize64k, rootDevices, &rootDeviceIndexReserved);
    EXPECT_EQ(addressRange.address, 0ull);
    EXPECT_EQ(addressRange.size, 0u);
}
} // namespace NEO
