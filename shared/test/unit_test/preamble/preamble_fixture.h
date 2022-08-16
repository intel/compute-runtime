/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"

using namespace NEO;

struct PreambleFixture : public DeviceFixture,
                         public LinearStreamFixture,
                         public HardwareParse,
                         public ::testing::Test {
    void SetUp() override {
        DeviceFixture::setUp();
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
        DeviceFixture::tearDown();
    }
};

class PreambleVfeState : public DeviceFixture,
                         public ::testing::Test,
                         public LinearStreamFixture,
                         public HardwareParse {
  public:
    void SetUp() override {
        ::testing::Test::SetUp();
        LinearStreamFixture::setUp();
        HardwareParse::setUp();
        DeviceFixture::setUp();
        testWaTable = &pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->workaroundTable;
    }
    void TearDown() override {
        DeviceFixture::tearDown();
        HardwareParse::tearDown();
        LinearStreamFixture::tearDown();
        ::testing::Test::TearDown();
    }

    WorkaroundTable *testWaTable = nullptr;
};
