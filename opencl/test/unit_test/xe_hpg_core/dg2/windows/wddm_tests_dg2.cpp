/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

#include "shared/source/helpers/constants.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm_residency_allocations_container.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_memory_manager_tests.h"

#include "mock_gmm_memory.h"

HWTEST_EXCLUDE_PRODUCT(PlatformWithFourDevicesTest, givenPlatformWithFourDevicesWhenCreateBufferThenAllocationIsColouredAndHasFourHandles, IGFX_DG2);

struct PlatformWithFourDevicesTestDG2 : public ::testing::Test {
    PlatformWithFourDevicesTestDG2() {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    }
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(4);
        initPlatform();
    }

    DebugManagerStateRestore restorer;

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
};

DG2TEST_F(PlatformWithFourDevicesTestDG2, givenPlatformWithFourDevicesWhenCreateBufferThenAllocationIsColouredAndHasFourHandles) {
    auto wddm = reinterpret_cast<WddmMock *>(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    MockWddmMemoryManager memoryManager(true, true, *platform()->peekExecutionEnvironment());
    memoryManager.supportsMultiStorageResources = true;

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrMultiTileArch = true;

    auto allocation = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, 4 * MemoryConstants::pageSize64k, AllocationType::BUFFER, true, 0b1111}));
    EXPECT_EQ(4u, allocation->getNumGmms());
    EXPECT_EQ(maxNBitValue(allocation->getNumGmms()), allocation->storageInfo.pageTablesVisibility.to_ullong());

    EXPECT_FALSE(allocation->storageInfo.tileInstanced);
    EXPECT_TRUE(allocation->storageInfo.multiStorage);
    EXPECT_EQ(4u, wddm->mapGpuVirtualAddressResult.called);
    for (auto gmmIndex = 0u; gmmIndex < 4u; gmmIndex++) {
        auto gmm = allocation->getGmm(gmmIndex);
        EXPECT_EQ(allocation->storageInfo.pageTablesVisibility, gmm->resourceParams.MultiTileArch.GpuVaMappingSet);
        EXPECT_EQ(1u << gmmIndex, gmm->resourceParams.MultiTileArch.LocalMemEligibilitySet);
        EXPECT_EQ(1u << gmmIndex, gmm->resourceParams.MultiTileArch.LocalMemPreferredSet);
        EXPECT_EQ(MemoryConstants::pageSize64k, gmm->gmmResourceInfo->getSizeAllocation());
        EXPECT_FALSE(gmm->gmmResourceInfo->getResourceFlags()->Info.NonLocalOnly);
        if constexpr (is32bit) {
            EXPECT_FALSE(gmm->gmmResourceInfo->getResourceFlags()->Info.LocalOnly);
            EXPECT_FALSE(gmm->gmmResourceInfo->getResourceFlags()->Info.NotLockable);
        } else {
            EXPECT_TRUE(gmm->gmmResourceInfo->getResourceFlags()->Info.LocalOnly);
            EXPECT_TRUE(gmm->gmmResourceInfo->getResourceFlags()->Info.NotLockable);
        }
    }
    memoryManager.freeGraphicsMemory(allocation);
}