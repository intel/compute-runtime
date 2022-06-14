/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;
using ProductConfigHelperXeHpgCoreTests = ::testing::Test;

XE_HPC_CORETEST_F(ProductConfigHelperXeHpgCoreTests, givenVariousVariantsOfXeHpcAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hpc_core", "xe_hpc", "xehpc", "XeHpc"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::returnReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HPC_RELEASE);
    }
}