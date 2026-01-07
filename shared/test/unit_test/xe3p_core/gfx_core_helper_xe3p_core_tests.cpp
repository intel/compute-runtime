/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using GfxCoreHelperTestsXe3pCore = GfxCoreHelperTest;
XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenGfxCoreHelperWhenAskingForTimestampPacketAlignmentThenReturnCachelineSize) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize;

    EXPECT_EQ(expectedAlignment, gfxCoreHelper.getTimestampPacketAllocatorAlignment());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenXe3pCoreWhenAskedForMinimialSimdThen16IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMinimalSIMDSize());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
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

using GfxCoreHelperTestsXe3pCoreWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEnginesAndTwoRegularCopyEngines) {
    DebugManagerStateRestore restore;

    const size_t numEnginesWithCccs = 18;
    const size_t numEnginesWithoutCccs = 17;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    for (auto debugFlag : {false, true}) {
        debugManager.flags.NodeOrdinal.set(debugFlag ? static_cast<int32_t>(aub_stream::ENGINE_CCCS) : -1);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

        auto releaseHelper = device->getReleaseHelper();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cccsEnabled = !releaseHelper->isRcsExposureDisabled() || debugFlag;

        EXPECT_EQ(cccsEnabled ? numEnginesWithCccs : numEnginesWithoutCccs, device->allEngines.size());

        device->getRootDeviceEnvironmentRef().setRcsExposure();
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
        EXPECT_EQ(cccsEnabled ? numEnginesWithCccs : numEnginesWithoutCccs, engines.size());

        struct EnginePropertiesMap {
            aub_stream::EngineType engineType;
            bool isCcs;
            bool isBcs;
        };

        if (!cccsEnabled) {
            const std::array<EnginePropertiesMap, numEnginesWithoutCccs> enginePropertiesMap = {{
                {aub_stream::ENGINE_CCS, true, false},
                {aub_stream::ENGINE_CCS1, true, false},
                {aub_stream::ENGINE_CCS2, true, false},
                {aub_stream::ENGINE_CCS3, true, false},
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

            for (size_t i = 0; i < numEnginesWithoutCccs; i++) {
                EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
                EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
                EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
            }
        } else {
            const std::array<EnginePropertiesMap, numEnginesWithCccs> enginePropertiesMap = {{
                {aub_stream::ENGINE_CCS, true, false},
                {aub_stream::ENGINE_CCS1, true, false},
                {aub_stream::ENGINE_CCS2, true, false},
                {aub_stream::ENGINE_CCS3, true, false},
                {aub_stream::ENGINE_CCCS, false, false},
                {debugFlag ? aub_stream::ENGINE_CCCS : aub_stream::ENGINE_CCS, debugFlag ? false : true, false},
                {debugFlag ? aub_stream::ENGINE_CCCS : aub_stream::ENGINE_CCS, debugFlag ? false : true, false},
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

            for (size_t i = 0; i < numEnginesWithCccs; i++) {
                EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
                EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
                EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
            }
        }
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCoreWithEnginesCheck, givenOneCcsEnabledWhenGetEnginesCalledThenCreateOnlyOneCcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();

    bool isBCS0Enabled = (aub_stream::ENGINE_BCS == productHelper.getDefaultCopyEngine());
    bool isBCSLowPriorityEnabled = gfxCoreHelper.areSecondaryContextsSupported();

    size_t numEngines = isBCS0Enabled ? 15 : 14;
    if (isBCSLowPriorityEnabled) {
        numEngines++;
    }

    bool renderCommandStreamerEnabled = device->getHardwareInfo().featureTable.flags.ftrRcsNode;
    if (!renderCommandStreamerEnabled) {
        numEngines--;
    }

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    for (const auto &engine : engines) {
        countEngine(engine.first, engine.second);
    }
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::internal));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::lowPriority));

    if (renderCommandStreamerEnabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
    }

    if (isBCS0Enabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS, EngineUsage::regular));
    }

    for (uint32_t idx = 1; idx < hwInfo.featureTable.ftrBcsInfo.size(); idx++) {
        if (idx == 8 && gfxCoreHelper.areSecondaryContextsSupported()) {
            EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::highPriority));
        } else {
            EXPECT_EQ(1u, getEngineCount(EngineHelpers::getBcsEngineAtIdx(idx), EngineUsage::regular));
        }
    }
    EXPECT_EQ(1u, getEngineCount(productHelper.getDefaultCopyEngine(), EngineUsage::internal));
    if (gfxCoreHelper.areSecondaryContextsSupported()) {
        EXPECT_EQ(1u, getEngineCount(productHelper.getDefaultCopyEngine(), EngineUsage::lowPriority));
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS3, EngineUsage::internal));
    EXPECT_TRUE(allEnginesChecked());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCoreWithEnginesCheck, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsupported) {
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
    auto &productHelper = device->getProductHelper();

    bool isBCS0Enabled = (aub_stream::ENGINE_BCS == productHelper.getDefaultCopyEngine());
    bool isBCSLowPriorityEnabled = gfxCoreHelper.areSecondaryContextsSupported();

    size_t numEngines = isBCS0Enabled ? 10 : 11;
    if (isBCSLowPriorityEnabled) {
        numEngines++;
    }

    bool renderCommandStreamerEnabled = device->getHardwareInfo().featureTable.flags.ftrRcsNode;
    if (!renderCommandStreamerEnabled) {
        numEngines--;
    }

    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    for (const auto &engine : engines) {
        countEngine(engine.first, engine.second);
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::internal));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::lowPriority));
    if (renderCommandStreamerEnabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
    }

    if (false == isBCS0Enabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS1, EngineUsage::internal));
        if (gfxCoreHelper.areSecondaryContextsSupported()) {
            EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS1, EngineUsage::lowPriority));
        }
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS1, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS3, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS3, EngineUsage::internal));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS4, EngineUsage::regular));
    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS5, EngineUsage::regular));

    if (gfxCoreHelper.areSecondaryContextsSupported()) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS6, EngineUsage::highPriority));
    } else {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS6, EngineUsage::regular));
    }

    EXPECT_TRUE(allEnginesChecked());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCoreWithEnginesCheck, givenGroupContextWhenCreatingDeviceThenCreateBcsLpContexts) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(2);

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
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    uint32_t bcsLpContextsCount = 0;
    for (auto &engine : device->getAllEngines()) {
        if (EngineHelpers::isBcs(engine.getEngineType()) && engine.getEngineUsage() == EngineUsage::lowPriority) {
            EXPECT_EQ(1u, EngineHelpers::getBcsIndex(engine.getEngineType()));
            bcsLpContextsCount++;
        }
    }

    EXPECT_EQ(1u, bcsLpContextsCount);

    bcsLpContextsCount = 0;
    for (auto &engine : engines) {
        if (EngineHelpers::isBcs(engine.first) && engine.second == EngineUsage::lowPriority) {
            EXPECT_EQ(1u, EngineHelpers::getBcsIndex(engine.first));
            bcsLpContextsCount++;
        }
    }

    EXPECT_EQ(1u, bcsLpContextsCount);
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
    const size_t numEngines = 8;

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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
    const size_t numEngines = 6;

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
        {aub_stream::ENGINE_CCS, true, false},
        {aub_stream::ENGINE_CCS, true, false},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        EXPECT_EQ(enginePropertiesMap[i].engineType, engines[i].first);
        EXPECT_EQ(enginePropertiesMap[i].isCcs, EngineHelpers::isCcs(enginePropertiesMap[i].engineType));
        EXPECT_EQ(enginePropertiesMap[i].isBcs, EngineHelpers::isBcs(enginePropertiesMap[i].engineType));
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnCcsAndCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(6u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(6u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[5].first);
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenCcsDisabledWhenGetGpgpuEnginesThenReturnCccsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(2u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(2u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[1].first);
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugFlagDisablingContextGroupWhenQueryingEnginesThenLowPriorityAndInternalEngineIsReturned) {
    constexpr size_t numEngines = 9;

    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(0);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(3);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(numEngines, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(numEngines, engines.size());

    struct EnginePropertiesMap {
        aub_stream::EngineType engineType;
        bool isRegular;
        bool isLowPriority;
        bool isInternal;
        bool isHighPriority;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{{aub_stream::ENGINE_CCS, true, false, false, false},
                                                                              {aub_stream::ENGINE_CCS1, true, false, false, false},
                                                                              {aub_stream::ENGINE_CCS, false, true, false, false},
                                                                              {aub_stream::ENGINE_CCS, false, false, true, false},

                                                                              {aub_stream::ENGINE_BCS, true, false, false, false},
                                                                              {aub_stream::ENGINE_BCS, false, false, true, false},
                                                                              {aub_stream::ENGINE_BCS1, true, false, false, false},
                                                                              {aub_stream::ENGINE_BCS2, true, false, false, false},
                                                                              {aub_stream::ENGINE_BCS2, false, false, true, false}}};

    for (size_t i = 0; i < numEngines; i++) {
        const auto &engine = engines[i];
        EXPECT_EQ(enginePropertiesMap[i].engineType, engine.first);
        EXPECT_EQ(enginePropertiesMap[i].isRegular, engine.second == EngineUsage::regular) << i;
        EXPECT_EQ(enginePropertiesMap[i].isLowPriority, engine.second == EngineUsage::lowPriority);
        EXPECT_EQ(enginePropertiesMap[i].isInternal, engine.second == EngineUsage::internal);
        EXPECT_EQ(enginePropertiesMap[i].isHighPriority, engine.second == EngineUsage::highPriority) << i;
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenContextGroupWhenQueryingEnginesThenLowPriorityHighPriorityAndInternalEngineIsReturned) {
    constexpr size_t numEngines = 9;

    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(4);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(3);
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
        bool isRegular;
        bool isLowPriority;
        bool isInternal;
        bool isHighPriority;
    };

    const std::array<EnginePropertiesMap, numEngines> enginePropertiesMap = {{
        {aub_stream::ENGINE_CCS, true, false, false, false},
        {aub_stream::ENGINE_CCS, false, true, false, false},
        {aub_stream::ENGINE_CCS, false, false, true, false},

        {aub_stream::ENGINE_BCS, true, false, false, false},
        {aub_stream::ENGINE_BCS, false, false, true, false},
        {aub_stream::ENGINE_BCS, false, true, false, false},
        {aub_stream::ENGINE_BCS1, true, false, false, false},
        {aub_stream::ENGINE_BCS1, false, false, true, false},
        {aub_stream::ENGINE_BCS2, false, false, false, true},
    }};

    for (size_t i = 0; i < numEngines; i++) {
        const auto &engine = engines[i];
        EXPECT_EQ(enginePropertiesMap[i].engineType, engine.first) << i;
        EXPECT_EQ(enginePropertiesMap[i].isRegular, engine.second == EngineUsage::regular) << i;
        EXPECT_EQ(enginePropertiesMap[i].isLowPriority, engine.second == EngineUsage::lowPriority);
        EXPECT_EQ(enginePropertiesMap[i].isInternal, engine.second == EngineUsage::internal);
        EXPECT_EQ(enginePropertiesMap[i].isHighPriority, engine.second == EngineUsage::highPriority);
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenContextGroupWhenQueryingInternalCopyEngineThenCorrectEngineIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(4);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(4);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    {
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        auto internalBcs = EngineHelpers::mapBcsIndexToEngineType(gfxCoreHelper.getInternalCopyEngineIndex(hwInfo), true);

        EXPECT_EQ(aub_stream::ENGINE_BCS2, internalBcs);
        EXPECT_NE(nullptr, device->tryGetEngine(internalBcs, EngineUsage::internal));
    }

    hwInfo.featureTable.ftrBcsInfo = 2;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    {
        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        auto internalBcs = EngineHelpers::mapBcsIndexToEngineType(gfxCoreHelper.getInternalCopyEngineIndex(hwInfo), true);

        EXPECT_EQ(aub_stream::ENGINE_BCS1, internalBcs);
        EXPECT_NE(nullptr, device->tryGetEngine(internalBcs, EngineUsage::internal));
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenNonBcsEngineIsVerifiedThenReturnFalse) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenGfxCoreHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue) {
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice * sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice * 2 * sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDefaultMemorySynchronizationCommandsWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    if (hardwareInfo.capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenSemaphoreWaitIsCalled) {
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased) {
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

using ProductHelperTestXe3pCore = Test<DeviceFixture>;

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenXe3pProductWhenAdjustPlatformForProductFamilyCalledThenOverrideWithCorrectFamily) {
    auto &productHelper = getHelper<ProductHelper>();

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;
    productHelper.adjustPlatformForProductFamily(&hwInfo);

    EXPECT_EQ(IGFX_XE3P_CORE, hwInfo.platform.eRenderCoreFamily);
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCheckTimestampWaitForQueuesSupportThenResultsIsCorrect) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.isDcFlushAllowed(), productHelper.isTimestampWaitSupportedForQueues(true));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForEvents());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 0u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 3u);
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenDefaultGfxCoreHelperHwWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    MockGraphicsAllocation allocation;
    allocation.overrideMemoryPool(MemoryPool::localMemory);
    allocation.setAllocationType(AllocationType::bufferHostMemory);
    EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(pDevice->getRootDeviceEnvironment(), allocation));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCallUseGemCreateExtInAllocateMemoryByKMDThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.useGemCreateExtInAllocateMemoryByKMD());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
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

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenMultitileConfigWhenConfiguringHwInfoThenBlitterIsEnabled) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
    }
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    auto hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isBlitterForImagesSupported());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenAskingForGlobalFenceSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.isReleaseGlobalFenceInCommandStreamRequired(*defaultHwInfo), !defaultHwInfo->capabilityTable.isIntegratedDevice);
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenAskingForCooperativeEngineSupportThenReturnFalse) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isCooperativeEngineSupported(*defaultHwInfo));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCallDeferMOCSToPatThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(false));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCallDeferMOCSToPatOnWSLThenTrueIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.deferMOCSToPatIndex(true));
}

