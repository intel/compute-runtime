/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mock_gdi/mock_gdi.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_kmdaf_listener.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"

using namespace OCLRT;

class WddmWithKmDafMock : public Wddm {
  public:
    using Wddm::featureTable;
    using Wddm::gdi;
    using Wddm::mapGpuVirtualAddressImpl;

    WddmWithKmDafMock() : Wddm() {
        kmDafListener.reset(new KmDafListenerMock);
    }

    KmDafListenerMock &getKmDafListenerMock() {
        return static_cast<KmDafListenerMock &>(*this->kmDafListener);
    }
};

class WddmKmDafListenerTest : public ::testing::Test {
  public:
    void SetUp() {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        wddmWithKmDafMock.reset(new WddmWithKmDafMock());
        wddmWithKmDafMock->gdi.reset(new MockGdi());
        wddmWithKmDafMock->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
        wddmWithKmDafMock->featureTable->ftrKmdDaf = true;
    }
    void TearDown() {
    }

    std::unique_ptr<WddmWithKmDafMock> wddmWithKmDafMock;
    ExecutionEnvironment *executionEnvironment;
};

TEST_F(WddmKmDafListenerTest, givenWddmWhenLockResourceIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    wddmWithKmDafMock->lockResource(ALLOCATION_HANDLE, false);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenUnlockResourceIsCalledThenKmDafListenerNotifyUnlockIsFedWithCorrectParams) {
    wddmWithKmDafMock->unlockResource(ALLOCATION_HANDLE);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, *wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMapGpuVirtualAddressIsCalledThenKmDafListenerNotifyMapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation(GraphicsAllocation::AllocationType::UNDECIDED, (void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    allocation.setDefaultHandle(ALLOCATION_HANDLE);
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.setDefaultGmm(gmm.get());

    auto heapIndex = MemoryManager::selectHeap(&allocation, allocation.getUnderlyingBuffer(), *platformDevices[0]);
    wddmWithKmDafMock->mapGpuVirtualAddressImpl(allocation.getDefaultGmm(), allocation.getDefaultHandle(), allocation.getUnderlyingBuffer(), allocation.getGpuAddressToModify(), heapIndex);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(allocation.getDefaultHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(allocation.getGpuAddress()), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenFreeGpuVirtualAddressIsCalledThenKmDafListenerNotifyUnmapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation(GraphicsAllocation::AllocationType::UNDECIDED, (void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    auto &gpuPtr = allocation.getGpuAddressToModify();
    gpuPtr = GPUVA;

    wddmWithKmDafMock->freeGpuVirtualAddress(gpuPtr, allocation.getUnderlyingBufferSize());

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hDevice);
    EXPECT_EQ(GPUVA, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMakeResidentIsCalledThenKmDafListenerNotifyMakeResidentIsFedWithCorrectParams) {
    MockWddmAllocation allocation;

    wddmWithKmDafMock->makeResident(&allocation.handle, 1, false, nullptr);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenEvictIsCalledThenKmDafListenerNotifyEvictIsFedWithCorrectParams) {
    MockWddmAllocation allocation;
    uint64_t sizeToTrim;

    wddmWithKmDafMock->evict(&allocation.handle, 1, sizeToTrim);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationIsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    WddmAllocation allocation(GraphicsAllocation::AllocationType::UNDECIDED, (void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.setDefaultGmm(gmm.get());
    auto &handle = allocation.getHandleToModify(0u);

    wddmWithKmDafMock->createAllocation(allocation.getAlignedCpuPtr(), gmm.get(), handle);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocation64IsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    WddmAllocation allocation(GraphicsAllocation::AllocationType::UNDECIDED, (void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.setDefaultGmm(gmm.get());
    auto &handle = allocation.getHandleToModify(0u);

    wddmWithKmDafMock->createAllocation64k(gmm.get(), handle);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationsAndMapGpuVaIsCalledThenKmDafListenerNotifyWriteTargetAndMapGpuVAIsFedWithCorrectParams) {
    OsHandleStorage storage;
    OsHandle osHandle = {0};
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    storage.fragmentStorageData[0].osHandleStorage = &osHandle;
    storage.fragmentStorageData[0].fragmentSize = 100;
    storage.fragmentStorageData[0].osHandleStorage->gmm = gmm.get();

    wddmWithKmDafMock->createAllocationsAndMapGpuVa(storage);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(osHandle.gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenKmDafLockIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    wddmWithKmDafMock->kmDafLock(ALLOCATION_HANDLE);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}
