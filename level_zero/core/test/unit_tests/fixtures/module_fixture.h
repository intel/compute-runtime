/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/test/unit_test/helpers/test_files.h"

#include "level_zero/core/source/module/module.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

struct ModuleFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        createModuleFromBinary();
    }

    void createModuleFromBinary() {
        std::string testFile;
        retrieveBinaryKernelFilename(testFile, binaryFilename + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(
            testFile.c_str(),
            size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        module.reset(Module::create(device, &moduleDesc, neoDevice, moduleBuildLog));
    }

    void createKernel() {
        ze_kernel_desc_t desc = {ZE_KERNEL_DESC_VERSION_CURRENT};
        desc.pKernelName = kernelName.c_str();

        kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
        kernel->module = module.get();
        kernel->initialize(&desc);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
};

} // namespace ult
} // namespace L0
