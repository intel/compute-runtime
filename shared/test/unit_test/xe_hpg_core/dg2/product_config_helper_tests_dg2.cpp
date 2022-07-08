/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;
using ProductConfigHelperDg2Tests = ::testing::Test;

DG2TEST_F(ProductConfigHelperDg2Tests, givenVariousVariantsOfXeHpgAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hpg_core", "xe_hpg", "xehpg", "XeHpg"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::getReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HPG_RELEASE);
    }
}