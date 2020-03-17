/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

namespace NEO {

class AsyncEventsHandler;

class ClExecutionEnvironment : public ExecutionEnvironment {
  public:
    ClExecutionEnvironment();
    AsyncEventsHandler *getAsyncEventsHandler() const;
    ~ClExecutionEnvironment() override;

  protected:
    std::unique_ptr<AsyncEventsHandler> asyncEventsHandler;
};
} // namespace NEO