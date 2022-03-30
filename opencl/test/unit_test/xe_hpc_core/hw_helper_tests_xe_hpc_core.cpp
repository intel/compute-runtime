/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using HwHelperTestsXeHpcCore = Test<ClDeviceFixture>;

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenPvcThenAuxTranslationIsNotRequired) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenHwHelperwhenAskingForDcFlushThenReturnFalse) {
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenLocalMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::COMMAND_BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInProperMemoryPool) {
    const uint32_t numDevices = 4u;
    const uint32_t tileIndex = 2u;
    const DeviceBitfield singleTileMask{static_cast<uint32_t>(1u << tileIndex)};
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};

    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    DebugManager.flags.EnableLocalMemory.set(true);
    initPlatform();

    auto clDevice = platform()->getClDevice(0);
    auto hwInfo = clDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0b111000; // not BD A0

    auto commandStreamReceiver = clDevice->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(singleTileMask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInLocalMemoryPool) {
    const uint32_t numDevices = 4u;
    const DeviceBitfield tile0Mask{0x1};
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};

    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    DebugManager.flags.EnableLocalMemory.set(true);
    DebugManager.flags.OverrideLeastOccupiedBank.set(0u);
    initPlatform();

    auto clDevice = platform()->getClDevice(0);
    auto hwInfo = clDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0b111000; // not BD A0

    auto commandStreamReceiver = clDevice->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(tile0Mask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenSingleTileBdA0CsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInProperMemoryPool) {
    const uint32_t numDevices = 4u;
    const uint32_t tileIndex = 2u;
    const DeviceBitfield tile0Mask = 1;
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup{&ultHwConfig};

    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManager.flags.CreateMultipleSubDevices.set(numDevices);
    DebugManager.flags.EnableLocalMemory.set(true);
    initPlatform();

    auto clDevice = platform()->getClDevice(0);
    auto hwInfo = clDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0; // BD A0

    auto commandStreamReceiver = clDevice->getSubDevice(tileIndex)->getDefaultEngine().commandStreamReceiver;
    auto &heap = commandStreamReceiver->getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, MemoryConstants::pageSize64k);
    auto heapAllocation = heap.getGraphicsAllocation();
    if (commandStreamReceiver->canUse4GbHeaps) {
        EXPECT_EQ(AllocationType::INTERNAL_HEAP, heapAllocation->getAllocationType());
    } else {
        EXPECT_EQ(AllocationType::LINEAR_STREAM, heapAllocation->getAllocationType());
    }
    EXPECT_EQ(tile0Mask, heapAllocation->storageInfo.memoryBanks);

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenPvcWhenAskedForMinimialSimdThen16IsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(16u, helper.getMinimalSIMDSize());
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, whenQueryingMaxNumSamplersThenReturnZero) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, helper.getMaxNumSamplers());
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        REVISION_D,
        CommonConstants::invalidStepping,
    };

    if (hardwareInfo.platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping == REVISION_A0) {
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        } else if (stepping == REVISION_B) {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        } else {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        }

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo));

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndLinkCopyEngines) {
    const size_t numEngines = 17;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
        {aub_stream::ENGINE_BCS4, false, true},
        {aub_stream::ENGINE_BCS5, false, true},
        {aub_stream::ENGINE_BCS6, false, true},
        {aub_stream::ENGINE_BCS7, false, true},
        {aub_stream::ENGINE_BCS8, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEngines) {
    const size_t numEngines = 17;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
        {aub_stream::ENGINE_BCS4, false, true},
        {aub_stream::ENGINE_BCS5, false, true},
        {aub_stream::ENGINE_BCS6, false, true},
        {aub_stream::ENGINE_BCS7, false, true},
        {aub_stream::ENGINE_BCS8, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCccsAsDefaultEngineWhenGetEnginesCalledThenChangeDefaultEngine) {
    const size_t numEngines = 17;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
        {aub_stream::ENGINE_BCS4, false, true},
        {aub_stream::ENGINE_BCS5, false, true},
        {aub_stream::ENGINE_BCS6, false, true},
        {aub_stream::ENGINE_BCS7, false, true},
        {aub_stream::ENGINE_BCS8, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenOneCcsEnabledWhenGetEnginesCalledThenCreateOnlyOneCcs) {
    const size_t numEngines = 14;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
        {aub_stream::ENGINE_BCS4, false, true},
        {aub_stream::ENGINE_BCS5, false, true},
        {aub_stream::ENGINE_BCS6, false, true},
        {aub_stream::ENGINE_BCS7, false, true},
        {aub_stream::ENGINE_BCS8, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsupported) {
    const size_t numEngines = 9;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.featureTable.ftrBcsInfo.set(0, false);
    hwInfo.featureTable.ftrBcsInfo.set(2, false);
    hwInfo.featureTable.ftrBcsInfo.set(7, false);
    hwInfo.featureTable.ftrBcsInfo.set(8, false);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
        {aub_stream::ENGINE_BCS4, false, true},
        {aub_stream::ENGINE_BCS5, false, true},
        {aub_stream::ENGINE_BCS6, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
    const size_t numEngines = 9;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
    const size_t numEngines = 7;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, whenNonBcsEngineIsVerifiedThenReturnFalse) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, whenPipecontrolWaIsProgrammedThenFlushL1Cache) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[64] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    uint64_t gpuAddress = 0x1234;

    MemorySynchronizationCommands<FamilyType>::addPipeControlWA(cmdStream, gpuAddress, *defaultHwInfo);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(buffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenHwHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;

    auto hwInfo = *defaultHwInfo;
    auto &helper = HwHelper::get(renderCoreFamily);

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
    EXPECT_TRUE(helper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_FALSE(helper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_TRUE(helper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_TRUE(helper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
    EXPECT_TRUE(helper.isFenceAllocationRequired(hwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    if (hardwareInfo.platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    if (hardwareInfo.platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    EXPECT_EQ(2 * sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto hardwareInfo = *defaultHwInfo;

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto hardwareInfo = *defaultHwInfo;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenMemorySynchronizationCommandsWhenAddingSynchronizationThenCorrectMethodIsUsed) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    struct {
        unsigned short revisionId;
        int32_t programGlobalFenceAsMiMemFenceCommandInCommandStream;
        bool expectMiSemaphoreWait;
    } testInputs[] = {
        {0x0, -1, true},
        {0x3, -1, false},
        {0x0, 0, true},
        {0x3, 0, true},
        {0x0, 1, false},
        {0x3, 1, false},
    };

    DebugManagerStateRestore debugRestorer;
    auto hardwareInfo = *defaultHwInfo;

    if (hardwareInfo.platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    uint64_t gpuAddress = 0x12345678;

    for (auto &testInput : testInputs) {
        hardwareInfo.platform.usRevId = testInput.revisionId;
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(
            testInput.programGlobalFenceAsMiMemFenceCommandInCommandStream);

        LinearStream commandStream(buffer, 128);
        auto synchronizationSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);

        MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, gpuAddress, false, hardwareInfo);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream);
        EXPECT_EQ(1u, hwParser.cmdList.size());

        if (testInput.expectMiSemaphoreWait) {
            EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), synchronizationSize);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, semaphoreCmd);
            EXPECT_EQ(static_cast<uint32_t>(-2), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());
        } else {
            EXPECT_EQ(sizeof(MI_MEM_FENCE), synchronizationSize);

            auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, fenceCmd);
            EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE, fenceCmd->getAFenceType());
        }
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenHwHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &helper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto &configs = helper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenDefaultHwHelperHwWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    MockGraphicsAllocation allocation;
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    allocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(*defaultHwInfo, allocation));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenNonTile0AccessWhenGettingIsBlitCopyRequiredForLocalMemoryThenTrueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);

    hwInfo.platform.usRevId = FamilyType::pvcBaseDieA0Masked;
    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.memoryBanks = 0b11;
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    graphicsAllocation.storageInfo.memoryBanks = 0b10;
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    {
        VariableBackup<unsigned short> revisionId{&hwInfo.platform.usRevId};
        revisionId = FamilyType::pvcBaseDieA0Masked ^ FamilyType::pvcBaseDieRevMask;
        EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
    {
        VariableBackup<bool> cloningOfPageTables{&graphicsAllocation.storageInfo.cloningOfPageTables};
        cloningOfPageTables = true;
        EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
    {
        VariableBackup<DeviceBitfield> memoryBanks{&graphicsAllocation.storageInfo.memoryBanks};
        memoryBanks = 0b1;
        EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCCCSEngineAndRevisionBWhenCallingIsCooperativeDispatchSupportedThenFalseIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto context = new NEO::MockContext(pClDevice);
    auto commandQueue = reinterpret_cast<MockCommandQueue *>(new MockCommandQueueHw<FamilyType>(context, pClDevice, 0));

    auto engineGroupType = helper.getEngineGroupType(commandQueue->getGpgpuEngine().getEngineType(),
                                                     commandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    auto retVal = helper.isCooperativeDispatchSupported(engineGroupType, hardwareInfo);
    EXPECT_TRUE(retVal);

    auto &hwConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    retVal = helper.isCooperativeDispatchSupported(engineGroupType, hardwareInfo);
    EXPECT_FALSE(retVal);
    commandQueue->release();
    context->decRefInternal();
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCCSEngineWhenCallingIsCooperativeDispatchSupportedThenTrueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto hwInfo = *defaultHwInfo;
    uint64_t hwInfoConfig = defaultHardwareInfoConfigTable[productFamily];
    hardwareInfoSetup[productFamily](&hwInfo, true, hwInfoConfig);
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo);
    ASSERT_NE(nullptr, device);
    auto clDevice = new MockClDevice{device};
    ASSERT_NE(nullptr, clDevice);
    auto context = new NEO::MockContext(clDevice);
    auto commandQueue = reinterpret_cast<MockCommandQueue *>(new MockCommandQueueHw<FamilyType>(context, clDevice, 0));

    auto engineGroupType = helper.getEngineGroupType(commandQueue->getGpgpuEngine().getEngineType(),
                                                     commandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);
    auto retVal = helper.isCooperativeDispatchSupported(engineGroupType, hwInfo);
    ASSERT_TRUE(retVal);
    commandQueue->release();
    context->decRefInternal();
    delete clDevice;
}

using HwInfoConfigTestXeHpcCore = ::testing::Test;

XE_HPC_CORETEST_F(HwInfoConfigTestXeHpcCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
    DebugManagerStateRestore restore;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
}

XE_HPC_CORETEST_F(HwInfoConfigTestXeHpcCore, givenMultitileConfigWhenConfiguringHwInfoThenEnableBlitter) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(true, hwInfo.capabilityTable.blitterOperationsSupported);
    }
}

using LriHelperTestsXeHpcCore = ::testing::Test;

XE_HPC_CORETEST_F(LriHelperTestsXeHpcCore, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    EXPECT_FALSE(expectedLri.getMmioRemapEnable());
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(true);

    LriHelper<FamilyType>::program(&stream, address, data, true);
    MI_LOAD_REGISTER_IMM *lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(buffer.get());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCccsDisabledWhenGetGpgpuEnginesCalledThenDontSetCccs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(8u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(8u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[4].first); // low priority
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[5].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCccsDisabledButDebugVariableSetWhenGetGpgpuEnginesCalledThenSetCccs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(9u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(9u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[5].first); // low priority
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[6].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);  // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[8].first);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, WhenCheckingSipWAThenFalseIsReturned) {
    EXPECT_FALSE(HwHelper::get(renderCoreFamily).isSipWANeeded(*defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, WhenCheckingPreferenceForBlitterForLocalToLocalTransfersThenReturnFalse) {
    EXPECT_FALSE(ClHwHelper::get(renderCoreFamily).preferBlitterForLocalToLocalTransfers());
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenBdA0WhenBcsSubDeviceSupportIsCheckedThenReturnFalse) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    constexpr uint8_t bdRev[4] = {0, 0b111001, 0b101001, 0b000101};

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.DoNotReportTile1BscWaActive.set(debugFlag);

        for (uint64_t subDevice = 0; subDevice < 4; subDevice++) {
            for (auto rev : bdRev) {
                hwInfo.platform.usRevId = rev;

                for (uint32_t engineType = 0; engineType < static_cast<uint32_t>(aub_stream::EngineType::NUM_ENGINES); engineType++) {
                    auto engineTypeT = static_cast<aub_stream::EngineType>(engineType);

                    bool result = hwHelper.isSubDeviceEngineSupported(hwInfo, DeviceBitfield(1llu << subDevice), engineTypeT);

                    bool affectedEngine = ((subDevice == 1) &&
                                           (aub_stream::ENGINE_BCS == engineTypeT ||
                                            aub_stream::ENGINE_BCS1 == engineTypeT ||
                                            aub_stream::ENGINE_BCS3 == engineTypeT));
                    bool isBdA0 = ((rev & FamilyType::pvcBaseDieRevMask) == FamilyType::pvcBaseDieA0Masked);

                    bool applyWa = affectedEngine;
                    applyWa &= isBdA0 || (debugFlag == 1);
                    applyWa &= (debugFlag != 0);

                    EXPECT_EQ(!applyWa, result);
                }
            }
        }
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenBdA0WhenAllocatingOnNonTileZeroThenForceTile0) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    constexpr uint8_t bdRev[4] = {0, 0b111001, 0b101001, 0b000101};
    constexpr DeviceBitfield originalTileMasks[4] = {0b1, 0b11, 0b10, 0b1011};

    constexpr DeviceBitfield tile0Mask = 1;
    constexpr DeviceBitfield allTilesMask = 0b1111;

    const AllocationProperties allocProperties(0, 1, AllocationType::UNKNOWN, allTilesMask);

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.set(debugFlag);

        for (auto rev : bdRev) {
            hwInfo.platform.usRevId = rev;

            bool isBdA0 = ((hwInfo.platform.usRevId & FamilyType::pvcBaseDieRevMask) == FamilyType::pvcBaseDieA0Masked);

            for (auto originalMask : originalTileMasks) {
                AllocationData allocData;

                allocData.flags.requiresCpuAccess = true;
                allocData.storageInfo.memoryBanks = originalMask;

                hwHelper.setExtraAllocationData(allocData, allocProperties, hwInfo);

                bool applyWa = (isBdA0 || (debugFlag == 1));
                applyWa &= (debugFlag != 0);

                if (applyWa) {
                    EXPECT_EQ(tile0Mask, allocData.storageInfo.memoryBanks);
                } else {
                    EXPECT_EQ(originalMask, allocData.storageInfo.memoryBanks);
                }
            }
        }
    }
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, givenCommandBufferAllocationWhenSetExtraAllocationDataThenUseSystemLocalMemoryOnlyForImplicitScalingCommandBuffers) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    constexpr DeviceBitfield singleTileBitfield = 0b0100;
    constexpr DeviceBitfield allTilesBitfield = 0b1111;

    const AllocationProperties singleTileAllocProperties(0, 1, AllocationType::COMMAND_BUFFER, singleTileBitfield);
    const AllocationProperties allTilesAllocProperties(0, 1, AllocationType::COMMAND_BUFFER, allTilesBitfield);

    AllocationData allocData;
    allocData.flags.useSystemMemory = false;

    hwHelper.setExtraAllocationData(allocData, singleTileAllocProperties, hwInfo);
    EXPECT_FALSE(allocData.flags.useSystemMemory);

    hwHelper.setExtraAllocationData(allocData, allTilesAllocProperties, hwInfo);
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 8, 1), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

XE_HPC_CORETEST_F(HwHelperTestsXeHpcCore, GivenRevisionIdWhenGetComputeUnitsUsedForScratchThenReturnValidValue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.EUCount *= 2;

    if (hwInfo.platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    struct {
        unsigned short revId;
        uint32_t expectedRatio;
    } testInputs[] = {
        {0x0, 8},
        {0x1, 8},
        {0x3, 16},
        {0x5, 16},
        {0x6, 16},
        {0x7, 16},
    };

    for (auto &testInput : testInputs) {
        hwInfo.platform.usRevId = testInput.revId;
        EXPECT_EQ(expectedValue * testInput.expectedRatio, helper.getComputeUnitsUsedForScratch(&hwInfo));
    }
}
