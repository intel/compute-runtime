/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/options.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

using namespace NEO;

namespace NEO {
extern bool getDevicesResult;
}; // namespace NEO

struct PlatformNegativeTest : public ::testing::Test {
    void SetUp() override {
        pPlatform.reset(new Platform());
    }

    std::unique_ptr<Platform> pPlatform;
};

TEST_F(PlatformNegativeTest, GivenPlatformWhenGetDevicesFailedThenFalseIsReturned) {
    VariableBackup<decltype(getDevicesResult)> bkp(&getDevicesResult, false);

    auto ret = pPlatform->initialize();
    EXPECT_FALSE(ret);
}
