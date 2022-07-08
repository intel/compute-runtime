/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "platforms.h"

using namespace NEO;
using ProductConfigHelperXeHpCoreTests = ::testing::Test;

XE_HP_CORE_TEST_F(ProductConfigHelperXeHpCoreTests, givenVariousVariantsOfXeHpAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hp_core", "xe_hp", "xehp", "XeHp"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::getReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HP_RELEASE);
    }
}