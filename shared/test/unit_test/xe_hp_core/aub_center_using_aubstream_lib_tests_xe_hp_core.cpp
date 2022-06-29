/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/test_macros/hw_test.h"
using namespace NEO;
using XeHpCoreAubCenterTests = ::testing::Test;

XE_HP_CORE_TEST_F(XeHpCoreAubCenterTests, GivenUseAubStreamDebugVariableSetWhenAubCenterIsCreatedThenAubCenterCreatesAubManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());

    MockAubCenter aubCenter(defaultHwInfo.get(), gmmHelper, false, "", CommandStreamReceiverType::CSR_AUB);

    EXPECT_NE(nullptr, aubCenter.aubManager.get());
}
