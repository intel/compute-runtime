/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/oclc_extensions_extra.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OclcExtensionsExtraTests, whenAskingForExtraOpenCLCFeaturesThenNoExtraFeaturesAreReturned) {
    OpenClCFeaturesContainer featuresContainer{};
    ASSERT_TRUE(featuresContainer.empty());
    auto releaseHelper = ReleaseHelper::create(defaultHwInfo->ipVersion);
    getOpenclCFeaturesListExtra(releaseHelper.get(), featuresContainer);
    EXPECT_TRUE(featuresContainer.empty());
}