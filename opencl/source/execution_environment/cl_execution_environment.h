/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include <mutex>
#include <utility>
#include <vector>

namespace NEO {

class AsyncEventsHandler;
namespace BuiltIn {
class DispatchInfoBuilder;
}

class ClExecutionEnvironment : public ExecutionEnvironment {
  public:
    ClExecutionEnvironment();
    AsyncEventsHandler *getAsyncEventsHandler() const;
    ~ClExecutionEnvironment() override;
    void prepareRootDeviceEnvironments(uint32_t numRootDevices) override;

  protected:
    std::unique_ptr<AsyncEventsHandler> asyncEventsHandler;
};
} // namespace NEO
