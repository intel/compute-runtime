/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/gmm_callbacks.h"
#include "test.h"

using namespace OCLRT;

typedef ::testing::Test Gen10GmmCallbacksTests;

GEN10TEST_F(Gen10GmmCallbacksTests, notSupportedDeviceCallback) {
    EXPECT_EQ(0, DeviceCallbacks<FamilyType>::notifyAubCapture(nullptr, 0, 0, false));
}

GEN10TEST_F(Gen10GmmCallbacksTests, notSupportedTTCallback) {
    EXPECT_EQ(0, TTCallbacks<FamilyType>::writeL3Address(nullptr, 1, 2));
}
