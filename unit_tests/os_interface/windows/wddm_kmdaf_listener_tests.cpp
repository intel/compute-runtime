/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/os_interface/windows/mock_kmdaf_listener.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/mock_gdi/mock_gdi.h"
#include "test.h"

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

class WddmKmDafListenerTest : public GmmEnvironmentFixture, public ::testing::Test {
  public:
    void SetUp() {
        GmmEnvironmentFixture::SetUp();
        wddmWithKmDafMock.reset(new WddmWithKmDafMock());
        wddmWithKmDafMock->gdi.reset(new MockGdi());
        wddmWithKmDafMock->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
        wddmWithKmDafMock->featureTable->ftrKmdDaf = true;
    }
    void TearDown() {
        GmmEnvironmentFixture::TearDown();
    }

    std::unique_ptr<WddmWithKmDafMock> wddmWithKmDafMock;
};

TEST_F(WddmKmDafListenerTest, givenWddmWhenLockResourceIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    MockWddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->lockResource(allocation);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenUnlockResourceIsCalledThenKmDafListenerNotifyUnlockIsFedWithCorrectParams) {
    MockWddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->unlockResource(allocation);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMapGpuVirtualAddressIsCalledThenKmDafListenerNotifyMapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    allocation.handle = ALLOCATION_HANDLE;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->mapGpuVirtualAddressImpl(allocation.gmm, allocation.handle, allocation.getUnderlyingBuffer(), allocation.gpuPtr, wddmWithKmDafMock->selectHeap(&allocation, allocation.getUnderlyingBuffer()));

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(allocation.gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenFreeGpuVirtualAddressIsCalledThenKmDafListenerNotifyUnmapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    allocation.gpuPtr = GPUVA;

    wddmWithKmDafMock->freeGpuVirtualAddress(allocation.gpuPtr, allocation.getUnderlyingBufferSize());

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
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->createAllocation(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocation64IsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr, MemoryPool::MemoryNull, false);
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->createAllocation64k(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
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
    MockWddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->kmDafLock(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}
