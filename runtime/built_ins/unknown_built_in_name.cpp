/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"

namespace NEO {

const char *getUnknownBuiltinAsString(EBuiltInOps::Type builtin) {
    return "unknown";
}

BuiltinDispatchInfoBuilder &BuiltInDispatchBuilderOp::getUnknownDispatchInfoBuilder(EBuiltInOps::Type operation, Device &device) {
    throw std::runtime_error("getBuiltinDispatchInfoBuilder failed");
}
} // namespace NEO
