/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "test.h"

using namespace NEO;

typedef ::testing::Test Gen11GmmCallbacksTests;

GEN11TEST_F(Gen11GmmCallbacksTests, notSupportedDeviceCallback) {
    EXPECT_EQ(0, DeviceCallbacks<FamilyType>::notifyAubCapture(nullptr, 0, 0, false));
}

GEN11TEST_F(Gen11GmmCallbacksTests, notSupportedTTCallback) {
    EXPECT_EQ(0, TTCallbacks<FamilyType>::writeL3Address(nullptr, 1, 2));
}
