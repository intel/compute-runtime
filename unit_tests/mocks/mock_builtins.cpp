/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_builtins.h"

namespace NEO {

std::unique_ptr<BuiltinDispatchInfoBuilder> MockBuiltins::setBuiltinDispatchInfoBuilder(EBuiltInOps::Type operation, Context &context, Device &device, std::unique_ptr<BuiltinDispatchInfoBuilder> builder) {
    uint32_t operationId = static_cast<uint32_t>(operation);
    auto &operationBuilder = BuiltinOpsBuilders[operationId];
    std::call_once(operationBuilder.second, [] {});
    operationBuilder.first.swap(builder);
    return builder;
}

} // namespace NEO
