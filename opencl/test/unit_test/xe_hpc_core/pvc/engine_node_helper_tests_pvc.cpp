/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;
using EngineNodeHelperPvcTests = ::Test<ClDeviceFixture>;

PVCTEST_F(EngineNodeHelperPvcTests, WhenGetBcsEngineTypeIsCalledWithoutSelectorEnabledForPVCThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto deviceBitfield = pDevice->getDeviceBitfield();

    pHwInfo->featureTable.ftrBcsInfo = 1;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    selectorCopyEngine.isMainUsed.store(true);
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b111;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b11;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b101;
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
}

PVCTEST_F(EngineNodeHelperPvcTests, WhenGetBcsEngineTypeIsCalledForPVCThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto deviceBitfield = pDevice->getDeviceBitfield();

    pHwInfo->featureTable.ftrBcsInfo = 1;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    selectorCopyEngine.isMainUsed.store(true);
    EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b111;
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b11;
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));

    pHwInfo->featureTable.ftrBcsInfo = 0b101;
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, false));
}

PVCTEST_F(EngineNodeHelperPvcTests, givenPvcBaseDieA0AndTile1WhenGettingBcsEngineTypeThenDoNotUseBcs1) {
    using namespace aub_stream;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableCopyEngineSelector.set(1);

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->featureTable.ftrBcsInfo = 0b11111;
    auto deviceBitfield = 0b10;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    {
        auto internalUsage = true;
        EXPECT_EQ(ENGINE_BCS3, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS3, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
    }
    {
        auto internalUsage = false;
        EXPECT_EQ(ENGINE_BCS, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, internalUsage));
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

    DebugManagerStateRestore restorer;
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

PVCTEST_F(EngineNodeHelperPvcTests, givenCccsDisabledWhenGetGpgpuEnginesCalledThenDontSetCccs) {
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

PVCTEST_F(EngineNodeHelperPvcTests, givenCCSEngineWhenCallingIsCooperativeDispatchSupportedThenTrueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto hwInfo = *defaultHwInfo;
    uint64_t hwInfoConfig = defaultHardwareInfoConfigTable[productFamily];
    hardwareInfoSetup[productFamily](&hwInfo, true, hwInfoConfig);
    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo);
    ASSERT_NE(nullptr, device);
    auto clDevice = new MockClDevice{device};
    ASSERT_NE(nullptr, clDevice); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
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

PVCTEST_F(EngineNodeHelperPvcTests, givenCCCSEngineAndRevisionBWhenCallingIsCooperativeDispatchSupportedThenFalseIsReturned) {
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

PVCTEST_F(EngineNodeHelperPvcTests, givenBcsDisabledWhenGetEnginesCalledThenDontCreateAnyBcs) {
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

PVCTEST_F(EngineNodeHelperPvcTests, givenOneBcsEnabledWhenGetEnginesCalledThenCreateOnlyOneBcs) {
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

PVCTEST_F(EngineNodeHelperPvcTests, givenNotAllCopyEnginesWhenSettingEngineTableThenDontAddUnsupported) {
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
    const size_t numEngines = 15;

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
    const size_t numEngines = 18;

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
    const size_t numEngines = 18;

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
    const size_t numEngines = 18;

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

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);
    hwInfo.platform.usRevId = 0u;

    bool expectedRetVal = true;

    graphicsAllocation.storageInfo.cloningOfPageTables = false;
    graphicsAllocation.storageInfo.memoryBanks = 0b11;
    EXPECT_EQ(expectedRetVal, hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    graphicsAllocation.storageInfo.memoryBanks = 0b10;
    EXPECT_EQ(expectedRetVal, hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    {
        VariableBackup<unsigned short> revisionId{&hwInfo.platform.usRevId};
        revisionId = 0b111000;
        EXPECT_FALSE(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
    {
        VariableBackup<bool> cloningOfPageTables{&graphicsAllocation.storageInfo.cloningOfPageTables};
        cloningOfPageTables = true;
        EXPECT_EQ(expectedRetVal, hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
    {
        VariableBackup<DeviceBitfield> memoryBanks{&graphicsAllocation.storageInfo.memoryBanks};
        memoryBanks = 0b1;
        EXPECT_FALSE(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    }
}