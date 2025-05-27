/*
 * Copyright (C) 2018-2025 Intel Corporation
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
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_kmdaf_listener.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class WddmWithKmDafMock : public Wddm {
  public:
    using Wddm::featureTable;
    using Wddm::mapGpuVirtualAddress;

    WddmWithKmDafMock(RootDeviceEnvironment &rootDeviceEnvironment)
        : Wddm(std::make_unique<HwDeviceIdWddm>(ADAPTER_HANDLE, LUID{}, 1u, rootDeviceEnvironment.executionEnvironment.osEnvironment.get(), std::make_unique<UmKmDataTranslator>()),
               rootDeviceEnvironment) {
        kmDafListener.reset(new KmDafListenerMock);
    }

    KmDafListenerMock &getKmDafListenerMock() {
        return static_cast<KmDafListenerMock &>(*this->kmDafListener);
    }
};

class WddmKmDafListenerTest : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment.initializeMemoryManager();
        rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
        auto osEnvironment = new OsEnvironmentWin();
        osEnvironment->gdi.reset(new MockGdi());
        executionEnvironment.osEnvironment.reset(osEnvironment);
        wddmWithKmDafMock = new WddmWithKmDafMock(*rootDeviceEnvironment);
        wddmWithKmDafMock->init();
        wddmWithKmDafMock->featureTable->flags.ftrKmdDaf = true;
    }
    void TearDown() override {
    }

    MockExecutionEnvironment executionEnvironment{};
    WddmWithKmDafMock *wddmWithKmDafMock = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

TEST_F(WddmKmDafListenerTest, givenWddmWhenLockResourceIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto handle = 0u;
    auto resourceHandle = 0u;
    auto ptr = reinterpret_cast<void *>(0x10000);

    wddmWithKmDafMock->createAllocation(ptr, gmm.get(), handle, resourceHandle, nullptr);

    wddmWithKmDafMock->lockResource(handle, false, 0x1000);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);

    wddmWithKmDafMock->destroyAllocations(&handle, 1, 0);
}

TEST_F(WddmKmDafListenerTest, givenInvalidAllocationWhenLockResourceFailsThenKmDafListenerIsNotNotified) {
    wddmWithKmDafMock->lockResource(0, false, 0x1000);

    EXPECT_FALSE(wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenUnlockResourceIsCalledThenKmDafListenerNotifyUnlockIsFedWithCorrectParams) {
    wddmWithKmDafMock->unlockResource(ALLOCATION_HANDLE, false);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, *wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.pfnEscape);
}
TEST_F(WddmKmDafListenerTest, givenInvalidAllocationWhenUnlockResourceIsCalledThenKmDafListenerIsNotNotified) {
    wddmWithKmDafMock->unlockResource(0, false);

    EXPECT_FALSE(wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.ftrKmdDaf);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAdapter);
    EXPECT_EQ(0u, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hDevice);
    EXPECT_EQ(std::nullopt, wddmWithKmDafMock->getKmDafListenerMock().notifyUnlockParametrization.hAllocation);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMapGpuVirtualAddressIsCalledThenKmDafListenerNotifyMapGpuVAIsFedWithCorrectParams) {
    uint64_t gpuPtr = 0u;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    wddmWithKmDafMock->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddmWithKmDafMock->getGfxPartition().Standard.Base,
                                            wddmWithKmDafMock->getGfxPartition().Standard.Limit, 0u, gpuPtr, AllocationType::unknown);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);

    auto gmmHelper = rootDeviceEnvironment->gmmHelper.get();
    EXPECT_EQ(gmmHelper->decanonize(gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.gpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenFreeGpuVirtualAddressIsCalledThenKmDafListenerNotifyUnmapGpuVAIsFedWithCorrectParams) {
    uint64_t gpuPtr = GPUVA;

    wddmWithKmDafMock->freeGpuVirtualAddress(gpuPtr, MemoryConstants::pageSize);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.hDevice);
    EXPECT_EQ(GPUVA, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.gpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyUnmapGpuVAParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenMakeResidentIsCalledThenKmDafListenerNotifyMakeResidentIsFedWithCorrectParams) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());

    wddmWithKmDafMock->makeResident(&allocation.handle, 1, false, nullptr, 0x1000);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyMakeResidentParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenEvictIsCalledThenKmDafListenerNotifyEvictIsFedWithCorrectParams) {
    MockWddmAllocation allocation(rootDeviceEnvironment->getGmmHelper());
    uint64_t sizeToTrim;

    wddmWithKmDafMock->evict(&allocation.handle, 1, sizeToTrim, true);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.hDevice);
    EXPECT_EQ(allocation.handle, *wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.phAllocation);
    EXPECT_EQ(1u, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.allocations);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyEvictParametrization.pfnEscape);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationIsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto handle = 0u;
    auto resourceHandle = 0u;
    auto ptr = reinterpret_cast<void *>(0x10000);

    wddmWithKmDafMock->createAllocation(ptr, gmm.get(), handle, resourceHandle, nullptr);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);

    wddmWithKmDafMock->destroyAllocations(&handle, 1, 0);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocation64IsCalledThenKmDafListenerNotifyWriteTargetIsFedWithCorrectParams) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto handle = 0u;

    wddmWithKmDafMock->createAllocation(gmm.get(), handle);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);

    wddmWithKmDafMock->destroyAllocations(&handle, 1, 0);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenCreateAllocationsAndMapGpuVaIsCalledThenKmDafListenerNotifyWriteTargetAndMapGpuVAIsFedWithCorrectParams) {
    OsHandleStorage storage;
    OsHandleWin osHandle;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(rootDeviceEnvironment->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    storage.fragmentStorageData[0].osHandleStorage = &osHandle;
    storage.fragmentStorageData[0].fragmentSize = 100;
    static_cast<OsHandleWin *>(storage.fragmentStorageData[0].osHandleStorage)->gmm = gmm.get();

    wddmWithKmDafMock->createAllocationsAndMapGpuVa(storage);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.hAllocation);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyWriteTargetParametrization.pfnEscape);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hDevice);
    EXPECT_EQ(osHandle.handle, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.hAllocation);

    auto gmmHelper = rootDeviceEnvironment->gmmHelper.get();
    EXPECT_EQ(gmmHelper->decanonize(osHandle.gpuPtr), wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.gpuVirtualAddress);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyMapGpuVAParametrization.pfnEscape);

    wddmWithKmDafMock->destroyAllocations(&osHandle.handle, 1, 0);
}

TEST_F(WddmKmDafListenerTest, givenWddmWhenKmDafLockIsCalledThenKmDafListenerNotifyLockIsFedWithCorrectParams) {
    wddmWithKmDafMock->kmDafLock(ALLOCATION_HANDLE);

    EXPECT_EQ(wddmWithKmDafMock->featureTable->flags.ftrKmdDaf, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.ftrKmdDaf);
    EXPECT_EQ(wddmWithKmDafMock->getAdapter(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAdapter);
    EXPECT_EQ(wddmWithKmDafMock->getDeviceHandle(), wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hDevice);
    EXPECT_EQ(ALLOCATION_HANDLE, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.hAllocation);
    EXPECT_EQ(0, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pLockFlags);
    EXPECT_EQ(wddmWithKmDafMock->getGdi()->escape.mFunc, wddmWithKmDafMock->getKmDafListenerMock().notifyLockParametrization.pfnEscape);
}
