/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ult_config_listener_l0.h"

#include "level_zero/core/source/driver/driver.h"

void L0::UltConfigListenerL0::OnTestStart(const ::testing::TestInfo &testInfo) {
    BaseUltConfigListener::OnTestStart(testInfo);

    globalDriverHandle = nullptr;
    driverCount = 0;
}

void L0::UltConfigListenerL0::OnTestEnd(const ::testing::TestInfo &testInfo) {

    EXPECT_EQ(nullptr, globalDriverHandle);
    EXPECT_EQ(0u, driverCount);

    BaseUltConfigListener::OnTestEnd(testInfo);
}
