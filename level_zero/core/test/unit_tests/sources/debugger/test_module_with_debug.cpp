/*
 * Copyright (C) 2019-2021 Intel Corporation
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
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_l0_debugger.h"
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

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(deviceL0, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, L0::BuildOptions::debugKernelEnable));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::generateDebugInfo));
    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, L0::BuildOptions::optDisable));
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

    auto module = std::make_unique<WhiteBox<::L0::Module>>(deviceL0, moduleBuildLog, ModuleType::User);
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

    auto module = std::make_unique<WhiteBox<::L0::Module>>(deviceL0, moduleBuildLog, ModuleType::User);
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

using KernelDebugSurfaceTest = Test<ModuleFixture>;

HWTEST_F(KernelDebugSurfaceTest, givenDebuggerAndBindfulKernelWhenAppendingKernelToCommandListThenBindfulSurfaceStateForDebugSurfaceIsProgrammed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    auto debugSurface = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), true,
         NEO::SipKernel::maxDbgSurfaceSize,
         NEO::GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA,
         false,
         false,
         device->getNEODevice()->getDeviceBitfield()});
    static_cast<L0::DeviceImp *>(device)->setDebugSurface(debugSurface);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> module = std::make_unique<MockModule>(device,
                                                                      moduleBuildLog,
                                                                      ModuleType::User);

    module->debugEnabled = true;

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::Kernel> kernel;
    kernel.module = module.get();
    kernel.immutableData.kernelInfo = &kernelInfo;

    ze_kernel_desc_t desc = {};

    kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = sizeof(RENDER_SURFACE_STATE);
    kernel.immutableData.surfaceStateHeapSize = 2 * sizeof(RENDER_SURFACE_STATE);
    kernel.immutableData.surfaceStateHeapTemplate.reset(new uint8_t[2 * sizeof(RENDER_SURFACE_STATE)]);
    module->kernelImmData = &kernel.immutableData;

    kernel.initialize(&desc);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(kernel.surfaceStateHeapData.get());
    debugSurfaceState = ptrOffset(debugSurfaceState, sizeof(RENDER_SURFACE_STATE));

    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.SurfaceState.Depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, debugSurfaceState->getHeight());
    EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, debugSurfaceState->getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, debugSurfaceState->getCoherencyType());
}

} // namespace ult
} // namespace L0
