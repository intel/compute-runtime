/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/test/unit_test/global_environment.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"

namespace L0 {
namespace ult {
template <bool useImagesBuiltins>
ze_result_t MockDeviceForSpv<useImagesBuiltins>::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                                              ze_module_build_log_handle_t *buildLog, ModuleType type) {
    if (!wasModuleCreated) {

        std::string kernelName;
        retrieveBinaryKernelFilename(kernelName, (useImagesBuiltins ? KernelBinaryHelper::BUILT_INS_WITH_IMAGES : KernelBinaryHelper::BUILT_INS) + "_", ".gen");

        size_t size = 0;
        auto src = loadDataFromFile(
            kernelName.c_str(),
            size);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        mockModulePtr.reset(Module::create(this, &moduleDesc, moduleBuildLog, ModuleType::Builtin));
        wasModuleCreated = true;
    }
    *module = mockModulePtr.get();
    return ZE_RESULT_SUCCESS;
}
template class MockDeviceForSpv<true>;
template class MockDeviceForSpv<false>;
}; // namespace ult
} // namespace L0