using LriHelperTestsXe3pCore = ::testing::Test;

XE3P_CORETEST_F(LriHelperTestsXe3pCore, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenNumGrfAndSimdSizeWhenAdjustingMaxWorkGroupSizeAndDisabled64BitAddressingThenCorrectWorkGroupSizeIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.Enable64BitAddressing.set(0);
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
        {512u, 16u, 512},
        {512u, 32u, 1024u},
        {128u, 1u, 64u},
        {160u, 1u, 48u},
        {192u, 1u, 40u},
        {256u, 1u, 32u},
        {512u, 1u, 32u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.adjustMaxWorkGroupSize(grfSize, simtSize, defaultMaxWorkGroupSize, rootDeviceEnvironment));
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenXe3pCoreWhenAskedForMinimialGrfSizeThen32IsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(32u, gfxCoreHelper.getMinimalGrfSize());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenParamsAndE64DisabledWhenCalculateNumThreadsPerThreadGroupThenMethodReturnProperValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.Enable64BitAddressing.set(0);

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
        {512u, 16u, 32u},
        {512u, 32u, 32u},
        {512u, 1u, 32u},
    }};

    for (auto &[grfSize, simtSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simtSize, totalWgSize, grfSize, rootDeviceEnvironment));
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenAskingForImplicitScalingImmWriteOffsetThenAlwaysReturnTsSize) {
    EXPECT_EQ(static_cast<uint32_t>(GfxCoreHelperHw<FamilyType>::getSingleTimestampPacketSizeHw()), ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenAskingIf48bResourceNeededForCmdBufferThenReturnFalse) {
    EXPECT_FALSE(getHelper<GfxCoreHelper>().is48ResourceNeededForCmdBuffer());
}
XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenFlagForce48bResourcesForCmdBufferWhenAskingIf48bResourceNeededForCmdBufferThenReturnTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.Force48bResourcesForCmdBuffer.set(1);
    EXPECT_TRUE(getHelper<GfxCoreHelper>().is48ResourceNeededForCmdBuffer());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenCooperativeKernelWhenAskingForSingleTileDispatchThenReturnFalse) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();

    EXPECT_FALSE(helper.singleTileExecImplicitScalingRequired(true));
    EXPECT_FALSE(helper.singleTileExecImplicitScalingRequired(false));

    debugManager.flags.SingleTileExecutionForCooperativeKernels.set(1);

    EXPECT_TRUE(helper.singleTileExecImplicitScalingRequired(true));
    EXPECT_FALSE(helper.singleTileExecImplicitScalingRequired(false));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugVariableSetWhenAskingForDuplicatedInOrderHostStorageThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();
    auto &rootExecEnv = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    const auto &compilerProductHelper = rootExecEnv.getHelper<CompilerProductHelper>();

    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(1);
    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(0);
    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(-1);

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    EXPECT_TRUE(helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);
    EXPECT_FALSE(helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugVariableSetWhenAskingForInOrderAtomicSignalingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();
    auto &rootExecEnv = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];
    const auto &compilerProductHelper = rootExecEnv.getHelper<CompilerProductHelper>();

    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(1);
    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(0);
    EXPECT_EQ(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo), helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.Enable64BitAddressing.set(-1);

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);
    EXPECT_TRUE(helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    EXPECT_FALSE(helper.inOrderAtomicSignallingEnabled(rootExecEnv));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDisabledFlagEnableExtendedScratchSurfaceSizeWhenCallGetMaxScratchSizeThenSizeIsCorrect) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    uint32_t maxScratchSize = 256 * MemoryConstants::kiloByte;
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(maxScratchSize, gfxCoreHelper.getMaxScratchSize(productHelper));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenCallGetDefaultSshSizeThenCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    size_t expectedDefaultSshSize = HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState);
    EXPECT_EQ(expectedDefaultSshSize, gfxCoreHelper.getDefaultSshSize(productHelper));
}

