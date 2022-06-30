/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include <memory>

namespace NEO {

struct MockExecutionEnvironment;
struct RootDeviceEnvironment;

struct HwInfoConfigTestWindows : public HwInfoConfigTest {
    HwInfoConfigTestWindows();
    ~HwInfoConfigTestWindows();

    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<OSInterface> osInterface;
    MockHwInfoConfigHw<IGFX_UNKNOWN> hwConfig;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;
};

} // namespace NEO
