/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/helpers/ult_hw_config.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"

namespace L0 {

std::unique_ptr<BuiltinFunctionsLib> BuiltinFunctionsLib::create(Device *device,
                                                                 NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltinFunctionsLib>(new ult::MockBuiltinFunctionsLibImpl(device, builtins));
}

bool BuiltinFunctionsLibImpl::initBuiltinsAsyncEnabled(Device *device) {
    return NEO::ultHwConfig.useinitBuiltinsAsyncEnabled;
}

} // namespace L0
