/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "platforms.h"

using namespace NEO;

template <typename T>
struct ProductConfigTest : public T {
    void SetUp() override {
        T::SetUp();
        hwInfo = *NEO::defaultHwInfo;
        hwInfoConfig = NEO::HwInfoConfig::get(productFamily);
    }

    NEO::HwInfoConfig *hwInfoConfig = nullptr;
    NEO::HardwareInfo hwInfo = {};
    AOT::PRODUCT_CONFIG productConfig = AOT::UNKNOWN_ISA;
};

struct ProductConfigHwInfoTests : public ProductConfigTest<::testing::TestWithParam<std::tuple<AOT::PRODUCT_CONFIG, PRODUCT_FAMILY>>> {
    void SetUp() override {
        ProductConfigTest::SetUp();
        std::tie(productConfig, prod) = GetParam();
        if (prod != productFamily) {
            GTEST_SKIP();
        }
    }
    PRODUCT_FAMILY prod = IGFX_UNKNOWN;
    const AheadOfTimeConfig invalidConfig = {CommonConstants::invalidRevisionID};
};

using ProductConfigTests = ProductConfigTest<::testing::Test>;
using ProductConfigHwInfoBadRevisionTests = ProductConfigHwInfoTests;
using ProductConfigHwInfoBadArchTests = ProductConfigHwInfoTests;
