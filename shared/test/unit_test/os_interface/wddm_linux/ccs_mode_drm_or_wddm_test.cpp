/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"

#include "gmm_memory.h"

static uint32_t ccsMode = 1u;

struct MockWddmLinux : NEO::Wddm {
    MockWddmLinux(std::unique_ptr<NEO::HwDeviceIdWddm> hwDeviceId, NEO::RootDeviceEnvironment &rootDeviceEnvironment)
        : NEO::Wddm(std::move(hwDeviceId), rootDeviceEnvironment) {
        constexpr uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte; // 4GB
        gfxPartition.SVM.Base = 0;
        gfxPartition.SVM.Limit = MemoryConstants::max64BitAppAddress + 1;
        decltype(gfxPartition.SVM) *precHeap = &gfxPartition.SVM;
        for (auto heap : {&gfxPartition.Heap32[0], &gfxPartition.Heap32[1], &gfxPartition.Heap32[2], &gfxPartition.Heap32[3], &gfxPartition.Standard, &gfxPartition.Standard64KB}) {
            heap->Base = precHeap->Limit;
            heap->Limit = heap->Base + gfxHeap32Size;
            precHeap = heap;
        }
        UNRECOVERABLE_IF(!isAligned<NEO::GfxPartition::heapGranularity>(precHeap->Limit));
    }

    bool reserveValidAddressRange(size_t size, void *&reservedMem) override {
        if (validAddressBase + size > MemoryConstants::max64BitAppAddress) {
            validAddressRangeReservations.push_back(std::make_tuple(size, reservedMem, false));
            return false;
        }
        reservedMem = reinterpret_cast<void *>(validAddressBase);
        validAddressBase += size;
        validAddressRangeReservations.push_back(std::make_tuple(size, reservedMem, true));
        return true;
    }

    void releaseReservedAddress(void *reservedAddress) override {
        Wddm::releaseReservedAddress(reservedAddress);
        validAddressRangeReleases.push_back(reservedAddress);
    }

    uintptr_t validAddressBase = 128U * MemoryConstants::megaByte + 4U; // intentionally only DWORD alignment to improve alignment testing
    std::vector<std::tuple<size_t, void *, bool>> validAddressRangeReservations;
    std::vector<void *> validAddressRangeReleases;

    using Wddm::featureTable;
    using Wddm::getReadOnlyFlagValue;
    using Wddm::gfxPartition;
    using Wddm::gfxPlatform;
    using Wddm::gmmMemory;
    using Wddm::isReadOnlyFlagFallbackSupported;
};

struct WddmLinuxMockHwDeviceIdWddm : public NEO::HwDeviceIdWddm {
    using HwDeviceIdWddm::HwDeviceIdWddm;
    using HwDeviceIdWddm::umKmDataTranslator;
};

struct MockGmmMemoryWddmLinux : NEO::GmmMemory {
    MockGmmMemoryWddmLinux(NEO::GmmClientContext *gmmClientContext) : NEO::GmmMemory(gmmClientContext) {
    }

    bool setDeviceInfo(GMM_DEVICE_INFO *deviceInfo) override {
        return true;
    }
};

struct CcsModeWddmLinuxTest : public ::testing::Test {
    void SetUp() override {
        osEnvironment = std::make_unique<NEO::OsEnvironmentWin>();
        auto hwDeviceIdIn = std::make_unique<WddmLinuxMockHwDeviceIdWddm>(NULL_HANDLE, LUID{}, 1u, osEnvironment.get(), std::make_unique<NEO::UmKmDataTranslator>());
        this->hwDeviceId = hwDeviceIdIn.get();
        auto &rootDeviceEnvironment = *mockExecEnv.rootDeviceEnvironments[0];
        auto wddm = std::make_unique<MockWddmLinux>(std::move(hwDeviceIdIn), rootDeviceEnvironment);
        this->wddm = wddm.get();
        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();

        wddm->gmmMemory = std::make_unique<MockGmmMemoryWddmLinux>(gmmHelper->getClientContext());
        static_cast<PLATFORM &>(*wddm->gfxPlatform) = NEO::defaultHwInfo->platform;

        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::move(wddm));
    }

    std::unique_ptr<NEO::OsEnvironmentWin> osEnvironment;
    NEO::MockExecutionEnvironment mockExecEnv;
    MockWddmLinux *wddm = nullptr;
    WddmLinuxMockHwDeviceIdWddm *hwDeviceId = nullptr;
};

TEST_F(CcsModeWddmLinuxTest, GivenValidCcsModeWhenConfigureCcsModeIsCalledWithWddmDriverModelThenVerifyCcsModeIsNotConfigured) {
    VariableBackup<uint32_t> ccsModeBackup(&ccsMode);
    DebugManagerStateRestore restorer;
    debugManager.flags.ZEX_NUMBER_OF_CCS.set("2");

    std::unique_ptr<OSInterface> osInterface = std::make_unique<OSInterface>();

    mockExecEnv.configureCcsMode();
    EXPECT_EQ(1u, ccsMode);
}
