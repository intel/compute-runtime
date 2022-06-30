/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/execution_environment/cl_execution_environment.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
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
void ClExecutionEnvironment::prepareRootDeviceEnvironments(uint32_t numRootDevices) {
    ExecutionEnvironment::prepareRootDeviceEnvironments(numRootDevices);
    builtinOpsBuilders.resize(numRootDevices);
    for (auto i = 0u; i < numRootDevices; i++) {
        builtinOpsBuilders[i] = std::make_unique<BuilderT[]>(EBuiltInOps::COUNT);
    }
}
} // namespace NEO
