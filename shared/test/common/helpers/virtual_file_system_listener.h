/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"

namespace NEO {

class VirtualFileSystemListener : public ::testing::TestEventListener {
  private:
    void OnTestStart(const testing::TestInfo &) override;
    void OnTestEnd(const testing::TestInfo &) override;
    void OnTestProgramStart(const testing::UnitTest &) override;
    void OnTestIterationStart(const testing::UnitTest &, int) override;
    void OnEnvironmentsSetUpStart(const testing::UnitTest &) override;
    void OnEnvironmentsSetUpEnd(const testing::UnitTest &) override;
    void OnTestPartResult(const testing::TestPartResult &) override;
    void OnEnvironmentsTearDownStart(const testing::UnitTest &) override;
    void OnEnvironmentsTearDownEnd(const testing::UnitTest &) override;
    void OnTestIterationEnd(const testing::UnitTest &, int) override;
    void OnTestProgramEnd(const testing::UnitTest &) override;
};

} // namespace NEO
