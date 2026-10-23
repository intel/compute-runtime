/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"

#include "gtest/gtest.h"

#include <chrono>
#include <vector>

namespace NEO {

class BaseUltConfigListener : public ::testing::EmptyTestEventListener {
  protected:
    void OnTestIterationStart(const ::testing::UnitTest &, int) override;
    void OnTestIterationEnd(const ::testing::UnitTest &, int) override;
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;

    bool testExcluded = false;

  private:
    void snapshotHwInfo();

    HardwareInfo referencedHwInfo;
    uint64_t referencedFeatureTableHash = 0;
    uint64_t referencedWorkaroundTableHash = 0;
    std::chrono::steady_clock::time_point testStart{};
    uint32_t maxOsContextCountBackup = 0;
    std::vector<char> stateSaveAreaHeaderSnapshot{100};
};

} // namespace NEO
