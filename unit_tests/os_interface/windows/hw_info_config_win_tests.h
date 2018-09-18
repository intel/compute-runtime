/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/os_interface/hw_info_config.h"
#include "unit_tests/os_interface/hw_info_config_tests.h"

#include <memory>

namespace OCLRT {

struct DummyHwConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
};

struct HwInfoConfigTestWindows : public HwInfoConfigTest {
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<OSInterface> osInterface;
    HardwareInfo testHwInfo;
    DummyHwConfig hwConfig;
};

} // namespace OCLRT
