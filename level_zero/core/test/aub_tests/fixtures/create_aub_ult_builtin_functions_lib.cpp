/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"

namespace L0 {

std::unique_ptr<BuiltInKernelLib> BuiltInKernelLib::create(Device *device,
                                                           NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltInKernelLib>(new BuiltInKernelLibImpl(device, builtins));
}

bool BuiltInKernelLibImpl::initBuiltinsAsyncEnabled(Device *device) {
    return false;
}

} // namespace L0
