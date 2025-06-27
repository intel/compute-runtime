/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"

using Xe2HpgMidThreadPreemptionTests = DevicePreemptionTests;

XE2_HPG_CORETEST_F(Xe2HpgMidThreadPreemptionTests, GivenValueArgSetWhenOverrideMidThreadPreemptionSupportThenPreemptionModeDisabled) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    bool value = true;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}

XE2_HPG_CORETEST_F(Xe2HpgMidThreadPreemptionTests, GivenValueArgNotSetWhenOverrideMidThreadPreemptionSupportThenMidThreadPreemptionMode) {
    device->setPreemptionMode(PreemptionMode::MidThread);
    bool value = false;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::MidThread, device->getPreemptionMode());
}

XE2_HPG_CORETEST_F(Xe2HpgMidThreadPreemptionTests, GivenFlagForcePreemptionModeSetAsDisabledWhenOverrideMidThreadPreemptionSupportThenNothingChanged) {
    device->setPreemptionMode(PreemptionMode::Disabled);
    debugManager.flags.ForcePreemptionMode.set(PreemptionMode::Disabled);
    bool value = true;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
    value = false;
    ClPreemptionHelper::overrideMidThreadPreemptionSupport(*context.get(), value);
    EXPECT_EQ(PreemptionMode::Disabled, device->getPreemptionMode());
}