/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using GfxCoreHelperTestsXe2HpgCore = GfxCoreHelperTest;

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenGfxCoreHelperWhenAskingForTimestampPacketAlignmentThenReturnCachelineSize) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize;

    EXPECT_EQ(expectedAlignment, gfxCoreHelper.getTimestampPacketAllocatorAlignment());
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenXe2HpgCoreWhenAskedForMinimialSimdThen16IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMinimalSIMDSize());
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEnginesAndTwoRegularCopyEngines) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenCccsAsDefaultEngineWhenGetEnginesCalledThenChangeDefaultEngine) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenOneCcsEnabledWhenGetEnginesCalledThenCreateOnlyOneCcs) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsuportted) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenFtrRcsDisabledWhenGettingGpgpuEnginesThenCCCSIsNotAdded) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenNodeOrdinalSetoToCCCSWhenGettingGpgpuEnginesThenCCCSIsAdded) {
    const size_t numEngines = 15;
    DebugManagerStateRestore restorer;
    debugManager.flags.NodeOrdinal.set(aub_stream::ENGINE_CCCS);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, whenNonBcsEngineIsVerifiedThenReturnFalse) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, whenPipecontrolWaIsProgrammedThenFlushL1Cache) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[64] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    uint64_t gpuAddress = 0x1234;

    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, gpuAddress, this->pDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::noWrite);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDataportFlush());
    EXPECT_TRUE(pipeControl->getUnTypedDataPortCacheFlush());
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenGfxCoreHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    EXPECT_EQ(0u, MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    EXPECT_EQ(0u, MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenDefaultMemorySynchronizationCommandsWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenSemaphoreWaitIsCalled) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
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

using ProductHelperTestXe2HpgCore = Test<DeviceFixture>;

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForQueues(false));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    MockGraphicsAllocation allocation;
    allocation.overrideMemoryPool(MemoryPool::localMemory);
    allocation.setAllocationType(AllocationType::bufferHostMemory);
    EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(pDevice->getRootDeviceEnvironment(), allocation));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
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

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenMultitileConfigWhenConfiguringHwInfoThenBlitterIsEnabled) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
    }
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    const auto &productHelper = getHelper<ProductHelper>();
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isBlitterForImagesSupported());
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallUseGemCreateExtInAllocateMemoryByKMDThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.useGemCreateExtInAllocateMemoryByKMD());
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenAskingForGlobalFenceSupportThenReturnTrue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isReleaseGlobalFenceInCommandStreamRequired(*defaultHwInfo));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenAskingForCooperativeEngineSupportThenReturnTrue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isCooperativeEngineSupported(*defaultHwInfo));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenAskingForIsIpSamplingSupportedThenReturnFalse) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isIpSamplingSupported(*defaultHwInfo));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallIsNewCoherencyModelSupportedThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isNewCoherencyModelSupported());
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallDeferMOCSToPatThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(false));
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenPatIndexWhenCheckIsCoherentAllocationThenReturnProperValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    std::vector<uint64_t> listOfCoherentPatIndexes = {1, 2, 4, 5, 7, 22, 23, 26, 27, 30, 31};
    for (auto patIndex : listOfCoherentPatIndexes) {
        EXPECT_TRUE(productHelper.isCoherentAllocation(patIndex).value());
    }
    std::vector<uint64_t> listOfNonCoherentPatIndexes = {3, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 24, 25, 28, 29};
    for (auto patIndex : listOfNonCoherentPatIndexes) {
        EXPECT_FALSE(productHelper.isCoherentAllocation(patIndex).value());
    }
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallIsTimestampWaitSupportedForEventsThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForEvents());
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallGetCommandBuffersPreallocatedPerCommandQueueThenReturnCorrectValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(2u, productHelper.getCommandBuffersPreallocatedPerCommandQueue());
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 1u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 3u);
}

XE2_HPG_CORETEST_F(ProductHelperTestXe2HpgCore, givenProductHelperWhenCallIsStagingBuffersEnabledThenReturnTrue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isStagingBuffersEnabled());
}

using LriHelperTestsXe2HpgCore = ::testing::Test;

