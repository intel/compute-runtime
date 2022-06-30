/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

namespace L0 {

std::unique_ptr<BuiltinFunctionsLib> BuiltinFunctionsLib::create(Device *device,
                                                                 NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltinFunctionsLib>(new BuiltinFunctionsLibImpl(device, builtins));
}

} // namespace L0
