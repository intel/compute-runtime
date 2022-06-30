/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

namespace NEO {
struct OclocProductConfigTests : public ::testing::TestWithParam<std::tuple<AOT::PRODUCT_CONFIG, PRODUCT_FAMILY>> {
    void SetUp() override {
        std::tie(aotConfig.ProductConfig, productFamily) = GetParam();
        mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    }

    AheadOfTimeConfig aotConfig;
    PRODUCT_FAMILY productFamily;
    std::unique_ptr<MockOfflineCompiler> mockOfflineCompiler;
};
} // namespace NEO
