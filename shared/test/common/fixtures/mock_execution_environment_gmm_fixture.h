/*
 * Copyright (C) 2020-2021 Intel Corporation
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
    void SetUp();
    void TearDown();

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;

  public:
    GmmHelper *getGmmHelper();
    GmmClientContext *getGmmClientContext();
};
} // namespace NEO