/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"

using ProductConfigHelperTests = ::testing::Test;

struct AotDeviceInfoTests : public ProductConfigHelperTests {
    AotDeviceInfoTests() {
        productConfigHelper = std::make_unique<ProductConfigHelper>();
        aotInfos = productConfigHelper->getDeviceAotInfo();
    }
    std::unique_ptr<ProductConfigHelper> productConfigHelper;
    std::vector<DeviceAotInfo> aotInfos{};
};

template <typename EqComparableT>
auto findAcronym(const EqComparableT &lhs) {
    return [&lhs](const auto &rhs) { return lhs == rhs; };
}