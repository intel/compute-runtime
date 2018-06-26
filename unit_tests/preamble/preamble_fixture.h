/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/preamble.h"
#include "runtime/helpers/options.h"
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
