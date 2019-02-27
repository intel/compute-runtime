/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/options.h"
#include "runtime/helpers/preamble.h"
#include "unit_tests/command_stream/linear_stream_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/hw_parse.h"

using namespace OCLRT;

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
        const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();
        HardwareInfo *pHwInfo = const_cast<HardwareInfo *>(&hwInfo);
        pOldWaTable = pHwInfo->pWaTable;
        memcpy(&testWaTable, pOldWaTable, sizeof(testWaTable));
        pHwInfo->pWaTable = &testWaTable;
    }
    void TearDown() override {
        const HardwareInfo &hwInfo = pPlatform->getDevice(0)->getHardwareInfo();
        HardwareInfo *pHwInfo = const_cast<HardwareInfo *>(&hwInfo);
        pHwInfo->pWaTable = pOldWaTable;
        PlatformFixture::TearDown();
        HardwareParse::TearDown();
        LinearStreamFixture::TearDown();
        ::testing::Test::TearDown();
    }

    WorkaroundTable testWaTable;
    const WorkaroundTable *pOldWaTable;
};
