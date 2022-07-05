/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/kernel_info_from_patchtokens.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
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

    constexpr size_t mockVIsaSize = 0x10;
    char mockVIsa[mockVIsaSize]{0};

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();
    kernelInfo->kernelDescriptor.external.debugData->vIsa = mockVIsa;
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = mockVIsaSize;
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

using ModuleWithZebinAndSLDTest = Test<ModuleWithZebinFixture>;
TEST_F(ModuleWithZebinAndSLDTest, GivenZebinThenCreateDebugZebinAndPassToSLD) {
    module->addEmptyZebin();
    module->addSegments();

    auto debugger = new MockActiveSourceLevelDebugger(new MockOsLibrary);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
    module->passDebugData();

    EXPECT_TRUE(module->translationUnit->debugData);
}

using KernelDebugSurfaceTest = Test<ModuleFixture>;

HWTEST_F(KernelDebugSurfaceTest, givenDebuggerAndBindfulKernelWhenAppendingKernelToCommandListThenBindfulSurfaceStateForDebugSurfaceIsProgrammed) {
    NEO::MockCompilerEnableGuard mock(true);
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
    auto &hwInfo = *NEO::defaultHwInfo.get();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto maxDbgSurfaceSize = hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
    auto debugSurface = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), true,
         maxDbgSurfaceSize,
         NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA,
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

using KernelInitializeTest = Test<L0DebuggerHwFixture>;

TEST_F(KernelInitializeTest, givenDebuggingEnabledWhenKernelsAreInitializedThenAllocationsAreNotResidentAndNotCopied) {
    uint32_t kernelHeap = 0xDEAD;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 4;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);

    memoryOperationsHandler->makeResidentCalledCount = 0;
    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    auto isa = kernelImmutableData.getIsaGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_NE(0, memcmp(isa, &kernelHeap, sizeof(kernelHeap)));
};

HWTEST_F(ModuleWithDebuggerL0Test, GivenDebugDataWithRelocationsWhenInitializingModuleThenRegisterElfWithRelocatedElfAndModuleCreateNotified) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

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

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);

    EXPECT_NE(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
    EXPECT_EQ(reinterpret_cast<char *>(kernelInfo->kernelDescriptor.external.relocatedDebugData.get()), getMockDebuggerL0Hw<FamilyType>()->lastReceivedElf);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenDebugDataWithoutRelocationsWhenInitializingModuleThenRegisterElfWithUnrelocatedElfAndModuleCreateNotified) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

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

    std::vector<uint8_t> data;
    data.resize(4);
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::SHT_PROGBITS;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix, data);
    auto elfBinary = elfEncoder.encode();

    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(elfBinary.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(elfBinary.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);

    EXPECT_EQ(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
    EXPECT_EQ(kernelInfo->kernelDescriptor.external.debugData->vIsa, getMockDebuggerL0Hw<FamilyType>()->lastReceivedElf);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenNoDebugDataWhenInitializingModuleThenDoNotRegisterElfAndDoNotNotifyModuleCreate) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

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

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

using ModuleWithZebinAndL0DebuggerTest = Test<L0DebuggerHwFixture>;

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinDebugDataWhenInitializingModuleThenRegisterElfAndNotifyModuleCreate) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->kernelImmDatas.push_back(std::move(kernelImmutableData));

    kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    moduleMock->kernelImmDatas.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));
    EXPECT_EQ(2u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinNoDebugDataWhenInitializingModuleThenDoNotRegisterElfAndDoNotNotifyModuleCreate) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinWhenModuleIsInitializedAndDestroyedThenModuleHandleIsAttachedAndRemoved) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->kernelImmDatas.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());

    getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn = 6;
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));

    auto expectedSegmentAllocationCount = 1u;
    expectedSegmentAllocationCount += moduleMock->translationUnit->globalConstBuffer != nullptr ? 1 : 0;
    expectedSegmentAllocationCount += moduleMock->translationUnit->globalVarBuffer != nullptr ? 1 : 0;

    EXPECT_EQ(expectedSegmentAllocationCount, getMockDebuggerL0Hw<FamilyType>()->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn, moduleMock->debugModuleHandle);

    moduleMock->destroy();
    moduleMock.release();

    EXPECT_EQ(6u, getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenModuleDebugHandleZeroWhenInitializingAndDestoryingModuleThenHandleIsNotPassedToDebugger) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->kernelImmDatas.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());

    getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn = 0u;
    EXPECT_TRUE(moduleMock->initialize(&moduleDesc, neoDevice));

    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn, moduleMock->debugModuleHandle);

    getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle = std::numeric_limits<uint32_t>::max();

    moduleMock->destroy();
    moduleMock.release();

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle);
}

