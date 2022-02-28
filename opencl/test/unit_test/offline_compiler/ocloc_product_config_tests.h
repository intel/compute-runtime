/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

namespace NEO {
struct OclocProductConfigTests : public ::testing::TestWithParam<std::tuple<PRODUCT_CONFIG, PRODUCT_FAMILY>> {
    void SetUp() override {
        std::tie(productConfig, productFamily) = GetParam();
        mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    }

    PRODUCT_CONFIG productConfig;
    PRODUCT_FAMILY productFamily;
    std::unique_ptr<MockOfflineCompiler> mockOfflineCompiler;
};
} // namespace NEO