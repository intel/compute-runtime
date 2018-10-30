/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/aub_center.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"
using namespace OCLRT;

class MockAubCenter : public AubCenter {
  public:
    using AubCenter::AubCenter;
    using AubCenter::aubManager;
};

TEST(AubCenter, GivenUseAubStreamDebugVarSetWhenAubCenterIsCreatedThenAubMangerIsInitialized) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseAubStream.set(true);

    MockAubCenter aubCenter;

    EXPECT_NE(nullptr, aubCenter.aubManager.get());
}
