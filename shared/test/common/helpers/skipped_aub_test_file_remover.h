/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"

#include <string>
#include <vector>

namespace NEO {
struct RootDeviceEnvironment;

class SkippedAubTestFileRemover : public ::testing::EmptyTestEventListener {
  public:
    static void recordAubFile(const RootDeviceEnvironment &rootDeviceEnvironment);

    void OnTestEnd(const ::testing::TestInfo &testInfo) override;

  private:
    static std::vector<std::string> aubFilesOfCurrentTest;
};

} // namespace NEO
