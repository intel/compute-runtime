/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/product_config_helper.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include "gtest/gtest.h"

namespace NEO {
struct OclocProductConfigTests : public ::testing::TestWithParam<std::tuple<AOT::PRODUCT_CONFIG, PRODUCT_FAMILY>> {
    void SetUp() override {
        std::tie(aotConfig.value, productFamily) = GetParam();
        mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    }

    HardwareIpVersion aotConfig;
    PRODUCT_FAMILY productFamily;
    std::unique_ptr<MockOfflineCompiler> mockOfflineCompiler;
};
} // namespace NEO
