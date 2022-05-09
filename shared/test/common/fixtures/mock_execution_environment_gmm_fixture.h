/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {

struct MockExecutionEnvironment;
class GmmHelper;
class GmmClientContext;

class MockExecutionEnvironmentGmmFixture {
  protected:
    void SetUp();    // NOLINT(readability-identifier-naming)
    void TearDown(); // NOLINT(readability-identifier-naming)

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;

  public:
    GmmHelper *getGmmHelper();
    GmmClientContext *getGmmClientContext();
};
} // namespace NEO
