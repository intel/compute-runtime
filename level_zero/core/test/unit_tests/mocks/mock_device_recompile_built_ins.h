/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

struct MockDeviceForRebuildBuilins : public Mock<DeviceImp> {

    struct MockModuleForRebuildBuiltins : public ModuleImp {
        MockModuleForRebuildBuiltins(Device *device) : ModuleImp(device, nullptr) {}

        ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                 ze_kernel_handle_t *phFunction) override {

            *phFunction = nullptr;
            return ZE_RESULT_SUCCESS;
        }
    };

    MockDeviceForRebuildBuilins(L0::Device *device) : Mock(device->getNEODevice(), static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment())) {
        driverHandle = device->getDriverHandle();
        builtins = BuiltinFunctionsLib::create(this, neoDevice->getBuiltIns());
    }
    ~MockDeviceForRebuildBuilins() {
    }

    ze_result_t createModule(const ze_module_desc_t *desc,
                             ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog) override {

        createModuleCalled = true;
        *module = new MockModuleForRebuildBuiltins(this);

        return ZE_RESULT_SUCCESS;
    }

    bool createModuleCalled = false;
};
} // namespace ult
} // namespace L0