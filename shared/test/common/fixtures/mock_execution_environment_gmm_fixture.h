/*
 * Copyright (C) 2020-2023 Intel Corporation
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
    void setUp();
    void tearDown();

  public:
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    GmmHelper *getGmmHelper();
    GmmClientContext *getGmmClientContext();
};
} // namespace NEO
