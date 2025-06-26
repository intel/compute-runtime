/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace NEO {
class Device;
} // namespace NEO

namespace L0 {
struct DriverHandleImp;

namespace ult {

template <bool useImagesBuiltins, bool isStateless>
class MockDeviceForSpv : public MockDeviceImp {
  protected:
    bool wasModuleCreated = false;
    bool useImagesBuiltinsPrev = false;
    bool isStatelessPrev = false;
    std::unique_ptr<L0::Module> mockModulePtr;

  public:
    MockDeviceForSpv(NEO::Device *device, L0::DriverHandleImp *driverHandle) : MockDeviceImp(device) {
        this->driverHandle = driverHandle;
        wasModuleCreated = false;
    }
    ze_result_t createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                             ze_module_build_log_handle_t *buildLog, ModuleType type) override;
    ~MockDeviceForSpv() override {
        builtins.reset(nullptr);
    }
};

} // namespace ult
} // namespace L0
