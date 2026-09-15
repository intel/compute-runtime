/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

extern bool sysmanUltsEnable;

using namespace NEO;

namespace L0 {
namespace ult {

class SysmanEnabledFixture : public ::testing::Test {
  public:
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        debugManager.flags.ZES_ENABLE_SYSMAN.set(true);
    }

  protected:
    DebugManagerStateRestore restorer;
};

} // namespace ult
} // namespace L0
