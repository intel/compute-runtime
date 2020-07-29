/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"

#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/kernel_info_from_patchtokens.h"
#include "test.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "active_debugger_fixture.h"

namespace L0 {
namespace ult {
using DeviceWithDebuggerEnabledTest = Test<ActiveDebuggerFixture>;
TEST_F(DeviceWithDebuggerEnabledTest, givenDebuggingEnabledWhenModuleIsCreatedThenDebugOptionsAreUsed) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    debugger->isOptDisabled = true;

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(deviceL0, moduleBuildLog));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, NEO::CompilerOptions::debugKernelEnable));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::generateDebugInfo));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::optDisable));
};

TEST_F(DeviceWithDebuggerEnabledTest, GivenDebuggeableKernelWhenModuleIsInitializedThenDebugEnabledIsTrue) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::make_unique<WhiteBox<::L0::Module>>(deviceL0, moduleBuildLog);
    ASSERT_NE(nullptr, module.get());

    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    iOpenCL::SPatchAllocateSystemThreadSurface systemThreadSurface = {};
    systemThreadSurface.Offset = 2;
    systemThreadSurface.PerThreadSystemThreadSurfaceSize = 3;
    kernelTokens.tokens.allocateSystemThreadSurface = &systemThreadSurface;

    auto kernelInfo = std::make_unique<KernelInfo>();
    populateKernelInfo(*kernelInfo, kernelTokens, sizeof(size_t));

    module->translationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());
    module->initialize(&moduleDesc, device);

    EXPECT_TRUE(module->isDebugEnabled());
}

TEST_F(DeviceWithDebuggerEnabledTest, GivenNonDebuggeableKernelWhenModuleIsInitializedThenDebugEnabledIsFalse) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::make_unique<WhiteBox<::L0::Module>>(deviceL0, moduleBuildLog);
    ASSERT_NE(nullptr, module.get());

    NEO::PatchTokenBinary::KernelFromPatchtokens kernelTokens;
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader;
    kernelTokens.header = &kernelHeader;
    kernelTokens.tokens.allocateSystemThreadSurface = nullptr;

    auto kernelInfo = std::make_unique<KernelInfo>();
    populateKernelInfo(*kernelInfo, kernelTokens, sizeof(size_t));

    module->translationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());
    module->initialize(&moduleDesc, device);

    EXPECT_FALSE(module->isDebugEnabled());
}

} // namespace ult
} // namespace L0
