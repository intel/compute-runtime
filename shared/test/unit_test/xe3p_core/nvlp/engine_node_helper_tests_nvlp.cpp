/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "per_product_test_definitions.h"

using namespace NEO;
using EngineNodeHelperNvlpTests = ::Test<DeviceFixture>;

NVLPTEST_F(EngineNodeHelperNvlpTests, WhenGetBcsEngineTypeIsCalledForNvlThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;

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