using NotifyModuleLoadTest = Test<ModuleFixture>;

HWTEST_F(NotifyModuleLoadTest, givenDebuggingEnabledWhenModuleIsCreatedAndFullyLinkedThenIsaAllocationsAreCopiedAndResidentOnlyForUserModules) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    ModuleBuildLog *moduleBuildLog = nullptr;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(nullptr);

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());

    memoryOperationsHandler->makeResidentCalledCount = 0;

    module->initialize(&moduleDesc, neoDevice);

    auto numIsaAllocations = static_cast<int>(module->getKernelImmutableDataVector().size());

    auto expectedMakeResidentCallsCount = numIsaAllocations + 1; // const surface
    if (module->getTranslationUnit()->programInfo.linkerInput) {
        expectedMakeResidentCallsCount += numIsaAllocations;
    }

    EXPECT_EQ(expectedMakeResidentCallsCount, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_TRUE(ki->isIsaCopiedToAllocation());
    }

    auto moduleBuiltin = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::Builtin));
    ASSERT_NE(nullptr, moduleBuiltin.get());

    memoryOperationsHandler->makeResidentCalledCount = 0;

    moduleBuiltin->initialize(&moduleDesc, neoDevice);

    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : moduleBuiltin->getKernelImmutableDataVector()) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }
}

HWTEST_F(NotifyModuleLoadTest, givenDebuggingEnabledWhenModuleWithUnresolvedSymbolsIsCreatedThenIsaAllocationsAreNotCopiedAndNotResident) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<Module>(new Module(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->dataRelocations.push_back(unresolvedRelocation);
    linkerInput->traits.requiresPatchingOfGlobalVariablesBuffer = true;

    module->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    memoryOperationsHandler->makeResidentCalledCount = 0;

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(nullptr);
    module->initialize(&moduleDesc, neoDevice);

    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }

    EXPECT_FALSE(module->isFullyLinked);
}

HWTEST_F(NotifyModuleLoadTest, givenDebuggingEnabledWhenModuleWithUnresolvedSymbolsIsDynamicallyLinkedThenIsaAllocationsAreCopiedAndMadeResident) {
    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    std::string testFile;
    retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<Module>(new Module(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());

    constexpr uint64_t gpuAddress = 0x12345;
    constexpr uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->textRelocations.push_back({unresolvedRelocation});

    module->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;

    memoryOperationsHandler->makeResidentCalledCount = 0;
    module->initialize(&moduleDesc, neoDevice);

    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }

    NEO::SymbolInfo symbolInfo{};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    auto module1 = std::make_unique<Module>(device, nullptr, ModuleType::User);

    module1->symbols[unresolvedRelocation.symbolName] = relocatedSymbol;

    std::vector<ze_module_handle_t> hModules = {module->toHandle(), module1->toHandle()};
    ze_result_t res = module->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_TRUE(module->isFullyLinked);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_TRUE(ki->isIsaCopiedToAllocation());
    }

    auto numIsaAllocations = module->getKernelImmutableDataVector().size();
    EXPECT_NE(0u, numIsaAllocations);
    auto expectedMakeResidentCallsCount = static_cast<int>(numIsaAllocations);
    EXPECT_EQ(expectedMakeResidentCallsCount, memoryOperationsHandler->makeResidentCalledCount);
}
} // namespace ult
} // namespace L0
