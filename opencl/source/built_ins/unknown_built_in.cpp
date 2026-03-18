/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"

namespace NEO {

BuiltIn::DispatchInfoBuilder &BuiltIn::DispatchBuilderOp::getUnknownDispatchInfoBuilder(BuiltIn::Group operation, ClDevice &device) {
    throw std::runtime_error("getBuiltinDispatchInfoBuilder failed");
}
} // namespace NEO
