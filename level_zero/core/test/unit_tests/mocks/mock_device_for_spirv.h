/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

template <bool useImagesBuiltins, bool isStateless>
class MockDeviceForSpv : public Mock<DeviceImp> {
  protected:
    bool wasModuleCreated = false;
    bool useImagesBuiltins_prev = false;
    bool isStateless_prev = false;
    std::unique_ptr<L0::Module> mockModulePtr;

  public:
    MockDeviceForSpv(NEO::Device *device, NEO::ExecutionEnvironment *ex, L0::DriverHandleImp *driverHandle) : Mock<DeviceImp>(device, ex) {
        this->driverHandle = driverHandle;
        wasModuleCreated = false;
    }
    ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog, ModuleType type) override;
    ~MockDeviceForSpv() override {
    }
};

} // namespace ult
} // namespace L0
