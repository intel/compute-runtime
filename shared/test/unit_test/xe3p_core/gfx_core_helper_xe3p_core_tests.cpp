/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "metrics_library_api_1_0.h"

using GfxCoreHelperTestsXe3pCore = GfxCoreHelperTest;
XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenXe3pCoreWhenAskedForMinimialSimdThen16IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMinimalSIMDSize());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenGettingMetricsLibraryGenIdThenXe3pIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe3P), gfxCoreHelper.getMetricsLibraryGenId());
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

        const auto &hwInfo = device->getHardwareInfo();
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cccsEnabled = !hwInfo.caps.rcsExposureDisabled || debugFlag;

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

    if (!isBCS0Enabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS1, EngineUsage::internal));
    }
    if (isBCSLowPriorityEnabled) {
        EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_BCS1, EngineUsage::lowPriority));
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDefaultMemorySynchronizationCommandsWhenAddingReleaseSynchronizationThenNothingIsProgrammed) {
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();

    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x0, NEO::FenceType::release, rootDeviceEnvironment);
    EXPECT_EQ(0u, commandStream.getUsed());
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
    EXPECT_EQ(static_cast<uint32_t>(-2), NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd));
    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd));
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());
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
    EXPECT_EQ(productHelper.isDcFlushAllowed(), productHelper.isTimestampWaitSupportedForQueues());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenCheckTimestampWaitSupportThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isTimestampWaitSupportedForEvents());
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenMultitileConfigWhenConfiguringHwInfoThenBlitterIsEnabled) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrBlitterRing = true;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
    }

    hwInfo.featureTable.flags.ftrBlitterRing = false;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);
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

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenAskingForGlobalFenceInPostSyncThenReturnFalse) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isGlobalFenceInPostSyncRequired(*defaultHwInfo));
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

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenXe3pCoreWhenAskedForMinimialGrfSizeThen32IsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(32u, gfxCoreHelper.getMinimalGrfSize());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenAskingForImplicitScalingImmWriteOffsetThenAlwaysReturnTsSize) {
    EXPECT_EQ(static_cast<uint32_t>(GfxCoreHelperHw<FamilyType>::getSingleTimestampPacketSizeHw()), ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 2u);

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

    EXPECT_TRUE(helper.duplicatedInOrderCounterStorageEnabled());

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    EXPECT_TRUE(helper.duplicatedInOrderCounterStorageEnabled());

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);
    EXPECT_FALSE(helper.duplicatedInOrderCounterStorageEnabled());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenDebugVariableSetWhenAskingForInOrderAtomicSignalingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();

    EXPECT_TRUE(helper.inOrderAtomicSignallingEnabled());

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);
    EXPECT_TRUE(helper.inOrderAtomicSignallingEnabled());

    debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    EXPECT_FALSE(helper.inOrderAtomicSignallingEnabled());
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

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenProductHelperWhenAskingForIsIpSamplingSupportedThenReturnTrue) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.isIpSamplingSupported(*defaultHwInfo));
}

XE3P_CORETEST_F(ProductHelperTestXe3pCore, givenGrfCount512WhenHeaplessModeDisabledThenAdjustedMaxThreadsPerThreadGroup) {
    uint32_t threadsPerThreadGroup = 22;
    const auto &productHelper = getHelper<ProductHelper>();
    auto values = {16, 32};

    // adjust is not done
    for (auto simt : values) {
        EXPECT_EQ(threadsPerThreadGroup, productHelper.adjustMaxThreadsPerThreadGroup(*defaultHwInfo, threadsPerThreadGroup, simt, 512));
    }
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenSetExtraPropertiesThenFlushLSC) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockExecutionEnvironment mockExecutionEnvironment{};

    PIPE_CONTROL pipeControl = FamilyType::cmdInitPipeControl;
    EXPECT_FALSE(pipeControl.getDataportFlush());
    EXPECT_FALSE(pipeControl.getUnTypedDataPortCacheFlush());

    PipeControlArgs args = {};
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_TRUE(pipeControl.getDataportFlush());
    EXPECT_TRUE(pipeControl.getUnTypedDataPortCacheFlush());
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenXe3pCoreWhenSetStallOnlyBarrierThenResourceBarrierProgrammed) {
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
    auto resourceBarrier = genCmdCast<RESOURCE_BARRIER *>(*itor);
    EXPECT_NE(nullptr, resourceBarrier);
    EXPECT_EQ(RESOURCE_BARRIER::SIGNAL_STAGE::SIGNAL_STAGE_GPGPU, resourceBarrier->getSignalStage());
    EXPECT_FALSE(resourceBarrier->getL1DataportCacheInvalidate());
    EXPECT_FALSE(resourceBarrier->getL1DataportUavFlush());
    EXPECT_FALSE(resourceBarrier->getDisableGoSyncWithWalkerPostSync());
}

struct GfxCoreHelperTestsXe3pCoreResourceBarrier : public GfxCoreHelperTestsXe3pCore,
                                                   public ::testing::WithParamInterface<uint32_t> {
};

