/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"

namespace NEO {

const char *getUnknownBuiltinAsString(EBuiltInOps::Type builtin) {
    return "unknown";
}

BuiltinDispatchInfoBuilder &BuiltIns::getUnknownDispatchInfoBuilder(EBuiltInOps::Type operation, Context &context, Device &device) {
    throw std::runtime_error("getBuiltinDispatchInfoBuilder failed");
}
} // namespace NEO
