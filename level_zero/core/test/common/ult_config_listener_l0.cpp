/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ult_config_listener_l0.h"

#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/common/test_macros/test_matcher_registry.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/ddi/ze_ddi_tables.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

void L0::UltConfigListenerL0::OnTestStart(const ::testing::TestInfo &testInfo) {
    if (!NEO::TestMatcherRegistry::willRunForCurrentProduct(testInfo, ::productFamily)) {
        return;
    }
    BaseUltConfigListener::OnTestStart(testInfo);

    globalDriverDispatch.core.isValidFlag = true;
    globalDriverDispatch.tools.isValidFlag = true;
    globalDriverDispatch.sysman.isValidFlag = true;
    globalDriverDispatch.runtime.isValidFlag = true;
    globalDriverHandles->clear();
}

void L0::UltConfigListenerL0::OnTestEnd(const ::testing::TestInfo &testInfo) {
    if (!NEO::TestMatcherRegistry::willRunForCurrentProduct(testInfo, ::productFamily)) {
        return;
    }

    globalDriverDispatch.core.isValidFlag = false;
    globalDriverDispatch.tools.isValidFlag = false;
    globalDriverDispatch.sysman.isValidFlag = false;
    globalDriverDispatch.runtime.isValidFlag = false;
    EXPECT_TRUE(globalDriverHandles->empty());
    EXPECT_EQ(nullptr, L0::Sysman::globalSysmanDriver);

    BaseUltConfigListener::OnTestEnd(testInfo);
}
