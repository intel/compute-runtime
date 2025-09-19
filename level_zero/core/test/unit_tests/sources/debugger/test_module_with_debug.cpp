/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
struct ModuleBuildLog;

namespace ult {

using ModuleWithDebuggerL0Test = Test<L0DebuggerHwFixture>;
TEST_F(ModuleWithDebuggerL0Test, givenL0DebuggerWhenModuleIsCreatedThenDebugOptionsAreNotUsed) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::user));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, neoDevice);

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, L0::BuildOptions::debugKernelEnable));
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, NEO::CompilerOptions::generateDebugInfo));
    EXPECT_FALSE(CompilerOptions::contains(cip->buildOptions, L0::BuildOptions::optDisable));
}

using KernelInitializeTest = Test<L0DebuggerHwFixture>;

TEST_F(KernelInitializeTest, givenDebuggingEnabledWhenKernelsAreInitializedThenAllocationsAreNotResidentAndNotCopied) {
    uint32_t kernelHeap = 0xDEAD;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.kernelHeapSize = 4;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    KernelImmutableData kernelImmutableData(device);
    kernelImmutableData.setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo.heapInfo.kernelHeapSize, false));
    auto isa = kernelImmutableData.getIsaGraphicsAllocation()->getUnderlyingBuffer();

    *reinterpret_cast<uint32_t *>(isa) = 0;

    memoryOperationsHandler->makeResidentCalledCount = 0;
    kernelImmutableData.initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    EXPECT_NE(0, memcmp(isa, &kernelHeap, sizeof(kernelHeap)));
};

using ModuleWithDebuggerL0MultiTileTest = Test<SingleRootMultiSubDeviceFixture>;

HWTEST_F(ModuleWithDebuggerL0MultiTileTest, GivenSubDeviceWhenCreatingModuleThenModuleCreateIsNotifiedWithCorrectDevice) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);
    neoDevice->getExecutionEnvironment()->setDebuggingMode(NEO::DebuggingMode::online);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->initDebuggerL0(neoDevice);
    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(subDevice0, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(subDevice0);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    auto debugData = MockElfEncoder<>::createRelocateableDebugDataElf();
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(debugData.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(debugData.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, subDevice0->getNEODevice()), ZE_RESULT_SUCCESS);

    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    EXPECT_EQ(1u, debuggerL0Hw->notifyModuleCreateCount);
    EXPECT_EQ(subDevice0->getNEODevice(), debuggerL0Hw->notifyModuleLoadAllocationsCapturedDevice);

    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_EQ(1, memoryOperationsHandler->makeResidentCalledCount);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenDebugDataWithRelocationsWhenInitializingModuleThenRegisterElfWithRelocatedElfAndModuleCreateNotified) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    auto debugData = MockElfEncoder<>::createRelocateableDebugDataElf();
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(debugData.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(debugData.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->registerElfAndLinkCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);

    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_NE(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
    EXPECT_EQ(reinterpret_cast<char *>(kernelInfo->kernelDescriptor.external.relocatedDebugData.get()), getMockDebuggerL0Hw<FamilyType>()->lastReceivedElf);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenBuiltinModuleWhenInitializingModuleThenModuleAndElfNOtificationsAreNotCalled) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::builtin);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;
    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();
    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    auto debugData = MockElfEncoder<>::createRelocateableDebugDataElf();
    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(debugData.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(debugData.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfAndLinkCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);

    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_NE(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
    EXPECT_EQ(nullptr, getMockDebuggerL0Hw<FamilyType>()->lastReceivedElf);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenDebugDataWithoutRelocationsWhenInitializingModuleThenRegisterElfWithUnrelocatedElfAndModuleCreateNotified) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    std::vector<uint8_t> data;
    data.resize(4);
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::SHT_PROGBITS;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix, data);
    auto elfBinary = elfEncoder.encode();

    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(elfBinary.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(elfBinary.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->registerElfAndLinkCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);

    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_EQ(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData.get());
    EXPECT_EQ(kernelInfo->kernelDescriptor.external.debugData->vIsa, getMockDebuggerL0Hw<FamilyType>()->lastReceivedElf);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenNoDebugDataWhenInitializingModuleThenDoNotRegisterElfAndNotifyModuleCreate) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

