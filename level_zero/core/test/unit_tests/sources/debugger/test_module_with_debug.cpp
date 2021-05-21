/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_elf.h"

#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/kernel_info_from_patchtokens.h"
#include "test.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_l0_debugger.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include "active_debugger_fixture.h"

namespace L0 {
namespace ult {
using DeviceWithDebuggerEnabledTest = Test<ActiveDebuggerFixture>;
TEST_F(DeviceWithDebuggerEnabledTest, givenDebuggingEnabledWhenModuleIsCreatedThenDebugOptionsAreUsed) {
    NEO::MockCompilerEnableGuard mock(true);
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

TEST_F(DeviceWithDebuggerEnabledTest, GivenDebugVarDebuggerOptDisableZeroWhenOptDisableIsTrueFromDebuggerThenOptDisableIsNotAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(0);

    NEO::MockCompilerEnableGuard mock(true);
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

    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, L0::BuildOptions::optDisable));
};

TEST_F(DeviceWithDebuggerEnabledTest, GivenDebugVarDebuggerOptDisableOneWhenOptDisableIsFalseFromDebuggerThenOptDisableIsAdded) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.DebuggerOptDisable.set(1);

    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    debugger->isOptDisabled = false;

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(deviceL0, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device);

    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, L0::BuildOptions::optDisable));
};

TEST_F(DeviceWithDebuggerEnabledTest, GivenDebuggeableKernelWhenModuleIsInitializedThenDebugEnabledIsTrue) {
    NEO::MockCompilerEnableGuard mock(true);
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
    NEO::MockCompilerEnableGuard mock(true);
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

using ModuleWithSLDTest = Test<ModuleFixture>;

TEST_F(ModuleWithSLDTest, GivenNoDebugDataWhenInitializingModuleThenRelocatedDebugDataIsNotCreated) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> module = std::make_unique<MockModule>(device,
                                                                      moduleBuildLog,
                                                                      ModuleType::User);
    module->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::Kernel> kernel;
    kernel.module = module.get();
    kernel.immutableData.kernelInfo = kernelInfo;

    kernel.immutableData.surfaceStateHeapSize = 64;
    kernel.immutableData.surfaceStateHeapTemplate.reset(new uint8_t[64]);
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    module->kernelImmData = &kernel.immutableData;
    module->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    EXPECT_EQ(nullptr, module->translationUnit->debugData.get());
    auto result = module->initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(result);

    EXPECT_EQ(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData);
}

TEST_F(ModuleWithSLDTest, GivenDebugDataWithSingleRelocationWhenInitializingModuleThenRelocatedDebugDataIsNotCreated) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    createKernel();

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::Kernel> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;

    kernelMock.immutableData.surfaceStateHeapSize = 64;
    kernelMock.immutableData.surfaceStateHeapTemplate.reset(new uint8_t[64]);
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->kernelImmData = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();
    kernelInfo->kernelDescriptor.external.debugData->vIsa = kernel->getKernelDescriptor().external.debugData->vIsa;
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = kernel->getKernelDescriptor().external.debugData->vIsaSize;
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    auto result = moduleMock->initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(result);

    EXPECT_EQ(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData);
}

TEST_F(ModuleWithSLDTest, GivenDebugDataWithMultipleRelocationsWhenInitializingModuleThenRelocatedDebugDataIsCreated) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    auto debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::Kernel> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->kernelImmData = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    auto debugData = MockElfEncoder<>::createRelocateableDebugDataElf();
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(debugData.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(debugData.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData);

    auto result = moduleMock->initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(result);

    EXPECT_NE(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData);
}

using KernelDebugSurfaceTest = Test<ModuleFixture>;

HWTEST_F(KernelDebugSurfaceTest, givenDebuggerAndBindfulKernelWhenAppendingKernelToCommandListThenBindfulSurfaceStateForDebugSurfaceIsProgrammed) {
    NEO::MockCompilerEnableGuard mock(true);
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
}

using ModuleWithDebuggerL0Test = Test<L0DebuggerHwFixture>;
TEST_F(ModuleWithDebuggerL0Test, givenDebuggingEnabledWhenModuleIsCreatedThenDebugOptionsAreNotUsed) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, neoDevice);

    EXPECT_TRUE(module->isDebugEnabled());

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, L0::BuildOptions::debugKernelEnable));
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::generateDebugInfo));
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, L0::BuildOptions::optDisable));
}

TEST_F(ModuleWithDebuggerL0Test, givenDebuggingEnabledWhenKernelsAreInitializedThenAllocationsAreBound) {
    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    EXPECT_EQ(1, memoryOperationsHandler->makeResidentCalledCount);
};

} // namespace ult
} // namespace L0
