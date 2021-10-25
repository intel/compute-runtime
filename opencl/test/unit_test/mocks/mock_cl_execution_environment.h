/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"

namespace NEO {
class MockClExecutionEnvironment : public ClExecutionEnvironment {
  public:
    using ClExecutionEnvironment::asyncEventsHandler;
    using ClExecutionEnvironment::builtinOpsBuilders;
    using ClExecutionEnvironment::ClExecutionEnvironment;
    std::unique_ptr<BuiltinDispatchInfoBuilder> setBuiltinDispatchInfoBuilder(uint32_t rootDeviceIndex, EBuiltInOps::Type operation, std::unique_ptr<BuiltinDispatchInfoBuilder> builder) {
        uint32_t operationId = static_cast<uint32_t>(operation);
        auto &operationBuilder = peekBuilders(rootDeviceIndex)[operationId];
        std::call_once(operationBuilder.second, [] {});
        operationBuilder.first.swap(builder);
        return builder;
    }
};
} // namespace NEO