using ProductHelperTestXe3p = ::testing::Test;

XE3P_CORETEST_F(ProductHelperTestXe3p, when64bAddressingIsEnabledForRTThenResourcesAreNot48b) {
    MockExecutionEnvironment executionEnvironment{};
    auto productHelper = &executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper->is48bResourceNeededForRayTracing());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported());

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported());

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenIsTranslationExceptionSupportedThenTrueIsReturned) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTranslationExceptionSupported());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCoreWithEnginesCheck, whenGetEnginesCalledThenRegularCcsIsAvailable) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0));

    auto &engines = device->getGfxCoreHelper().getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    EXPECT_EQ(device->allEngines.size(), engines.size());

    for (size_t idx = 0; idx < engines.size(); idx++) {
        countEngine(engines[idx].first, engines[idx].second);
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::regular));
    EXPECT_EQ(0u, getEngineCount(aub_stream::ENGINE_CCCS, EngineUsage::regular));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenAskingForIsIpSamplingSupportedThenReturnFalse) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isIpSamplingSupported(*defaultHwInfo));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenGrfCount512WhenHeaplessModeDisabledThenAdjustedMaxThreadsPerThreadGroup) {
    uint32_t threadsPerThreadGroup = 22;
    uint32_t expectedMaxThreadsPerThreadGroup = 32u;
    const auto &productHelper = getHelper<ProductHelper>();
    auto values = {16, 32};
    for (auto simt : values) {
        EXPECT_EQ(expectedMaxThreadsPerThreadGroup, productHelper.adjustMaxThreadsPerThreadGroup(threadsPerThreadGroup, simt, 512, false));
    }

    // adjust is not done
    for (auto simt : values) {
        EXPECT_EQ(threadsPerThreadGroup, productHelper.adjustMaxThreadsPerThreadGroup(threadsPerThreadGroup, simt, 512, true));
    }
}
