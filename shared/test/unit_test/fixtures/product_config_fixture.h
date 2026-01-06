/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test_base.h"

#include "gtest/gtest.h"
#include "neo_aot_platforms.h"

using namespace NEO;

template <typename T>
struct ProductConfigTest : public T {
    void SetUp() override {
        T::SetUp();
        hwInfo = *NEO::defaultHwInfo;
        compilerProductHelper = NEO::CompilerProductHelper::create(productFamily);
    }

    std::unique_ptr<NEO::CompilerProductHelper> compilerProductHelper = nullptr;
    NEO::HardwareInfo hwInfo = {};
    uint32_t productConfig = AOT::UNKNOWN_ISA;
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
    const HardwareIpVersion invalidConfig = {CommonConstants::invalidRevisionID};
};

using ProductConfigTests = ProductConfigTest<::testing::Test>;
