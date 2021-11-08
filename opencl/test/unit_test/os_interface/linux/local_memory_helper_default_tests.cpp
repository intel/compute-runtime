/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "test.h"

using namespace NEO;

TEST(LocalMemoryHelperTestsDefault, givenUnsupportedPlatformWhenTranslateIfRequiredReturnSameData) {
    auto *data = new uint8_t{};
    auto localMemHelper = LocalMemoryHelper::get(IGFX_UNKNOWN);
    auto ret = localMemHelper->translateIfRequired(data, 1);
    EXPECT_EQ(ret.get(), data);
}
