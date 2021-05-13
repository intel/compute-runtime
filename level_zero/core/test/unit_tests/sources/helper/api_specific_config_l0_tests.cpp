/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(ApiSpecificConfigL0Tests, WhenGettingApiTypeThenCorrectTypeIsReturned) {
    EXPECT_EQ(ApiSpecificConfig::L0, ApiSpecificConfig::getApiType());
}

} // namespace NEO