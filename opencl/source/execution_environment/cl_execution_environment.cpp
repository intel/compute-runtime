/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/execution_environment/cl_execution_environment.h"

#include "opencl/source/event/async_events_handler.h"

namespace NEO {

ClExecutionEnvironment::ClExecutionEnvironment() : ExecutionEnvironment() {
    asyncEventsHandler.reset(new AsyncEventsHandler());
}

AsyncEventsHandler *ClExecutionEnvironment::getAsyncEventsHandler() const {
    return asyncEventsHandler.get();
}

ClExecutionEnvironment::~ClExecutionEnvironment() {
    asyncEventsHandler->closeThread();
};
} // namespace NEO
