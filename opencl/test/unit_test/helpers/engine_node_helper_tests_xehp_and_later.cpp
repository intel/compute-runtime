/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

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