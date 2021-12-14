/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;
using EngineNodeHelperPvcTests = ::Test<ClDeviceFixture>;

PVCTEST_F(EngineNodeHelperPvcTests, WhenGetBcsEngineTypeIsCalledForPVCThenCorrectBcsEngineIsReturned) {
    using namespace aub_stream;

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

    auto pHwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    pHwInfo->featureTable.ftrBcsInfo = 0b11111;
    auto deviceBitfield = 0b10;
    auto &selectorCopyEngine = pDevice->getNearestGenericSubDevice(0)->getSelectorCopyEngine();

    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, true));
    EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, true));
    EXPECT_EQ(ENGINE_BCS2, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, true));
    EXPECT_EQ(ENGINE_BCS4, EngineHelpers::getBcsEngineType(*pHwInfo, deviceBitfield, selectorCopyEngine, true));
}
