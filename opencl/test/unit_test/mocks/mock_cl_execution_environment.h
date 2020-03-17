/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/execution_environment/cl_execution_environment.h"

namespace NEO {
class MockClExecutionEnvironment : public ClExecutionEnvironment {
  public:
    using ClExecutionEnvironment::asyncEventsHandler;
    using ClExecutionEnvironment::ClExecutionEnvironment;
};
} // namespace NEO