using ModuleWithZebinAndL0DebuggerTest = Test<L0DebuggerHwFixture>;

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinDebugDataWhenInitializingModuleThenRegisterElfAndNotifyModuleCreate) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram<>();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenDumpElfFlagAndZebinWhenInitializingModuleThenDebugElfIsDumpedToFile) {
    USE_REAL_FILE_SYSTEM();
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_ELF);

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram<>();

    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());

    std::string fileName = "dumped_debug_module.elf";
    EXPECT_FALSE(virtualFileExists(fileName));

    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_TRUE(virtualFileExists(fileName));
    removeVirtualFile(fileName.c_str());
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinNoDebugDataWhenInitializingModuleThenDoNotRegisterElfAndDoNotNotifyModuleCreate) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->registerElfCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleCreateCount);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenZebinWhenModuleIsInitializedAndDestroyedThenModuleHandleIsAttachedAndRemoved) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram<>();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());

    getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn = 6;
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);

    auto expectedSegmentAllocationCount = 1u;
    expectedSegmentAllocationCount += moduleMock->translationUnit->globalConstBuffer != nullptr ? 1 : 0;
    expectedSegmentAllocationCount += moduleMock->translationUnit->globalVarBuffer != nullptr ? 1 : 0;

    EXPECT_EQ(expectedSegmentAllocationCount, getMockDebuggerL0Hw<FamilyType>()->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn, moduleMock->debugModuleHandle);

    moduleMock->destroy();
    moduleMock.release();

    EXPECT_EQ(6u, getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleDestroyCount);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenNonZebinBinaryWhenDestroyModuleThenModuleDestroyNotified) {

    auto cip = std::make_unique<NEO::MockCompilerInterfaceCaptureBuildOptions>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface = std::move(cip);

    uint8_t binary[10] = {0};
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    std::vector<uint8_t> data;
    data.resize(4);
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::SHT_PROGBITS;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix, data);
    auto elfBinary = elfEncoder.encode();

    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(elfBinary.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(elfBinary.data());
    kernelInfo->kernelDescriptor.external.debugData->genIsa = nullptr;
    kernelInfo->kernelDescriptor.external.debugData->genIsaSize = 0;

    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    moduleMock->destroy();
    moduleMock.release();
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleDestroyCount);
}

HWTEST_F(ModuleWithDebuggerL0Test, GivenNoDebugDataWhenDestroyingModuleThenNotifyModuleDestroy) {

    auto cip = std::make_unique<NEO::MockCompilerInterfaceCaptureBuildOptions>();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface = std::move(cip);

    uint8_t binary[10] = {0};
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;
    ModuleBuildLog *moduleBuildLog = nullptr;

    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, moduleBuildLog, ModuleType::user);
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    auto mockTranslationUnit = toMockPtr(moduleMock->translationUnit.get());
    mockTranslationUnit->processUnpackedBinaryCallBase = false;

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    Mock<::L0::KernelImp> kernelMock;
    kernelMock.module = moduleMock.get();
    kernelMock.immutableData.kernelInfo = kernelInfo;
    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;

    moduleMock->data = &kernelMock.immutableData;
    moduleMock->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);
    EXPECT_EQ(mockTranslationUnit->processUnpackedBinaryCalled, 1u);
    moduleMock->destroy();
    moduleMock.release();
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->notifyModuleDestroyCount);
}

