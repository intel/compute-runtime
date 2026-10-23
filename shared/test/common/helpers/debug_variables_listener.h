/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include "gtest/gtest.h"

namespace NEO {

class DebugVariablesListener : public ::testing::EmptyTestEventListener {
  private:
    void OnTestIterationStart(const ::testing::UnitTest &, int) override;
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
    void OnTestIterationEnd(const ::testing::UnitTest &, int) override;

    DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;
};

} // namespace NEO
