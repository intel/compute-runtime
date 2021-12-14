/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"
#include "shared/test/common/test_macros/test.h"

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

class PreambleVfeState : public DeviceFixture,
                         public ::testing::Test,
                         public LinearStreamFixture,
                         public HardwareParse {
  public:
    void SetUp() override {
        ::testing::Test::SetUp();
        LinearStreamFixture::SetUp();
        HardwareParse::SetUp();
        DeviceFixture::SetUp();
        testWaTable = &pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->workaroundTable;
    }
    void TearDown() override {
        DeviceFixture::TearDown();
        HardwareParse::TearDown();
        LinearStreamFixture::TearDown();
        ::testing::Test::TearDown();
    }

    WorkaroundTable *testWaTable;
};
