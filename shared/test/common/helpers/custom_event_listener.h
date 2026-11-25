/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"

#include <string>
#include <utility>
#include <vector>

class CCustomEventListener : public ::testing::EmptyTestEventListener {
  public:
    CCustomEventListener(::testing::TestEventListener *listener);
    ~CCustomEventListener() override;

    void setHwPrefix(const char *hardwarePrefix);

  private:
    void OnTestIterationStart(const ::testing::UnitTest &unitTest, int iteration) override;
    void OnTestIterationEnd(const ::testing::UnitTest &unitTest, int iteration) override;
    void OnTestPartResult(const ::testing::TestPartResult &testPartResult) override;
    void OnTestEnd(const ::testing::TestInfo &testCase) override;
    void OnTestProgramEnd(const ::testing::UnitTest &unitTest) override;

    ::testing::TestEventListener *listener;
    std::vector<std::pair<std::string, int>> testFailures;

    int currentSeed = -1;
    std::string hardwarePrefix;
};
