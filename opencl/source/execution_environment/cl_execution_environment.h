/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/builtinops/built_in_ops.h"
#include "shared/source/execution_environment/execution_environment.h"

#include <mutex>
#include <utility>
#include <vector>

namespace NEO {

class AsyncEventsHandler;
class BuiltinDispatchInfoBuilder;

class ClExecutionEnvironment : public ExecutionEnvironment {
  public:
    ClExecutionEnvironment();
    AsyncEventsHandler *getAsyncEventsHandler() const;
    ~ClExecutionEnvironment() override;
    void prepareRootDeviceEnvironments(uint32_t numRootDevices) override;
    using BuilderT = std::pair<std::unique_ptr<BuiltinDispatchInfoBuilder>, std::once_flag>;
    BuilderT *peekBuilders(uint32_t rootDeviceIndex) { return builtinOpsBuilders[rootDeviceIndex].get(); }

  protected:
    std::vector<std::unique_ptr<BuilderT[]>> builtinOpsBuilders;
    std::unique_ptr<AsyncEventsHandler> asyncEventsHandler;
};
} // namespace NEO