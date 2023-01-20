/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "hw_cmds_xe_hpc_core_base.h"

using ClGfxCoreHelperTestsXeHpcCore = Test<ClDeviceFixture>;
;

XE_HPC_CORETEST_F(ClGfxCoreHelperTestsXeHpcCore, givenXeHpcThenAuxTranslationIsNotRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo));
}

XE_HPC_CORETEST_F(ClGfxCoreHelperTestsXeHpcCore, WhenCheckingPreferenceForBlitterForLocalToLocalTransfersThenReturnFalse) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_FALSE(clGfxCoreHelper.preferBlitterForLocalToLocalTransfers());
}

using GfxCoreHelperTestsXeHpcCore = Test<ClDeviceFixture>;

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenGfxCoreHelperwhenAskingForDcFlushThenReturnFalse) {
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenLocalMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::COMMAND_BUFFER, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenSingleTileBdA0CsrWhenAllocatingCsrSpecificAllocationsThenStoreThemInProperMemoryPool) {
    const uint32_t numDevices = 4u;
    const uint32_t tileIndex = 2u;
    [[maybe_unused]] const DeviceBitfield tile0Mask = 1;
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
    auto &productHelper = clDevice->getProductHelper();
    if (productHelper.isTilePlacementResourceWaRequired(*hwInfo)) {
        EXPECT_EQ(tile0Mask, heapAllocation->storageInfo.memoryBanks);
    }

    commandStreamReceiver->ensureCommandBufferAllocation(heap, heap.getAvailableSpace() + 1, 0u);
    auto commandBufferAllocation = heap.getGraphicsAllocation();
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, commandBufferAllocation->getAllocationType());
    EXPECT_NE(heapAllocation, commandBufferAllocation);
    EXPECT_EQ(commandBufferAllocation->getMemoryPool(), MemoryPool::LocalMemory);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenXeHpcWhenAskedForMinimialSimdThen16IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMinimalSIMDSize());
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, whenQueryingMaxNumSamplersThenReturnZero) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.getMaxNumSamplers());
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_EQ(0u, gfxCoreHelper.getBarriersCountFromHasBarriers(0u));
    EXPECT_EQ(1u, gfxCoreHelper.getBarriersCountFromHasBarriers(1u));
    EXPECT_EQ(2u, gfxCoreHelper.getBarriersCountFromHasBarriers(2u));
    EXPECT_EQ(4u, gfxCoreHelper.getBarriersCountFromHasBarriers(3u));
    EXPECT_EQ(8u, gfxCoreHelper.getBarriersCountFromHasBarriers(4u));
    EXPECT_EQ(16u, gfxCoreHelper.getBarriersCountFromHasBarriers(5u));
    EXPECT_EQ(24u, gfxCoreHelper.getBarriersCountFromHasBarriers(6u));
    EXPECT_EQ(32u, gfxCoreHelper.getBarriersCountFromHasBarriers(7u));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCccsDisabledButDebugVariableSetWhenIsCooperativeEngineSupportedEnabledAndGetGpgpuEnginesCalledThenSetCccsProperly) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(13u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(13u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[7].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[8].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[9].first);  // low priority
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[10].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[11].first);  // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[12].first);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCccsDisabledWhenIsCooperativeEngineSupportedEnabledAndGetGpgpuEnginesCalledThenDontSetCccs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(12u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(12u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[7].first);
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[8].first); // low priority
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[9].first); // internal
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[10].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[11].first);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenBcsDisabledWhenIsCooperativeEngineSupportedEnabledAndGetEnginesCalledThenDontCreateAnyBcs) {
    const size_t numEngines = 11;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenOneBcsEnabledWhenIsCooperativeEngineSupportedEnabledAndGetEnginesCalledThenCreateOnlyOneBcs) {
    const size_t numEngines = 13;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenNotAllCopyEnginesWhenIsCooperativeEngineSupportedEnabledAndSettingEngineTableThenDontAddUnsupported) {
    const size_t numEngines = 10;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.featureTable.ftrBcsInfo.set(0, false);
    hwInfo.featureTable.ftrBcsInfo.set(3, false);
    hwInfo.featureTable.ftrBcsInfo.set(7, false);
    hwInfo.featureTable.ftrBcsInfo.set(8, false);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenOneCcsEnabledWhenIsCooperativeEngineSupportedEnabledAndGetEnginesCalledThenCreateOnlyOneCcs) {
    const size_t numEngines = 16;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCccsAsDefaultEngineWhenIsCooperativeEngineSupportedEnabledAndGetEnginesCalledThenChangeDefaultEngine) {
    const size_t numEngines = 22;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, whenIsCooperativeEngineSupportedEnabledAndGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEngines) {
    const size_t numEngines = 22;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, whenIsCooperativeEngineSupportedEnabledAndGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndLinkCopyEngines) {
    const size_t numEngines = 22;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getHardwareInfo());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS1, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS2, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCS3, true, false},
        {aub_stream::ENGINE_CCCS, false, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS, false, true},
        {aub_stream::ENGINE_BCS1, false, true},
        {aub_stream::ENGINE_BCS2, false, true},
        {aub_stream::ENGINE_BCS3, false, true},
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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, whenNonBcsEngineIsVerifiedThenReturnFalse) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, whenPipecontrolWaIsProgrammedThenFlushL1Cache) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[64] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    uint64_t gpuAddress = 0x1234;

    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, gpuAddress, *defaultHwInfo);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(buffer);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenGfxCoreHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;

    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_FALSE(gfxCoreHelper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo));

    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto hardwareInfo = *defaultHwInfo;

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto hardwareInfo = *defaultHwInfo;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenGfxCoreHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto &configs = gfxCoreHelper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

