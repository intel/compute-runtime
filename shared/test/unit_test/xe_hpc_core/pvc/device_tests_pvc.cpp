/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

using DeviceTestsPvc = ::testing::Test;

HWTEST_EXCLUDE_PRODUCT(DeviceTests, givenZexNumberOfCssEnvVariableSetAmbigouslyWhenDeviceIsCreatedThenDontApplyAnyLimitations, IGFX_PVC)

PVCTEST_F(DeviceTestsPvc, WhenDeviceIsCreatedThenOnlyOneCcsEngineIsExposed) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.SetCommandStreamReceiver.set(1);
    auto hwInfo = *defaultHwInfo;

    hwInfo.featureTable.flags.ftrCCSNode = 1;
    hwInfo.gtSystemInfo.CCSInfo.IsValid = 1;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    auto device = deviceFactory.rootDevices[0];

    auto computeEngineGroupIndex = device->getEngineGroupIndexFromEngineGroupType(EngineGroupType::compute);
    auto computeEngineGroup = device->getRegularEngineGroups()[computeEngineGroupIndex];
    EXPECT_EQ(1u, computeEngineGroup.engines.size());
}

PVCTEST_F(DeviceTestsPvc, givenZexNumberOfCssEnvVariableDefinedForXeHpcWhenSingleDeviceIsCreatedThenCreateDevicesWithProperCcsCount) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;

    debugManager.flags.ZEX_NUMBER_OF_CCS.set("0:4");
    debugManager.flags.SetCommandStreamReceiver.set(1);

    auto hwInfo = *defaultHwInfo;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();
    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};

    {
        auto hardwareInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
        hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

        executionEnvironment.adjustCcsCount();
        EXPECT_EQ(std::min(4u, defaultHwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled), hardwareInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    }
}

struct MemoryManagerDirectSubmissionImplicitScalingPvcTest : public ::testing::Test {

    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
        auto allTilesMask = executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->deviceAffinityMask.getGenericSubDevicesMask();

        allocationProperties = std::make_unique<AllocationProperties>(mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::unknown, allTilesMask);
        allocationProperties->flags.multiOsContextCapable = true;

        constexpr auto enableLocalMemory = true;
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, *executionEnvironment);

        memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->reserveOnBanks(1u, MemoryConstants::pageSize2M);
        EXPECT_NE(0u, memoryManager->internalLocalMemoryUsageBankSelector[mockRootDeviceIndex]->getLeastOccupiedBank(allTilesMask));
    }

    DebugManagerStateRestore restorer{};

    constexpr static DeviceBitfield firstTileMask{1u};
    constexpr static auto numSubDevices = 2u;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment{};
    std::unique_ptr<AllocationProperties> allocationProperties{};
    std::unique_ptr<MockMemoryManager> memoryManager{};
};

PVCTEST_F(MemoryManagerDirectSubmissionImplicitScalingPvcTest, givenDirectSubmissionForceLocalMemoryStorageEnabledForAllEnginesWhenAllocatingMemoryForCommandOrRingOrSemaphoreBufferThenFirstBankIsSelected) {
    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(2);

    HardwareInfo hwInfo = *defaultHwInfo;

    for (auto bdA0 : ::testing::Bool()) {
        uint32_t expectedPlacement = 0b10;

        if (bdA0) {
            hwInfo.platform.usRevId &= ~PVC::pvcBaseDieRevMask;
        } else {
            hwInfo.platform.usRevId |= PVC::pvcBaseDieRevMask;
        }

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);

        for (auto &multiTile : ::testing::Bool()) {
            if (multiTile || bdA0) {
                expectedPlacement = static_cast<uint32_t>(firstTileMask.to_ulong());
            }

            for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
                allocationProperties->allocationType = allocationType;
                allocationProperties->flags.multiOsContextCapable = multiTile;
                auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

                EXPECT_NE(nullptr, allocation);

                EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
                EXPECT_EQ(expectedPlacement, allocation->storageInfo.getMemoryBanks());

                memoryManager->freeGraphicsMemory(allocation);
            }
        }
    }
}

PVCTEST_F(MemoryManagerDirectSubmissionImplicitScalingPvcTest, givenDirectSubmissionForceLocalMemoryStorageDefaultModeWhenAllocatingMemoryForCommandOrRingOrSemaphoreBufferThenFirstBankIsSelected) {
    debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.set(-1);

    HardwareInfo hwInfo = *defaultHwInfo;

    for (auto bdA0 : ::testing::Bool()) {
        uint32_t expectedPlacement = 0b10;

        if (bdA0) {
            hwInfo.platform.usRevId &= ~PVC::pvcBaseDieRevMask;
        } else {
            hwInfo.platform.usRevId |= PVC::pvcBaseDieRevMask;
        }

        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);

        for (auto &multiTile : ::testing::Bool()) {
            if (multiTile || bdA0) {
                expectedPlacement = static_cast<uint32_t>(firstTileMask.to_ulong());
            }

            for (auto &allocationType : {AllocationType::commandBuffer, AllocationType::ringBuffer, AllocationType::semaphoreBuffer}) {
                allocationProperties->allocationType = allocationType;
                allocationProperties->flags.multiOsContextCapable = multiTile;
                auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(*allocationProperties, nullptr);

                EXPECT_NE(nullptr, allocation);

                EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
                EXPECT_EQ(expectedPlacement, allocation->storageInfo.getMemoryBanks());

                memoryManager->freeGraphicsMemory(allocation);
            }
        }
    }
}