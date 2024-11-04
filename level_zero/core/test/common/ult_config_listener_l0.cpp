/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ult_config_listener_l0.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

void L0::UltConfigListenerL0::OnTestStart(const ::testing::TestInfo &testInfo) {
    BaseUltConfigListener::OnTestStart(testInfo);

    globalDriverHandle = nullptr;
    L0::globalDriver = nullptr;
    driverCount = 0;
}

void L0::UltConfigListenerL0::OnTestEnd(const ::testing::TestInfo &testInfo) {

    EXPECT_EQ(nullptr, globalDriverHandle);
    EXPECT_EQ(0u, driverCount);
    EXPECT_EQ(nullptr, L0::globalDriver);
    EXPECT_EQ(nullptr, L0::Sysman::globalSysmanDriver);

    BaseUltConfigListener::OnTestEnd(testInfo);
}
