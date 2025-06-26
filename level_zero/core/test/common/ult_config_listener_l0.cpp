/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ult_config_listener_l0.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/ddi/ze_ddi_tables.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

void L0::UltConfigListenerL0::OnTestStart(const ::testing::TestInfo &testInfo) {
    BaseUltConfigListener::OnTestStart(testInfo);
    globalDriverDispatch.core.isValidFlag = true;
    globalDriverDispatch.tools.isValidFlag = true;
    globalDriverDispatch.sysman.isValidFlag = true;
    globalDriverHandles->clear();
}

void L0::UltConfigListenerL0::OnTestEnd(const ::testing::TestInfo &testInfo) {
    globalDriverDispatch.core.isValidFlag = false;
    globalDriverDispatch.tools.isValidFlag = false;
    globalDriverDispatch.sysman.isValidFlag = false;
    EXPECT_TRUE(globalDriverHandles->empty());
    EXPECT_EQ(nullptr, L0::Sysman::globalSysmanDriver);

    BaseUltConfigListener::OnTestEnd(testInfo);
}
