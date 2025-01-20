/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_sip.h"

#include "gtest/gtest.h"

namespace NEO {

class MockSipListener : public ::testing::TestEventListener {
  private:
    void OnTestStart(const testing::TestInfo &) override{};
    void OnTestEnd(const testing::TestInfo &) override;

    void OnTestProgramStart(const testing::UnitTest &) override{};
    void OnTestIterationStart(const testing::UnitTest &, int) override{};
    void OnEnvironmentsSetUpStart(const testing::UnitTest &) override{};
    void OnEnvironmentsSetUpEnd(const testing::UnitTest &) override{};
    void OnTestPartResult(const testing::TestPartResult &) override{};
    void OnEnvironmentsTearDownStart(const testing::UnitTest &) override{};
    void OnEnvironmentsTearDownEnd(const testing::UnitTest &) override{};
    void OnTestIterationEnd(const testing::UnitTest &, int) override{};
    void OnTestProgramEnd(const testing::UnitTest &) override{};
};

} // namespace NEO