using ProductHelperTestXeHpcCore = Test<DeviceFixture>;

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenDefaultProductHelperHwWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    MockGraphicsAllocation allocation;
    allocation.overrideMemoryPool(MemoryPool::LocalMemory);
    allocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(*defaultHwInfo, allocation));
}

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
    DebugManagerStateRestore restore;
    auto &productHelper = getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
}

XE_HPC_CORETEST_F(ProductHelperTestXeHpcCore, givenMultitileConfigWhenConfiguringHwInfoThenEnableBlitter) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        productHelper.configureHardwareCustom(&hwInfo, nullptr);

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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, WhenCheckingSipWAThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isSipWANeeded(*defaultHwInfo));
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenBdA0WhenBcsSubDeviceSupportIsCheckedThenReturnFalse) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    constexpr uint8_t bdRev[4] = {0, 0b111001, 0b101001, 0b000101};

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.DoNotReportTile1BscWaActive.set(debugFlag);

        for (uint64_t subDevice = 0; subDevice < 4; subDevice++) {
            for (auto rev : bdRev) {
                hwInfo.platform.usRevId = rev;

                for (uint32_t engineType = 0; engineType < static_cast<uint32_t>(aub_stream::EngineType::NUM_ENGINES); engineType++) {
                    auto engineTypeT = static_cast<aub_stream::EngineType>(engineType);

                    bool result = gfxCoreHelper.isSubDeviceEngineSupported(hwInfo, DeviceBitfield(1llu << subDevice), engineTypeT);

                    bool affectedEngine = ((subDevice == 1) &&
                                           (aub_stream::ENGINE_BCS == engineTypeT ||
                                            aub_stream::ENGINE_BCS1 == engineTypeT ||
                                            aub_stream::ENGINE_BCS3 == engineTypeT));
                    bool isBdA0 = productHelper.isBcsReportWaRequired(hwInfo);

                    bool applyWa = affectedEngine;
                    applyWa &= isBdA0 || (debugFlag == 1);
                    applyWa &= (debugFlag != 0);

                    EXPECT_EQ(!applyWa, result);
                }
            }
        }
    }
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenBdA0WhenAllocatingOnNonTileZeroThenForceTile0) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    constexpr uint8_t bdRev[4] = {0, 0b111001, 0b101001, 0b000101};
    constexpr DeviceBitfield originalTileMasks[4] = {0b1, 0b11, 0b10, 0b1011};

    constexpr DeviceBitfield tile0Mask = 1;
    constexpr DeviceBitfield allTilesMask = 0b1111;

    const AllocationProperties allocProperties(0, 1, AllocationType::UNKNOWN, allTilesMask);

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.ForceTile0PlacementForTile1ResourcesWaActive.set(debugFlag);

        for (auto rev : bdRev) {
            hwInfo.platform.usRevId = rev;

            bool isBdA0 = productHelper.isTilePlacementResourceWaRequired(hwInfo);

            for (auto originalMask : originalTileMasks) {
                AllocationData allocData;

                allocData.flags.requiresCpuAccess = true;
                allocData.storageInfo.memoryBanks = originalMask;

                gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, hwInfo);

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

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, givenCommandBufferAllocationWhenSetExtraAllocationDataThenUseSystemLocalMemoryOnlyForImplicitScalingCommandBuffers) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr DeviceBitfield singleTileBitfield = 0b0100;
    constexpr DeviceBitfield allTilesBitfield = 0b1111;

    const AllocationProperties singleTileAllocProperties(0, 1, AllocationType::COMMAND_BUFFER, singleTileBitfield);
    const AllocationProperties allTilesAllocProperties(0, 1, AllocationType::COMMAND_BUFFER, allTilesBitfield);

    AllocationData allocData;
    allocData.flags.useSystemMemory = false;

    gfxCoreHelper.setExtraAllocationData(allocData, singleTileAllocProperties, hwInfo);
    EXPECT_FALSE(allocData.flags.useSystemMemory);

    gfxCoreHelper.setExtraAllocationData(allocData, allTilesAllocProperties, hwInfo);
    EXPECT_FALSE(allocData.flags.useSystemMemory);
}

XE_HPC_CORETEST_F(GfxCoreHelperTestsXeHpcCore, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    EXPECT_EQ(ClGfxCoreHelperMock::makeDeviceIpVersion(12, 8, 1), clGfxCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}
