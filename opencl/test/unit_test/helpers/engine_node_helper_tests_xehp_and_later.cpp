/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using EngineNodeHelperTestsXeHPAndLater = ::Test<ClDeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, EngineNodeHelperTestsXeHPAndLater, WhenGetBcsEngineTypeIsCalledThenBcsEngineIsReturned) {
    const auto hwInfo = pDevice->getHardwareInfo();
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, {}, selectorCopyEngine, false));
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenDebugVariableSetWhenAskingForEngineTypeThenReturnTheSameAsVariableIndex, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    DeviceBitfield deviceBitfield = 0b11;

    const auto hwInfo = pDevice->getHardwareInfo();
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    for (int32_t i = 0; i <= 9; i++) {
        DebugManager.flags.ForceBcsEngineIndex.set(i);

        if (i == 0) {
            EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
        } else if (i <= 8) {
            EXPECT_EQ(static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + i - 1), EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
        } else {
            EXPECT_ANY_THROW(EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, false));
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenForceBCSForInternalCopyEngineWhenGetBcsEngineTypeForInternalEngineThenForcedTypeIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceBCSForInternalCopyEngine.set(0u);

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0xff;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
    DeviceBitfield deviceBitfield = 0xff;

    {
        DebugManager.flags.ForceBCSForInternalCopyEngine.set(0u);
        auto engineType = EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, engineType);
    }
    {
        DebugManager.flags.ForceBCSForInternalCopyEngine.set(3u);
        auto engineType = EngineHelpers::getBcsEngineType(hwInfo, deviceBitfield, selectorCopyEngine, true);
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, engineType);
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEngines, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DeviceBitfield deviceBitfield = 0b10;

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo.set(7);
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
    for (int32_t i = 0; i <= 20; i++) {
        while (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSubDeviceEngineSupported(hwInfo, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType - aub_stream::EngineType::ENGINE_BCS1 + 1)) {
            expectedEngineType++;
            if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
                expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
            }
        }

        auto engineType = EngineHelpers::selectLinkCopyEngine(hwInfo, deviceBitfield, selectorCopyEngine.selector);
        EXPECT_EQ(engineType, static_cast<aub_stream::EngineType>(expectedEngineType));

        expectedEngineType++;
        if (static_cast<aub_stream::EngineType>(expectedEngineType) > aub_stream::EngineType::ENGINE_BCS8) {
            expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
        }
    }
}

HWTEST2_F(EngineNodeHelperTestsXeHPAndLater, givenEnableCmdQRoundRobindBcsEngineAssignAndMainCopyEngineIncludedWhenSelectLinkCopyEngineThenRoundRobinOverAllAvailableLinkedCopyEnginesAndMainCopyEngine, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(0);
    DeviceBitfield deviceBitfield = 0b10;

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
    for (int32_t i = 0; i <= 20; i++) {
        while (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSubDeviceEngineSupported(hwInfo, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
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

        auto engineType = EngineHelpers::selectLinkCopyEngine(hwInfo, deviceBitfield, selectorCopyEngine.selector);
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
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(0);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(6);
    DeviceBitfield deviceBitfield = 0b10;

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS;
    for (int32_t i = 0; i <= 20; i++) {
        while (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSubDeviceEngineSupported(hwInfo, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
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

        auto engineType = EngineHelpers::selectLinkCopyEngine(hwInfo, deviceBitfield, selectorCopyEngine.selector);
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
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(6);
    DeviceBitfield deviceBitfield = 0b10;

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS1;
    for (int32_t i = 0; i <= 20; i++) {
        while (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSubDeviceEngineSupported(hwInfo, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
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

        auto engineType = EngineHelpers::selectLinkCopyEngine(hwInfo, deviceBitfield, selectorCopyEngine.selector);
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
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.set(1u);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.set(2);
    DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.set(5);
    DeviceBitfield deviceBitfield = 0b10;

    auto hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.featureTable.ftrBcsInfo = 0x17f;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    int32_t expectedEngineType = aub_stream::EngineType::ENGINE_BCS3;
    for (int32_t i = 0; i <= 20; i++) {
        while (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSubDeviceEngineSupported(hwInfo, deviceBitfield, static_cast<aub_stream::EngineType>(expectedEngineType)) || !hwInfo.featureTable.ftrBcsInfo.test(expectedEngineType == aub_stream::EngineType::ENGINE_BCS
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

        auto engineType = EngineHelpers::selectLinkCopyEngine(hwInfo, deviceBitfield, selectorCopyEngine.selector);
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