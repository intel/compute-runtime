/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "per_product_test_definitions.h"

using namespace NEO;
using EngineNodeHelperCriTests = ::Test<DeviceFixture>;

CRITEST_F(EngineNodeHelperCriTests, WhenGetBcsEngineTypeIsCalledForCriThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto pHwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto deviceBitfield = pDevice->getDeviceBitfield();
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    for (const auto ftrBcsInfoVal : {0b111, 0b110}) { // first return main copy engine (BCS1), then BCS2
        selectorCopyEngine.isMainUsed.store(false);
        pHwInfo->featureTable.ftrBcsInfo = ftrBcsInfoVal;
        EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    }

    for (const auto ftrBcsInfoVal : {0b10111, 0b10110}) { // first return main copy engine (BCS1), then round robin between copy engines BCS2/BCS4
        selectorCopyEngine.isMainUsed.store(false);
        pHwInfo->featureTable.ftrBcsInfo = ftrBcsInfoVal;
        EXPECT_EQ(ENGINE_BCS1, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
        EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, false));
    }
}
