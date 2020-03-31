/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/test/unit_test/command_stream/linear_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct PreambleFixture : public DeviceFixture,
                         public LinearStreamFixture,
                         public HardwareParse,
                         public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        DeviceFixture::TearDown();
    }
};

class PreambleVfeState : public PlatformFixture,
                         public ::testing::Test,
                         public LinearStreamFixture,
                         public HardwareParse {
  public:
    void SetUp() override {
        ::testing::Test::SetUp();
        LinearStreamFixture::SetUp();
        HardwareParse::SetUp();
        PlatformFixture::SetUp();
        testWaTable = &pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->workaroundTable;
    }
    void TearDown() override {
        PlatformFixture::TearDown();
        HardwareParse::TearDown();
        LinearStreamFixture::TearDown();
        ::testing::Test::TearDown();
    }

    WorkaroundTable *testWaTable;
};
