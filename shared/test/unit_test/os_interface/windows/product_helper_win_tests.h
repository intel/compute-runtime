/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include <memory>

namespace NEO {

struct MockExecutionEnvironment;
struct RootDeviceEnvironment;

struct ProductHelperTestWindows : public ProductHelperTest {
    ProductHelperTestWindows();
    ~ProductHelperTestWindows();

    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<OSInterface> osInterface;
    MockProductHelperHw<IGFX_UNKNOWN> hwConfig;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironment;

    template <typename HelperType>
    HelperType &getHelper() const;
};

} // namespace NEO
