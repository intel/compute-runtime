/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/source/module/module_imp.h"

#include "active_debugger_fixture.h"

namespace L0 {
namespace ult {
using DeviceWithDebuggerEnabledTest = Test<ActiveDebuggerFixture>;
TEST_F(DeviceWithDebuggerEnabledTest, givenDebuggingEnabledWhenModuleIsCreatedThenDebugOptionsAreUsed) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    debugger->isOptDisabled = true;

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(deviceL0, device, moduleBuildLog));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, NEO::CompilerOptions::debugKernelEnable));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::generateDebugInfo));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::optDisable));
};

} // namespace ult
} // namespace L0
