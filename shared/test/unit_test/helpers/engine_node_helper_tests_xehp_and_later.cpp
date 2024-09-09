/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using EngineNodeHelperTestsXeHPAndLater = ::Test<DeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, EngineNodeHelperTestsXeHPAndLater, WhenGetBcsEngineTypeIsCalledThenBcsEngineIsReturned) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, {}, selectorCopyEngine, false));
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenDebugVariableSetWhenAskingForEngineTypeThenReturnTheSameAsVariableIndex, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    DeviceBitfield deviceBitfield = 0b11;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    for (int32_t i = 0; i <= 9; i++) {
        debugManager.flags.ForceBcsEngineIndex.set(i);

        if (i == 0) {
            EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        } else if (i <= 8) {
            EXPECT_EQ(static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + i - 1), EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        } else {
            EXPECT_ANY_THROW(EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenForceBCSForInternalCopyEngineWhenGetBcsEngineTypeForInternalEngineThenForcedTypeIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceBCSForInternalCopyEngine.set(0u);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0xff;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    DeviceBitfield deviceBitfield = 0xff;

    {
        debugManager.flags.ForceBCSForInternalCopyEngine.set(0u);
        auto engineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, engineType);
    }
    {
        debugManager.flags.ForceBCSForInternalCopyEngine.set(3u);
        auto engineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, engineType);
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenLessThanFourCopyEnginesWhenGetBcsEngineTypeForInternalEngineThenReturnLastAvailableEngine, IsAtLeastXeHpcCore) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    DeviceBitfield deviceBitfield = 0xff;

    {
        hwInfo.featureTable.ftrBcsInfo = 0b1;
        auto engineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, engineType);
    }
    {
        hwInfo.featureTable.ftrBcsInfo = 0b11;
        auto engineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, engineType);
    }
    {
        hwInfo.featureTable.ftrBcsInfo = 0b111;
        auto engineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, engineType);
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenLessThanFourCopyEnginesWhenGetGpgpuEngineInstancesThenUseLastCopyEngineAsInternal, IsAtLeastXeHpcCore) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();

    auto hasInternalEngine = [](const EngineInstancesContainer &engines, aub_stream::EngineType expectedEngineType) {
        for (auto &[engineType, engineUsage] : engines) {
            if (engineType == expectedEngineType && engineUsage == EngineUsage::internal) {
                return true;
            }
        }
        return false;
    };

    {
        if (aub_stream::EngineType::ENGINE_BCS == productHelper.getDefaultCopyEngine()) {
            hwInfo.featureTable.ftrBcsInfo = 0b1;
            auto &engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
            EXPECT_TRUE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS));
            EXPECT_FALSE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS3));
        }
    }
    {
        hwInfo.featureTable.ftrBcsInfo = 0b11;
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
        EXPECT_TRUE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS1));
        EXPECT_FALSE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS3));
    }
    {
        hwInfo.featureTable.ftrBcsInfo = 0b111;
        auto &engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
        EXPECT_TRUE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS2));
        EXPECT_FALSE(hasInternalEngine(engines, aub_stream::EngineType::ENGINE_BCS3));
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEngines, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DeviceBitfield deviceBitfield = 0b10;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo.set(7);
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    for (int32_t i = 0; i <= 20; i++) {
        while (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            expectedEngineType++;
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(pDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        expectedEngineType++;
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignAndMainCopyEngineIncludedWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEnginesAndMainCopyEngine, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(0);
    DeviceBitfield deviceBitfield = 0b10;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    for (int32_t i = 0; i <= 20; i++) {
        while (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
                                                                                                                                                                                                     ? 0
                                                                                                                                                                                                     : expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            } else {
                expectedEngineType++;
            }
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(pDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        } else {
            expectedEngineType++;
        }
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignAndMainCopyEngineIncludedAndLimitSetWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEnginesAndMainCopyEngine, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(0);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(6);
    DeviceBitfield deviceBitfield = 0b10;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    for (int32_t i = 0; i <= 20; i++) {
        while (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
                                                                                                                                                                                                     ? 0
                                                                                                                                                                                                     : expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            } else {
                expectedEngineType++;
            }
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS5) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(pDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        } else {
            expectedEngineType++;
        }
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS5) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignAndLimitSetWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEnginesAndMainCopyEngine, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(6);
    DeviceBitfield deviceBitfield = 0b10;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    for (int32_t i = 0; i <= 20; i++) {
        while (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
                                                                                                                                                                                                     ? 0
                                                                                                                                                                                                     : expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            } else {
                expectedEngineType++;
            }
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS6) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(pDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        } else {
            expectedEngineType++;
        }
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS6) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignAndStartOffsetIncludedWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEngines, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(2);
    debugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(5);
    DeviceBitfield deviceBitfield = 0b10;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS3;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    for (int32_t i = 0; i <= 20; i++) {
        while (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
                                                                                                                                                                                                     ? 0
                                                                                                                                                                                                     : expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            } else {
                expectedEngineType++;
            }
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS7) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS3;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(pDevice->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        if (expectedEngineType == aub_stream::EngineType::ENGINE_BCS) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        } else {
            expectedEngineType++;
        }
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS7) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS3;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenHpCopyEngineWhenSelectLinkCopyEngineThenHpEngineIsNotSelected, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(8);
    DeviceBitfield deviceBitfield = 0b1;
    hardwareInfo.featureTable.ftrBcsInfo = 0b10010;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex));

    const auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hardwareInfo);
    if (hpEngine == aub_stream::EngineType::NUM_ENGINES) {
        GTEST_SKIP();
    }

    auto &selectorCopyEngine = device->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    auto engineType = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType);

    auto engineType2 = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType2);
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenHpCopyEngineAndBcs0And1And2RegularEnginesWhenDefaultCopyIsNotBcs1ThenHpEngineIsNotSelectedAndDifferentEnginesAreReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(8);
    DeviceBitfield deviceBitfield = 0b1;

    hardwareInfo.featureTable.ftrBcsInfo = 0b10111;
    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex));

    const auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hardwareInfo);
    if (hpEngine == aub_stream::EngineType::NUM_ENGINES) {
        GTEST_SKIP();
    }

    auto &selectorCopyEngine = device->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    auto engineType = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType);

    auto engineType2 = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType2);

    auto &productHelper = device->getRootDeviceEnvironment().getProductHelper();

    if (aub_stream::ENGINE_BCS1 != productHelper.getDefaultCopyEngine()) {
        EXPECT_NE(engineType, engineType2);
        EXPECT_EQ(aub_stream::ENGINE_BCS2, engineType);
        EXPECT_EQ(aub_stream::ENGINE_BCS1, engineType2);
    } else {
        EXPECT_EQ(engineType, engineType2);
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenBcs2HpCopyEngineAndBcs0And1RegularEnginesWhenSelectingLinkCopyEngineThenBcs1IsSelected, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.ContextGroupSize.set(8);
    DeviceBitfield deviceBitfield = 0b1;
    hardwareInfo.featureTable.ftrBcsInfo = 0b00111;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo, rootDeviceIndex));

    const auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hardwareInfo);
    if (hpEngine == aub_stream::EngineType::NUM_ENGINES) {
        GTEST_SKIP();
    }

    auto &selectorCopyEngine = device->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    auto engineType = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType);

    auto engineType2 = EngineHelpers::selectLinkCopyEngine(device->getRootDeviceEnvironment(), deviceBitfield, selectorCopyEngine.selector);
    EXPECT_NE(hpEngine, engineType2);

    EXPECT_EQ(engineType, engineType2);
    EXPECT_EQ(aub_stream::ENGINE_BCS1, engineType2);
}
