/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/ult_hw_config.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"

using namespace NEO;

struct PlatformNegativeTest : public ::testing::Test {
    void SetUp() override {
        pPlatform.reset(new Platform());
    }

    std::unique_ptr<Platform> pPlatform;
};

TEST_F(PlatformNegativeTest, GivenPlatformWhenGetDevicesFailedThenFalseIsReturned) {
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.mockedGetDevicesFuncResult = false;

    auto ret = pPlatform->initialize();
    EXPECT_FALSE(ret);
}
