/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

typedef ::testing::Test Gen9GmmCallbacksTests;

GEN9TEST_F(Gen9GmmCallbacksTests, GivenDefaultWhenNotifyingAubCaptureThenDeviceCallbackIsNotSupported) {
    EXPECT_EQ(0, DeviceCallbacks<FamilyType>::notifyAubCapture(nullptr, 0, 0, false));
}

GEN9TEST_F(Gen9GmmCallbacksTests, GivenDefaultWhenWritingL3AddressThenTtCallbackIsNotSupported) {
    EXPECT_EQ(0, TTCallbacks<FamilyType>::writeL3Address(nullptr, 1, 2));
}
