/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using EngineNodeHelperPvcTests = ::Test<DeviceFixture>;

PVCTEST_F(EngineNodeHelperPvcTests, WhenGetBcsEngineTypeIsCalledWithoutSelectorEnabledForPVCThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto pHwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto deviceBitfield = pDevice->getDeviceBitfield();

    pHwInfo->featureTable.ftrBcsInfo = 1;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    selectorCopyEngine.isMainUsed.store(true);
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b111;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b11;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b101;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
}

PVCTEST_F(EngineNodeHelperPvcTests, WhenGetBcsEngineTypeIsCalledForPVCThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto pHwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto deviceBitfield = pDevice->getDeviceBitfield();

    pHwInfo->featureTable.ftrBcsInfo = 1;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    selectorCopyEngine.isMainUsed.store(true);
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b111;
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b11;
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b101;
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
}

PVCTEST_F(EngineNodeHelperPvcTests, givenPvcBaseDieA0AndTile1WhenGettingBcsEngineTypeThenDoNotUseBcs1) {
    using namespace aub_stream;
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableCopyEngineSelector.set(1);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto pHwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    pHwInfo->featureTable.ftrBcsInfo = 0b11111;
    pHwInfo->platform.usRevId &= ~PVC::pvcBaseDieRevMask;
    auto deviceBitfield = 0b10;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    {
        auto internalUsage = true;
        EXPECT_EQ(ENGINE_BCS3, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS3, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
    }
    {
        auto internalUsage = false;
        EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage));
    }
}

PVCTEST_F(EngineNodeHelperPvcTests, givenCccsDisabledButDebugVariableSetWhenGetGpgpuEnginesCalledThenSetCccsProperly) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto productHelper = ProductHelper::create(hwInfo.platform.eProductFamily);

    DebugManagerStateRestore restorer;
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    for (uint32_t stepping : {REVISION_A0, REVISION_B}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(stepping, hwInfo);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cooperativeDispatchSupported = productHelper->isCooperativeEngineSupported(hwInfo);

        EXPECT_EQ((stepping == REVISION_B), cooperativeDispatchSupported);

        EXPECT_EQ(cooperativeDispatchSupported ? 13u : 9u, device->allEngines.size());
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
        EXPECT_EQ(cooperativeDispatchSupported ? 13u : 9u, engines.size());

        uint32_t index = 0;

        EXPECT_EQ(aub_stream::ENGINE_CCS, engines[index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[++index].first);
        EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[++index].first); // low priority
        EXPECT_EQ(aub_stream::ENGINE_CCCS, engines[++index].first); // internal
        EXPECT_EQ(aub_stream::ENGINE_BCS, engines[++index].first);  // internal
        EXPECT_EQ(aub_stream::ENGINE_BCS, engines[++index].first);
    }
}

PVCTEST_F(EngineNodeHelperPvcTests, givenCccsDisabledWhenGetGpgpuEnginesCalledThenDontSetCccs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto productHelper = ProductHelper::create(hwInfo.platform.eProductFamily);

    for (uint32_t stepping : {REVISION_A0, REVISION_B}) {
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(stepping, hwInfo);

        auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
        auto &gfxCoreHelper = device->getGfxCoreHelper();

        bool cooperativeDispatchSupported = productHelper->isCooperativeEngineSupported(hwInfo);

        EXPECT_EQ((stepping == REVISION_B), cooperativeDispatchSupported);

        EXPECT_EQ(cooperativeDispatchSupported ? 12u : 8u, device->allEngines.size());
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
        EXPECT_EQ(cooperativeDispatchSupported ? 12u : 8u, engines.size());

        uint32_t index = 0;

        EXPECT_EQ(aub_stream::ENGINE_CCS, engines[index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[++index].first);
        }
        EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[++index].first);
        if (cooperativeDispatchSupported) {
            EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[++index].first);
        }
        EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[++index].first); // low priority
        EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[++index].first); // internal
        EXPECT_EQ(aub_stream::ENGINE_BCS, engines[++index].first);
        EXPECT_EQ(aub_stream::ENGINE_BCS, engines[++index].first);
    }
}

PVCTEST_F(EngineNodeHelperPvcTests, givenCCSEngineWhenCallingIsCooperativeDispatchSupportedThenTrueIsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &rootDeviceEnvironment = *pDevice->executionEnvironment->rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    auto engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::ENGINE_CCS, EngineUsage::cooperative, hwInfo);
    auto retVal = gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment);
    EXPECT_TRUE(retVal);
}

PVCTEST_F(EngineNodeHelperPvcTests, givenCCCSEngineAndRevisionBWhenCallingIsCooperativeDispatchSupportedThenFalseIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &rootDeviceEnvironment = *pDevice->executionEnvironment->rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    auto engineGroupType = gfxCoreHelper.getEngineGroupType(aub_stream::ENGINE_CCCS, EngineUsage::cooperative, hwInfo);
    auto retVal = gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment);
    EXPECT_TRUE(retVal);

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    retVal = gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment);
    EXPECT_FALSE(retVal);
}

PVCTEST_F(EngineNodeHelperPvcTests, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 7;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 9;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsupported) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 9;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.featureTable.ftrBcsInfo.set(0, false);
    hwInfo.featureTable.ftrBcsInfo.set(3, false);
    hwInfo.featureTable.ftrBcsInfo.set(7, false);
    hwInfo.featureTable.ftrBcsInfo.set(8, false);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, givenOneCcsEnabledWhenGetEnginesCalledThenCreateOnlyOneCcs) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 15;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, givenCccsAsDefaultEngineWhenGetEnginesCalledThenChangeDefaultEngine) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 18;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndEightLinkCopyEngines) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 18;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, whenGetGpgpuEnginesThenReturnTwoCccsEnginesAndFourCcsEnginesAndLinkCopyEngines) {
    const auto &productHelper = getHelper<ProductHelper>();

    const size_t numEngines = 18;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

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

PVCTEST_F(EngineNodeHelperPvcTests, givenNonTile0AccessWhenGettingIsBlitCopyRequiredForLocalMemoryThenProperValueIsReturned) {

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    auto &productHelper = getHelper<ProductHelper>();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::bufferHostMemory);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::localMemory);
    hwInfo.platform.usRevId = 0u;

    bool expectedRetVal = true;

    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.memoryBanks = 0b11;
    EXPECT_EQ(expectedRetVal, productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    graphicsAllocation.storageInfo.memoryBanks = 0b10;
    EXPECT_EQ(expectedRetVal, productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    {
        VariableBackup<unsigned short> revisionId{&hwInfo.platform.usRevId};
        revisionId = 0b111000;
        EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    }
    {
        VariableBackup<bool> cloningOfPageTables{&graphicsAllocation.storageInfo.cloningOfPageTables};
        cloningOfPageTables = true;
        EXPECT_EQ(expectedRetVal, productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    }
    {
        VariableBackup<DeviceBitfield> memoryBanks{&graphicsAllocation.storageInfo.memoryBanks};
        memoryBanks = 0b1;
        EXPECT_FALSE(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    }
}
