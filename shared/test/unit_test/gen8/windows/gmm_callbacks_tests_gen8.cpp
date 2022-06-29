/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8GmmCallbacksTests = ::testing::Test;

GEN8TEST_F(Gen8GmmCallbacksTests, GivenDefaultWhenNotifyingAubCaptureThenDeviceCallbackIsNotSupported) {
    EXPECT_EQ(0, DeviceCallbacks<FamilyType>::notifyAubCapture(nullptr, 0, 0, false));
}

GEN8TEST_F(Gen8GmmCallbacksTests, GivenDefaultWhenWritingL3AddressThenTtCallbackIsNotSupported) {
    EXPECT_EQ(0, TTCallbacks<FamilyType>::writeL3Address(nullptr, 1, 2));
}
