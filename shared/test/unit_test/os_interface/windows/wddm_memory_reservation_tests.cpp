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
    using MemoryManager::reserveGpuAddressOnHeap;
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
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto gmmHelper = memManager->getGmmHelper(0);
    auto addressRange = memManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);

    addressRange = memManager->reserveGpuAddress(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);
}

TEST_F(WddmMemoryReservationTests, givenWddmMemoryManagerWhenGpuAddressIsReservedWithInvalidStartAddresThenDifferentAddressReserved) {
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    uint64_t invalidAddress = 0x1234;
    auto gmmHelper = memManager->getGmmHelper(0);
    auto addressRange = memManager->reserveGpuAddressOnHeap(invalidAddress, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);
    EXPECT_NE(invalidAddress, addressRange.address);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);

    addressRange = memManager->reserveGpuAddress(invalidAddress, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_NE(invalidAddress, addressRange.address);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);
}

TEST_F(WddmMemoryReservationTests, givenWddmMemoryManagerWhenGpuAddressIsReservedWithValidStartAddresThenSameAddressReserved) {
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto gmmHelper = memManager->getGmmHelper(0);
    auto addressRange = memManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);
    auto previousAddress = addressRange.address;
    memManager->freeGpuAddress(addressRange, 0);
    auto newAddressRange = memManager->reserveGpuAddressOnHeap(previousAddress, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);
    EXPECT_EQ(previousAddress, addressRange.address);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    memManager->freeGpuAddress(addressRange, 0);

    addressRange = memManager->reserveGpuAddress(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);
    previousAddress = addressRange.address;
    memManager->freeGpuAddress(addressRange, 0);
    newAddressRange = memManager->reserveGpuAddress(previousAddress, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(previousAddress, addressRange.address);

    EXPECT_EQ(rootDeviceIndexReserved, 0u);
    EXPECT_LE(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard64KB), gmmHelper->decanonize(addressRange.address));
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

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 1;
    auto addressRange = memManager->reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved, HeapIndex::heapStandard64KB, MemoryConstants::pageSize64k);
    EXPECT_EQ(addressRange.address, 0ull);
    EXPECT_EQ(addressRange.size, 0u);
    addressRange = memManager->reserveGpuAddress(0ull, MemoryConstants::pageSize64k, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(addressRange.address, 0ull);
    EXPECT_EQ(addressRange.size, 0u);
}
} // namespace NEO
