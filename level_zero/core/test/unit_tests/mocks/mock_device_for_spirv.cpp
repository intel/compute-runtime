/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

namespace L0 {
struct ModuleBuildLog;

namespace ult {
ze_result_t MockDeviceForSpv::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                           ze_module_build_log_handle_t *buildLog, ModuleType type) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    if (!wasModuleCreated) {

        ModuleBuildLog *moduleBuildLog = nullptr;
        mockModulePtr.reset(Module::create(this, desc, moduleBuildLog, type, &result));
        wasModuleCreated = true;
    }
    *module = mockModulePtr.get();
    return result;
}
}; // namespace ult
} // namespace L0
