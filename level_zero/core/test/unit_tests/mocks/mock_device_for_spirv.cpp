/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_device_for_spirv.h"

#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/global_environment.h"

#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {
namespace ult {
template <bool useImagesBuiltins, bool isStateless>
ze_result_t MockDeviceForSpv<useImagesBuiltins, isStateless>::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                                                           ze_module_build_log_handle_t *buildLog, ModuleType type) {
    const std::string builtinCopyfill("builtin_copyfill");
    const std::string builtinImages("builtin_images");
    if ((wasModuleCreated) && ((useImagesBuiltins != useImagesBuiltins_prev) || (isStateless != isStateless_prev)))
        wasModuleCreated = false;

    if (!wasModuleCreated) {

        std::string kernelName;

        retrieveBinaryKernelFilename(kernelName, (useImagesBuiltins ? builtinImages : builtinCopyfill) + (isStateless ? "_stateless_" : "_"), ".gen");

        size_t size = 0;
        auto src = loadDataFromFile(
            kernelName.c_str(),
            size);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;
        ze_result_t result = ZE_RESULT_SUCCESS;
        mockModulePtr.reset(Module::create(this, &moduleDesc, moduleBuildLog, ModuleType::Builtin, &result));
        wasModuleCreated = true;
        useImagesBuiltins_prev = useImagesBuiltins;
        isStateless_prev = isStateless;
    }
    *module = mockModulePtr.get();
    return ZE_RESULT_SUCCESS;
}
template class MockDeviceForSpv<true, false>;
template class MockDeviceForSpv<false, false>;
template class MockDeviceForSpv<false, true>;
}; // namespace ult
} // namespace L0