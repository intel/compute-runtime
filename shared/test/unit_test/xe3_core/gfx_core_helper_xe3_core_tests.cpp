/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using GfxCoreHelperTestsXe3Core = GfxCoreHelperTest;

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, whenGettingMetricsLibraryGenIdThenXe3IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe3), gfxCoreHelper.getMetricsLibraryGenId());
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, true, 10, AllocationType::commandBuffer, false, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, WhenAskingForDcFlushThenReturnTrue) {
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenGfxCoreHelperWhenAskingForTimestampPacketAlignmentThenReturnCachelineSize) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize;

    EXPECT_EQ(expectedAlignment, gfxCoreHelper.getTimestampPacketAllocatorAlignment());
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenXe3CoreWhenAskedForMinimialSimdThen16IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMinimalSIMDSize());
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEnginesAndTwoRegularCopyEngines) {
    const size_t numEngines = 18;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenCccsAsDefaultEngineWhenGetEnginesCalledThenChangeDefaultEngine) {
    const size_t numEngines = 18;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenOneCcsEnabledWhenGetEnginesCalledThenCreateOnlyOneCcs) {
    const size_t numEngines = 15;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenFtrRcsDisabledWhenGettingGpgpuEnginesThenCCCSIsNotAdded) {
    const size_t numEngines = 14;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenNodeOrdinalSetoToCCCSWhenGettingGpgpuEnginesThenCCCSIsAdded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.NodeOrdinal.set(aub_stream::ENGINE_CCCS);
    const size_t numEngines = 15;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isCcs;
        bool isBcs;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false},
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsuportted) {
    const size_t numEngines = 10;

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
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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
        {aub_stream::ENGINE_BCS3, false, true}, // regular
        {aub_stream::ENGINE_BCS3, false, true}, // internal
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
    const size_t numEngines = 9;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
    const size_t numEngines = 7;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
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

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(3u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(3u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[2].first);
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, whenNonBcsEngineIsVerifiedThenReturnFalse) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenGfxCoreHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue) {
    DebugManagerStateRestore dbgRestore;

    const auto hwInfo = *defaultHwInfo;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(-1);
    EXPECT_EQ(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper), !hwInfo.capabilityTable.isIntegratedDevice);

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_FALSE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(1);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice * sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice * 2 * sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenDefaultMemorySynchronizationCommandsWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x0, NEO::FenceType::release, rootDeviceEnvironment);

    if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment) > 0) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream);
        EXPECT_EQ(1u, hwParser.cmdList.size());
        auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
        ASSERT_NE(nullptr, fenceCmd);
        EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
    }
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenSemaphoreWaitIsCalled) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);
    uint64_t gpuAddress = 0x12345678;

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, gpuAddress, NEO::FenceType::release, rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream);
    EXPECT_EQ(1u, hwParser.cmdList.size());
    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*hwParser.cmdList.begin());
    ASSERT_NE(nullptr, semaphoreCmd);
    EXPECT_EQ(static_cast<uint32_t>(-2), semaphoreCmd->getSemaphoreDataDword());
    EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x0, NEO::FenceType::release, rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream);
    EXPECT_EQ(1u, hwParser.cmdList.size());
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
}

using ProductHelperTestXe3Core = Test<DeviceFixture>;

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCheckTimestampWaitForQueuesSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForQueues(false));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForEvents());
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCallUseGemCreateExtInAllocateMemoryByKMDThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.useGemCreateExtInAllocateMemoryByKMD());
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 1u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 3u);
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenDefaultGfxCoreHelperHwWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    MockGraphicsAllocation allocation;
    allocation.overrideMemoryPool(MemoryPool::localMemory);
    allocation.setAllocationType(AllocationType::bufferHostMemory);
    EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(pDevice->getRootDeviceEnvironment(), allocation));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
    DebugManagerStateRestore restore;
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);

    debugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenMultitileConfigWhenConfiguringHwInfoThenBlitterIsEnabled) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
    }
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isBlitterForImagesSupported());
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenAskingForGlobalFenceSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.isReleaseGlobalFenceInCommandStreamRequired(*defaultHwInfo), !defaultHwInfo->capabilityTable.isIntegratedDevice);
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCallDeferMOCSToPatThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(false));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCallDeferMOCSToPatOnWSLThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(true));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenAskingForCooperativeEngineSupportThenReturnFalse) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isCooperativeEngineSupported(*defaultHwInfo));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenAskingForIsIpSamplingSupportedThenReturnFalse) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isIpSamplingSupported(*defaultHwInfo));
}

XE3_CORETEST_F(ProductHelperTestXe3Core, givenProductHelperWhenCallIsNewCoherencyModelSupportedThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isNewCoherencyModelSupported());
}

