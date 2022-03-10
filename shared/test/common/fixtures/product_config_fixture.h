/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_base.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct ProductConfigTests : public ::testing::Test {
    void SetUp() override {
        hwInfo = *NEO::defaultHwInfo;
        hwInfoConfig = NEO::HwInfoConfig::get(productFamily);
    }

    NEO::HwInfoConfig *hwInfoConfig = nullptr;
    NEO::HardwareInfo hwInfo = {};
    PRODUCT_CONFIG productConfig = UNKNOWN_ISA;
};