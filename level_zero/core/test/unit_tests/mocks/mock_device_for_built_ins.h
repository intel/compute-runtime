/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
struct Device;

namespace ult {

struct MockDeviceForBuiltinTests : public MockDeviceImp {

    using MockDeviceImp::MockDeviceImp;
    struct MockModuleForBuiltinTests : public ModuleImp {
        MockModuleForBuiltinTests(Device *device, ModuleType type);
        ze_result_t createKernel(const ze_kernel_desc_t *desc, ze_kernel_handle_t *kernelHandle) override;
    };

    ze_result_t createModule(const ze_module_desc_t *desc,
                             ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog, ModuleType type) override;
    ze_module_format_t formatForModule{};
    bool createModuleCalled = false;
    ModuleType typeCreated = ModuleType::user;
};
} // namespace ult
} // namespace L0
