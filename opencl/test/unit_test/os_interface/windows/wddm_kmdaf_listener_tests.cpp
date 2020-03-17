/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mock_gdi/mock_gdi.h"
#include "opencl/test/unit_test/os_interface/windows/mock_kmdaf_listener.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
#include "test.h"

using namespace NEO;

class WddmWithKmDafMock : public Wddm {
  public:
    using Wddm::featureTable;
    using Wddm::mapGpuVirtualAddress;

    WddmWithKmDafMock(RootDeviceEnvironment &rootDeviceEnvironment) : Wddm(std::make_unique<HwDeviceId>(ADAPTER_HANDLE, LUID{}, rootDeviceEnvironment.executionEnvironment.osEnvironment.get()), rootDeviceEnvironment) {
        kmDafListener.reset(new KmDafListenerMock);
    }

    KmDafListenerMock &getKmDafListenerMock() {
        return static_cast<KmDafListenerMock &>(*this->kmDafListener);
    }
};

class WddmKmDafListenerTest : public ::testing::Test {
  public:
    void SetUp() {
        executionEnvironment = platform()->peekExecutionEnvironment();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        auto osEnvironment = new OsEnvironmentWin();
        osEnvironment->gdi.reset(new MockGdi());
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddmWithKmDafMock.reset(new WddmWithKmDafMock(*rootDeviceEnvironment));
        wddmWithKmDafMock->init();
        wddmWithKmDafMock->featureTable->ftrKmdDaf = true;
    }
    void TearDown() {
    }

    std::unique_ptr<WddmWithKmDafMock> wddmWithKmDafMock;
    ExecutionEnvironment *executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

TEST_F(WddmKmDafListenerTest, givenWddmWhenLockResourceIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    wddmWithKmDafMock->lockResource(ALLOCATION_HANDLE, false, 0x1000);

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
    uint64_t gpuPtr = 0u;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 1, false);

    wddmWithKmDafMock->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddmWithKmDafMock->getGfxPartition().Standard.Base,
                                            wddmWithKmDafMock->getGfxPartition().Standard.Limit, 0u, gpuPtr);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);
    EXPECT_EQ(GmmHelper::decanonize(gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenFreeGpuVirtualAddressIsCalledThenKmDafListenerNotifyUnmapGpuVAIsFedWithCorrectParams) {
    uint64_t gpuPtr = GPUVA;

    wddmWithKmDafMock->freeGpuVirtualAddress(gpuPtr, MemoryConstants::pageSize);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hDevice);
    EXPECT_EQ(GPUVA, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.GpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMakeResidentIsCalledThenKmDafListenerNotifyMakeResidentIsFedWithCorrectParams) {
    MockWddmAllocation allocation;

    wddmWithKmDafMock->makeResident(&allocation.handle, 1, false, nullptr, 0x1000);

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
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 1, false);
    auto handle = 0u;
    auto resourceHandle = 0u;
    auto ptr = reinterpret_cast<void *>(0x10000);

    wddmWithKmDafMock->createAllocation(ptr, gmm.get(), handle, resourceHandle, nullptr);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDevice(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocation64IsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 1, false);
    auto handle = 0u;

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
    auto gmm = std::unique_ptr<Gmm>(new Gmm(rootDeviceEnvironment->getGmmClientContext(), nullptr, 1, false));
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