HWTEST_F(ModuleWithZebinAndL0DebuggerTest, GivenModuleDebugHandleZeroWhenInitializingAndDestoryingModuleThenHandleIsNotPassedToDebugger) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);
    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    uint32_t kernelHeap = 0;
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.kernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

    auto kernelImmutableData = ::std::make_unique<KernelImmutableData>(device);
    kernelImmutableData->setIsaPerKernelAllocation(this->allocateIsaMemory(kernelInfo->heapInfo.kernelHeapSize, false));
    kernelImmutableData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);
    std::unique_ptr<MockModule> moduleMock = std::make_unique<MockModule>(device, nullptr, ModuleType::user);
    moduleMock->kernelImmData.push_back(std::move(kernelImmutableData));

    auto zebin = ZebinTestData::ValidEmptyProgram<>();
    moduleMock->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);
    moduleMock->translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
    moduleMock->translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
    memcpy_s(moduleMock->translationUnit->unpackedDeviceBinary.get(), moduleMock->translationUnit->unpackedDeviceBinarySize,
             zebin.storage.data(), zebin.storage.size());

    getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn = 0u;
    EXPECT_EQ(moduleMock->initialize(&moduleDesc, neoDevice), ZE_RESULT_SUCCESS);

    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->segmentCountWithAttachedModuleHandle);
    EXPECT_EQ(getMockDebuggerL0Hw<FamilyType>()->moduleHandleToReturn, moduleMock->debugModuleHandle);

    getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle = std::numeric_limits<uint32_t>::max();

    moduleMock->destroy();
    moduleMock.release();

    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), getMockDebuggerL0Hw<FamilyType>()->removedZebinModuleHandle);
}

using NotifyModuleLoadTest = Test<ModuleFixture>;

HWTEST_F(NotifyModuleLoadTest, givenDebuggingEnabledWhenModuleIsCreatedAndFullyLinkedThenIsaAllocationsAreCopiedAndResidentOnlyForUserModules) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    auto elfAdditionalSections = {ZebinTestData::AppendElfAdditionalSection::constant}; // for const surface allocation copy
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), elfAdditionalSections);
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(nullptr);

    auto module = std::make_unique<WhiteBox<::L0::Module>>(device, moduleBuildLog, ModuleType::user);
    ASSERT_NE(nullptr, module.get());

    memoryOperationsHandler->makeResidentCalledCount = 0;

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    module->translationUnit->programInfo.linkerInput = std::move(linkerInput);
    module->initialize(&moduleDesc, neoDevice);

    auto numIsaAllocations = static_cast<int>(module->getKernelImmutableDataVector().size());

    auto expectedMakeResidentCallsCount = numIsaAllocations + 1; // const surface
    expectedMakeResidentCallsCount += numIsaAllocations;

    EXPECT_EQ(expectedMakeResidentCallsCount, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_TRUE(ki->isIsaCopiedToAllocation());
    }

    auto moduleBuiltin = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::builtin));
    ASSERT_NE(nullptr, moduleBuiltin.get());

    memoryOperationsHandler->makeResidentCalledCount = 0;

    moduleBuiltin->initialize(&moduleDesc, neoDevice);

    EXPECT_EQ(0, memoryOperationsHandler->makeResidentCalledCount);

    for (auto &ki : moduleBuiltin->getKernelImmutableDataVector()) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }
}

HWTEST_F(NotifyModuleLoadTest, givenDebuggingEnabledWhenModuleWithUnresolvedSymbolsIsCreatedThenIsaAllocationsAreNotCopiedAndNotResident) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<Module>(new Module(device, moduleBuildLog, ModuleType::user));
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

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto memoryOperationsHandler = new NEO::MockMemoryOperations();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<Module>(new Module(device, moduleBuildLog, ModuleType::user));
    ASSERT_NE(nullptr, module.get());

    constexpr uint64_t gpuAddress = 0x12345;
    constexpr uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::address;
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
    NEO::Linker::RelocatedSymbol<SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    auto module1 = std::make_unique<Module>(device, nullptr, ModuleType::user);

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
