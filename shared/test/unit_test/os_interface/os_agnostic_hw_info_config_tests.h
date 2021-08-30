/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

using namespace NEO;

struct DummyHwConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
};

struct OsAgnosticHwInfoConfigTest : public ::testing::Test,
                                    public DeviceFixture {
    void SetUp() override;
    void TearDown() override;

    DummyHwConfig hwConfig;
};
