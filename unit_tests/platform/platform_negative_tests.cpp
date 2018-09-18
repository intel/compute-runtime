/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/variable_backup.h"
#include "test.h"

using namespace OCLRT;

namespace OCLRT {
extern bool getDevicesResult;
}; // namespace OCLRT

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
