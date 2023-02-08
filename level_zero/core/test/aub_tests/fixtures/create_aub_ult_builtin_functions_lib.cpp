/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

namespace L0 {

std::unique_ptr<BuiltinFunctionsLib> BuiltinFunctionsLib::create(Device *device,
                                                                 NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltinFunctionsLib>(new BuiltinFunctionsLibImpl(device, builtins));
}

bool BuiltinFunctionsLibImpl::initBuiltinsAsyncEnabled(Device *device) {
    return false;
}

} // namespace L0
