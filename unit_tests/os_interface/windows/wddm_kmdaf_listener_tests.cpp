/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "unit_tests/os_interface/windows/mock_kmdaf_listener.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/mock_gdi/mock_gdi.h"
#include "test.h"

using namespace OCLRT;

class WddmWithKmDafMock : public Wddm {
  public:
    using Wddm::gdi;

    WddmWithKmDafMock() : Wddm() {
        kmDafListener.reset(new KmDafListenerMock);
    }

    KmDafListenerMock &getKmDafListenerMock() {
        return static_cast<KmDafListenerMock &>(*this->kmDafListener);
    }

    FeatureTable *getFeatureTable() {
        return featureTable.get();
    }

    bool mapGpuVirtualAddressImpl(Gmm *gmm, D3DKMT_HANDLE handle, void *cpuPtr, uint64_t size, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, bool allocation32bit, bool use64kbPages, bool useHeap1) override {
        return Wddm::mapGpuVirtualAddressImpl(gmm, handle, cpuPtr, size, gpuPtr, allocation32bit, use64kbPages, useHeap1);
    };
};

class WddmKmDafListenerTest : public ::testing::Test {
  public:
    void SetUp() {
        wddmWithKmDafMock.reset(new WddmWithKmDafMock());
        wddmWithKmDafMock->gdi.reset(new MockGdi());
        wddmWithKmDafMock->init<DEFAULT_TEST_FAMILY_NAME>();
        wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf = true;
    }

    std::unique_ptr<WddmWithKmDafMock> wddmWithKmDafMock;
};

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenLockResourceIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    WddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->lockResource(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenUnlockResourceIsCalledThenKmDafListenerNotifyUnlockIsFedWithCorrectParams) {
    WddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->unlockResource(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenMapGpuVirtualAddressIsCalledThenKmDafListenerNotifyMapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.handle = ALLOCATION_HANDLE;
    auto gmm = std::unique_ptr<Gmm>(GmmHelper::create(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->mapGpuVirtualAddressImpl(allocation.gmm, allocation.handle, allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), allocation.gpuPtr, false, false, false);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(allocation.gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenFreeGpuVirtualAddressIsCalledThenKmDafListenerNotifyUnmapGpuVAIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    allocation.gpuPtr = GPUVA;

    wddmWithKmDafMock->freeGpuVirtualAddres(allocation.gpuPtr, allocation.getUnderlyingBufferSize());

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hDevice);
    EXPECT_EQ(GPUVA, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenMakeResidentIsCalledThenKmDafListenerNotifyMakeResidentIsFedWithCorrectParams) {
    WddmAllocation allocation;

    wddmWithKmDafMock->makeResident(&allocation.handle, 1, false, nullptr);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenEvictIsCalledThenKmDafListenerNotifyEvictIsFedWithCorrectParams) {
    WddmAllocation allocation;
    uint64_t sizeToTrim;

    wddmWithKmDafMock->evict(&allocation.handle, 1, sizeToTrim);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationIsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    auto gmm = std::unique_ptr<Gmm>(GmmHelper::create(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->createAllocation(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocation64IsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    WddmAllocation allocation((void *)0x23000, 0x1000, nullptr);
    auto gmm = std::unique_ptr<Gmm>(GmmHelper::create(nullptr, 1, false));
    allocation.gmm = gmm.get();

    wddmWithKmDafMock->createAllocation64k(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationsAndMapGpuVaIsCalledThenKmDafListenerNotifyWriteTargetAndMapGpuVAIsFedWithCorrectParams) {
    OsHandleStorage storage;
    OsHandle osHandle = {0};
    auto gmm = std::unique_ptr<Gmm>(GmmHelper::create(nullptr, 1, false));
    storage.fragmentStorageData[0].osHandleStorage = &osHandle;
    storage.fragmentStorageData[0].fragmentSize = 100;
    storage.fragmentStorageData[0].osHandleStorage->gmm = gmm.get();

    wddmWithKmDafMock->createAllocationsAndMapGpuVa(storage);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(osHandle.gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

HWTEST_F(WddmKmDafListenerTest, givenWddmWhenKmDafLockIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    WddmAllocation allocation;
    allocation.handle = ALLOCATION_HANDLE;

    wddmWithKmDafMock->kmDafLock(&allocation);

    EXPECT_EQ(wddmWithKmDafMock->getFeatureTable()->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(allocation.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}