XE2_HPG_CORETEST_F(LriHelperTestsXe2HpgCore, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
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

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenAllocDataWhenSetExtraAllocationDataThenSetLocalMemForProperTypes) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    for (int type = 0; type < static_cast<int>(AllocationType::count); type++) {
        AllocationProperties allocProperties(0, 1, static_cast<AllocationType>(type), {});
        AllocationData allocData{};
        allocData.flags.useSystemMemory = true;
        allocData.flags.requiresCpuAccess = false;

        gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, pDevice->getRootDeviceEnvironment());

        if (defaultHwInfo->featureTable.flags.ftrLocalMemory) {
            if (allocProperties.allocationType == AllocationType::commandBuffer ||
                allocProperties.allocationType == AllocationType::ringBuffer) {
                EXPECT_FALSE(allocData.flags.useSystemMemory);
                EXPECT_TRUE(allocData.flags.requiresCpuAccess);
            } else if (allocProperties.allocationType == AllocationType::semaphoreBuffer) {
                if (getHelper<ProductHelper>().isAcquireGlobalFenceInDirectSubmissionRequired(pDevice->getHardwareInfo())) {
                    EXPECT_FALSE(allocData.flags.useSystemMemory);
                } else {
                    EXPECT_TRUE(allocData.flags.useSystemMemory);
                }
                EXPECT_TRUE(allocData.flags.requiresCpuAccess);
            } else {
                EXPECT_FALSE(allocData.flags.useSystemMemory);
                EXPECT_FALSE(allocData.flags.requiresCpuAccess);
            }
        }
    }
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenXe2HpgWhenCallIsMatrixMultiplyAccumulateSupportedThenReturnTrue) {
    const auto &compilerProductHelper = getHelper<CompilerProductHelper>();
    auto releaseHelper = this->pDevice->getReleaseHelper();
    EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenNumGrfAndSimdSizeWhenAdjustingMaxWorkGroupSizeThenCorrectWorkGroupSizeIsReturned) {
    auto defaultMaxWorkGroupSize = 2048u;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    std::array<std::array<uint32_t, 3>, 6> values = {{
        {GrfConfig::defaultGrfNumber, 16u, 1024u}, // Grf Size, SIMT Size, Max Num of threads
        {GrfConfig::defaultGrfNumber, 32u, 1024u},
        {GrfConfig::largeGrfNumber, 16u, 512u},
        {GrfConfig::largeGrfNumber, 32u, 1024u},
        {GrfConfig::defaultGrfNumber, 1u, 64u},
        {GrfConfig::largeGrfNumber, 1u, 64u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.adjustMaxWorkGroupSize(grfSize, simtSize, defaultMaxWorkGroupSize, rootDeviceEnvironment));
    }
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenParamsWhenCalculateNumThreadsPerThreadGroupThenMethodReturnProperValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto totalWgSize = 2048u;
    std::array<std::array<uint32_t, 3>, 6> values = {{
        {GrfConfig::defaultGrfNumber, 16u, 64u}, // Grf Size, SIMT Size, Max Num of threads
        {GrfConfig::defaultGrfNumber, 32u, 32u},
        {GrfConfig::defaultGrfNumber, 1u, 64u},
        {GrfConfig::largeGrfNumber, 16u, 32u},
        {GrfConfig::largeGrfNumber, 32u, 32u},
        {GrfConfig::largeGrfNumber, 1u, 64u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThdreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThdreadsPerThreadGroup, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simtSize, totalWgSize, grfSize, rootDeviceEnvironment));
    }
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);

    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, whenGettingMetricsLibraryGenIdThenXe2HpgIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe2HPG), gfxCoreHelper.getMetricsLibraryGenId());
}

using GfxCoreHelperTestsXe2HpgCoreWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCoreWithEnginesCheck, whenGetEnginesCalledThenRegularCcsIsNotAvailable) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0));

    auto &engines = device->getGfxCoreHelper().getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    EXPECT_EQ(device->allEngines.size(), engines.size());

    for (size_t idx = 0; idx < engines.size(); idx++) {
        countEngine(engines[idx].first, engines[idx].second);
    }

    EXPECT_EQ(0u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
}

XE2_HPG_CORETEST_F(GfxCoreHelperTestsXe2HpgCore, givenXe2HpgWhenSetStallOnlyBarrierThenResourceBarrierProgrammed) {
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