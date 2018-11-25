/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_aub_center.h"

#include "gtest/gtest.h"
using namespace OCLRT;

TEST(AubCenter, GivenUseAubStreamDebugVariableSetWhenAubCenterIsCreatedThenAubManagerIsNotCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    MockAubCenter aubCenter(platformDevices[0], false, "");

    EXPECT_EQ(nullptr, aubCenter.aubManager.get());
}
