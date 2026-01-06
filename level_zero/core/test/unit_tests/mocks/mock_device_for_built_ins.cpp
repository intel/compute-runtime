/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_device_for_built_ins.h"

#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
struct Device;

namespace ult {

MockDeviceForBuiltinTests::MockModuleForBuiltinTests::MockModuleForBuiltinTests(Device *device, ModuleType type) : ModuleImp(device, nullptr, type){};

ze_result_t MockDeviceForBuiltinTests::MockModuleForBuiltinTests::createKernel(const ze_kernel_desc_t *desc,
                                                                               ze_kernel_handle_t *kernelHandle) {

    *kernelHandle = new Mock<KernelImp>();
    return ZE_RESULT_SUCCESS;
}

ze_result_t MockDeviceForBuiltinTests::createModule(const ze_module_desc_t *desc,
                                                    ze_module_handle_t *module,
                                                    ze_module_build_log_handle_t *buildLog, ModuleType type) {
    if (desc) {
        formatForModule = desc->format;
    }
    createModuleCalled = true;
    typeCreated = type;
    *module = new MockModuleForBuiltinTests(this, type);

    return ZE_RESULT_SUCCESS;
}

} // namespace ult
} // namespace L0
