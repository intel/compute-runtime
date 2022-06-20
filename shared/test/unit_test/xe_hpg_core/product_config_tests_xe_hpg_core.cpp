/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;
using ProductConfigHelperXeHpgCoreTests = ::testing::Test;

XE_HPG_CORETEST_F(ProductConfigHelperXeHpgCoreTests, givenVariousVariantsOfAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hpg_core", "xe_hpg", "xehpg", "XeHpg"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::returnReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HPG_RELEASE);
    }
}