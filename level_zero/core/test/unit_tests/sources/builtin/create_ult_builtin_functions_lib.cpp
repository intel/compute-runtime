/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/helpers/ult_hw_config.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"

namespace L0 {

std::unique_ptr<BuiltInKernelLib> BuiltInKernelLib::create(Device *device,
                                                           NEO::BuiltIns *builtins) {
    auto lib = std::make_unique<ult::MockBuiltInKernelLibImpl>(device, builtins);
    lib->svmAllocsManagerAtCreation = device->getDriverHandle()->getSvmAllocsManager();
    return lib;
}

bool BuiltInKernelLibImpl::initBuiltinsAsyncEnabled(Device *device) {
    return NEO::ultHwConfig.useinitBuiltinsAsyncEnabled;
}

} // namespace L0