XE3P_CORETEST_P(GfxCoreHelperTestsXe3pCoreResourceBarrier, givenXe3pCoreWhenSetStallOnlyBarrierWithDebugFlagThenSetL1CacheFlush) {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;
    constexpr static auto bufferSize = sizeof(RESOURCE_BARRIER);

    DebugManagerStateRestore restorer;
    auto mode = GetParam();
    debugManager.flags.ResourceBarrierL1FlushMode.set(mode);

    PipeControlArgs args;
    args.csStallOnly = true;
    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, PostSyncMode::noWrite, 0u, 0u, args);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList resourceBarrierList = hwParser.getCommandsList<RESOURCE_BARRIER>();
    EXPECT_EQ(1u, resourceBarrierList.size());
    GenCmdList::iterator itor = resourceBarrierList.begin();
    auto resourceBarrier = genCmdCast<RESOURCE_BARRIER *>(*itor);
    EXPECT_NE(nullptr, resourceBarrier);
    if (mode == 1) {
        EXPECT_TRUE(resourceBarrier->getL1DataportCacheInvalidate());
        EXPECT_FALSE(resourceBarrier->getL1DataportUavFlush());
    } else if (mode == 2) {
        EXPECT_FALSE(resourceBarrier->getL1DataportCacheInvalidate());
        EXPECT_TRUE(resourceBarrier->getL1DataportUavFlush());
    } else if (mode == 3) {
        EXPECT_TRUE(resourceBarrier->getL1DataportCacheInvalidate());
        EXPECT_TRUE(resourceBarrier->getL1DataportUavFlush());
    }
}

INSTANTIATE_TEST_SUITE_P(GfxCoreHelperTestsXe3pCoreResourceBarrierValues,
                         GfxCoreHelperTestsXe3pCoreResourceBarrier,
                         ::testing::Values(1, 2, 3));

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, whenIsWalkerPostSyncSkipEnabledCalledThenReturnTrue) {
    DebugManagerStateRestore restorer{};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isWalkerPostSyncSkipEnabled(false));
    EXPECT_TRUE(gfxCoreHelper.isWalkerPostSyncSkipEnabled(true));

    debugManager.flags.EnableWalkerPostSyncSkip.set(1);
    EXPECT_TRUE(gfxCoreHelper.isWalkerPostSyncSkipEnabled(false));

    debugManager.flags.EnableWalkerPostSyncSkip.set(0);
    EXPECT_FALSE(gfxCoreHelper.isWalkerPostSyncSkipEnabled(false));
}

using CompilerProductHelperTestsXe3pCore = Test<DeviceFixture>;

XE3P_CORETEST_F(CompilerProductHelperTestsXe3pCore, givenHeaplessModeWhenApplyExtraInternalOptionsIsCalledThenInternalOptionsAreCorrect) {
    std::string enable64bitAddressing = "-ze-intel-64bit-addressing";

    auto &compilerProductHelper = getHelper<CompilerProductHelper>();

    {
        std::string internalOptions;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);
        EXPECT_TRUE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        std::string internalOptions;
        NEO::CompilerOptions::HeaplessMode heaplessMode = NEO::CompilerOptions::HeaplessMode::disabled;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, compilerProductHelper, heaplessMode);
        EXPECT_FALSE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        std::string internalOptions;
        NEO::CompilerOptions::HeaplessMode heaplessMode = NEO::CompilerOptions::HeaplessMode::enabled;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, compilerProductHelper, heaplessMode);
        EXPECT_TRUE(hasSubstr(internalOptions, enable64bitAddressing));
    }
}

using GfxCoreHelperTestsXe3pCoreWithEnginesCheck = GfxCoreHelperTestWithEnginesCheck;

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCoreWithEnginesCheck, givenWddmWhenGetEnginesCalledThenPowerHintEnginesAreCreated) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
    device->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<MockDriverModelWDDM>());

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());

    for (const auto &engine : engines) {
        countEngine(engine.first, engine.second);
    }

    EXPECT_EQ(1u, getEngineCount(aub_stream::ENGINE_CCS, EngineUsage::powerHint));
    EXPECT_EQ(1u, getEngineCount(productHelper.getDefaultCopyEngine(), EngineUsage::powerHint));
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenRtStacksPerDssBelowLimitsWhenAdjustingRTDispatchGlobalsThenValuesAreNotClamped) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr uint32_t rtStacksPerDss = 1024u;
    RTDispatchGlobals dispatchGlobals = {};
    gfxCoreHelper.adjustRTDispatchGlobals(dispatchGlobals, rtStacksPerDss);

    EXPECT_EQ(rtStacksPerDss, dispatchGlobals.numDSSRTStacks);
    EXPECT_EQ(rtStacksPerDss, dispatchGlobals.syncNumDSSRTStacks);
}

XE3P_CORETEST_F(GfxCoreHelperTestsXe3pCore, givenRtStacksPerDssAboveLimitsWhenAdjustingRTDispatchGlobalsThenValuesAreClamped) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr uint32_t rtStacksPerDss = 8192u;
    RTDispatchGlobals dispatchGlobals = {};
    gfxCoreHelper.adjustRTDispatchGlobals(dispatchGlobals, rtStacksPerDss);

    EXPECT_EQ(2048u, dispatchGlobals.numDSSRTStacks);
    EXPECT_EQ(4096u, dispatchGlobals.syncNumDSSRTStacks);
}
