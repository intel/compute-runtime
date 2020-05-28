/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using ModuleTest = Test<ModuleFixture>;

HWTEST_F(ModuleTest, givenBinaryWithDebugDataWhenModuleCreatedFromNativeBinaryThenDebugDataIsStored) {
    size_t size = 0;
    std::unique_ptr<uint8_t[]> data;
    auto result = module->getDebugInfo(&size, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    data = std::make_unique<uint8_t[]>(size);
    result = module->getDebugInfo(&size, data.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, data.get());
    EXPECT_NE(0u, size);
}

HWTEST_F(ModuleTest, givenKernelCreateReturnsSuccess) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.version = ZE_KERNEL_DESC_VERSION_CURRENT;
    kernelDesc.flags = ZE_KERNEL_FLAG_NONE;
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    Kernel::fromHandle(kernelHandle)->destroy();
}

HWTEST_F(ModuleTest, givenKernelCreateWithIncorrectKernelNameReturnsFailure) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.version = ZE_KERNEL_DESC_VERSION_CURRENT;
    kernelDesc.flags = ZE_KERNEL_FLAG_NONE;
    kernelDesc.pKernelName = "nonexistent_function";

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

struct ModuleSpecConstantsTests : public DeviceFixture,
                                  public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();

        mockCompiler = new MockCompilerInterface(moduleNumSpecConstants);
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

        mockTranslationUnit = new MockModuleTranslationUnit(device);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    const uint32_t moduleNumSpecConstants = 4;
    ze_module_constants_t specConstants;
    std::vector<uint64_t> specConstantsPointerValues;

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    MockCompilerInterface *mockCompiler;
    MockModuleTranslationUnit *mockTranslationUnit;
};

HWTEST_F(ModuleSpecConstantsTests, givenSpecializationConstantsSetInDescriptorTheModuleCorrectlyPassesThemToTheCompiler) {
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, binaryFilename + "_", ".spv");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
    for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants; i++) {
        specConstantsPointerValues.push_back(reinterpret_cast<uint64_t>(&mockCompiler->moduleSpecConstantsValues[i]));
    }

    specConstants.pConstantIds = mockCompiler->moduleSpecConstantsIds.data();
    specConstants.pConstantValues = specConstantsPointerValues.data();
    moduleDesc.pConstants = &specConstants;

    auto module = new Module(device, nullptr);
    module->translationUnit.reset(mockTranslationUnit);

    bool success = module->initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(success);
    module->destroy();
}

} // namespace ult
} // namespace L0
