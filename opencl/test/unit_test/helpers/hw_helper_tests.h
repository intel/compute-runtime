/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

using HwHelperTest = Test<ClDeviceFixture>;

void testDefaultImplementationOfSetupHardwareCapabilities(HwHelper &hwHelper, const HardwareInfo &hwInfo);

constexpr struct {
    __REVID stepping;
    uint32_t aubStreamStepping;
} steppingPairsToTest[] = {
    {REVISION_A0, AubMemDump::SteppingValues::A},
    {REVISION_A1, AubMemDump::SteppingValues::A},
    {REVISION_A3, AubMemDump::SteppingValues::A},
    {REVISION_B, AubMemDump::SteppingValues::B},
    {REVISION_C, AubMemDump::SteppingValues::C},
    {REVISION_D, AubMemDump::SteppingValues::D},
    {REVISION_K, AubMemDump::SteppingValues::K}};