using LriHelperTestsXe3Core = ::testing::Test;

XE3_CORETEST_F(LriHelperTestsXe3Core, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    EXPECT_FALSE(expectedLri.getMmioRemapEnable());
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(true);

    LriHelper<FamilyType>::program(&stream, address, data, true, false);
    MI_LOAD_REGISTER_IMM *lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(buffer.get());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenNumGrfAndSimdSizeWhenAdjustingMaxWorkGroupSizeThenCorrectWorkGroupSizeIsReturned) {
    auto defaultMaxWorkGroupSize = 2048u;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    std::array<std::array<uint32_t, 3>, 15> values = {{
        {128u, 16u, 1024u}, // Grf Size, SIMT Size, Max Num of threads
        {128u, 32u, 1024u},
        {160u, 16u, 768u},
        {160u, 32u, 1024u},
        {192u, 16u, 640u},
        {192u, 32u, 1024u},
        {256u, 16u, 512u},
        {256u, 32u, 1024u},
        {512u, 16u, 256u},
        {512u, 32u, 512u},
        {128u, 1u, 64u},
        {160u, 1u, 48u},
        {192u, 1u, 40u},
        {256u, 1u, 32u},
        {512u, 1u, 16u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.adjustMaxWorkGroupSize(grfSize, simtSize, defaultMaxWorkGroupSize, rootDeviceEnvironment));
    }
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenXe3CoreWhenAskedForMinimialGrfSizeThen32IsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(32u, gfxCoreHelper.getMinimalGrfSize());
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenParamsWhenCalculateNumThreadsPerThreadGroupThenMethodReturnProperValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto totalWgSize = 2048u;
    std::array<std::array<uint32_t, 3>, 15> values = {{
        {128u, 16u, 64u}, // Grf Size, SIMT Size, Max Num of threads
        {128u, 32u, 32u},
        {128u, 1u, 64u},
        {160u, 16u, 48u},
        {160u, 32u, 32u},
        {160u, 1u, 48u},
        {192u, 16u, 40u},
        {192u, 32u, 32u},
        {192u, 1u, 40u},
        {256u, 16u, 32u},
        {256u, 32u, 32u},
        {256u, 1u, 32u},
        {512u, 16u, 16u},
        {512u, 32u, 16u},
        {512u, 1u, 16u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simtSize, totalWgSize, grfSize, rootDeviceEnvironment));
    }
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);

    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenGfxCoreHelperWhenUsmCompressionSupportedCalledThenReturnTrue) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_FALSE(gfxCoreHelper.usmCompressionSupported(*defaultHwInfo));

    defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    EXPECT_TRUE(gfxCoreHelper.usmCompressionSupported(*defaultHwInfo));

    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    EXPECT_FALSE(gfxCoreHelper.usmCompressionSupported(*defaultHwInfo));

    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_TRUE(gfxCoreHelper.usmCompressionSupported(*defaultHwInfo));
}

using ProductHelperTestXe3 = ::testing::Test;

XE3_CORETEST_F(ProductHelperTestXe3, when64bAddressingIsEnabledForRTThenResourcesAreNot48b) {
    DebugManagerStateRestore restore;
    MockExecutionEnvironment executionEnvironment{};
    auto productHelper = &executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper->is48bResourceNeededForRayTracing());

    debugManager.flags.Enable64bAddressingForRayTracing.set(true);
    EXPECT_FALSE(productHelper->is48bResourceNeededForRayTracing());

    debugManager.flags.Enable64bAddressingForRayTracing.set(false);
    EXPECT_TRUE(productHelper->is48bResourceNeededForRayTracing());
}

using GfxCoreHelperTestsXe3CoreWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

XE3_CORETEST_F(GfxCoreHelperTestsXe3CoreWithEnginesCheck, whenGetEnginesCalledThenRegularCcsIsNotAvailable) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0));

    auto &engines = device->getGfxCoreHelper().getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    EXPECT_EQ(device->allEngines.size(), engines.size());

    for (size_t idx = 0; idx < engines.size(); idx++) {
        countEngine(engines[idx].first, engines[idx].second);
    }

    EXPECT_EQ(0u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
}

XE3_CORETEST_F(GfxCoreHelperTestsXe3Core, givenXe3WhenSetStallOnlyBarrierThenResourceBarrierProgrammed) {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;
    constexpr static auto bufferSize = sizeof(RESOURCE_BARRIER);

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, PostSyncMode::noWrite, 0u, 0u, args);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList resourceBarrierList = hwParser.getCommandsList<RESOURCE_BARRIER>();
    EXPECT_EQ(1u, resourceBarrierList.size());
    GenCmdList::iterator itor = resourceBarrierList.begin();
    EXPECT_TRUE(hwParser.isStallingBarrier<FamilyType>(itor));
}