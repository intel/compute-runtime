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
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe_hpc_core_base.h"
using namespace NEO;
using XeHpcCoreAubCenterTests = ::testing::Test;

XE_HPC_CORETEST_F(XeHpcCoreAubCenterTests, GivenUseAubStreamDebugVariableSetWhenAubCenterIsCreatedThenAubCenterCreatesAubManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());

    MockAubCenter aubCenter(defaultHwInfo.get(), gmmHelper, false, "", CommandStreamReceiverType::CSR_AUB);

    EXPECT_NE(nullptr, aubCenter.aubManager.get());
}
