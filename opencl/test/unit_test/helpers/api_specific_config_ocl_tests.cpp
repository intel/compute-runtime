/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigOclTests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::OCL, ApiSpecificConfig::getApiType());
}

} // namespace NEO