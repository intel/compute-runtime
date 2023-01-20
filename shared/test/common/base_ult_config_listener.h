/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"

#include "gtest/gtest.h"

namespace NEO {

class BaseUltConfigListener : public ::testing::EmptyTestEventListener {
  protected:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;

  private:
    DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;
    HardwareInfo referencedHwInfo;
};

} // namespace NEO
