/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

struct MockDeviceForBuiltinTests : public Mock<DeviceImp> {

    struct MockModuleForBuiltinTests : public ModuleImp {
        MockModuleForBuiltinTests(Device *device, ModuleType type) : ModuleImp(device, nullptr, type) {}

        ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                 ze_kernel_handle_t *kernelHandle) override {

            *kernelHandle = new Mock<KernelImp>();
            return ZE_RESULT_SUCCESS;
        }
    };

    MockDeviceForBuiltinTests(NEO::Device *device) : Mock(device, device->getExecutionEnvironment()) {
    }

    ze_result_t createModule(const ze_module_desc_t *desc,
                             ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog, ModuleType type) override {
        if (desc) {
            formatForModule = desc->format;
        }
        createModuleCalled = true;
        typeCreated = type;
        *module = new MockModuleForBuiltinTests(this, type);

        return ZE_RESULT_SUCCESS;
    }
    ze_module_format_t formatForModule{};
    bool createModuleCalled = false;
    ModuleType typeCreated = ModuleType::User;
};
} // namespace ult
} // namespace L0
