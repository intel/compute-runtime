/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;
using ProductConfigHelperPvcTests = ::testing::Test;

PVCTEST_F(ProductConfigHelperPvcTests, givenVariousVariantsOfXeHpcAcronymsWhenGetReleaseThenCorrectValueIsReturned) {
    std::vector<std::string> acronymsVariants = {"xe_hpc_core", "xe_hpc", "xehpc", "XeHpc"};
    for (auto &acronym : acronymsVariants) {
        ProductConfigHelper::adjustDeviceName(acronym);
        auto ret = ProductConfigHelper::getReleaseForAcronym(acronym);
        EXPECT_EQ(ret, AOT::XE_HPC_RELEASE);
    }
}