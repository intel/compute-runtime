/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/compiler_interface/linker_mock.h"

#include "test.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
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

using ModuleTestSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(ModuleTest, givenNonPatchedTokenThenSurfaceBaseAddressIsCorrectlySet, ModuleTestSupport) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.version = ZE_KERNEL_DESC_VERSION_CURRENT;
    kernelDesc.flags = ZE_KERNEL_FLAG_NONE;
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    res = device->getDriverHandle()->allocDeviceMem(device->toHandle(),
                                                    ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT,
                                                    16384u,
                                                    0u,
                                                    &devicePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    kernelImp->setBufferSurfaceState(argIndex, devicePtr, gpuAlloc);

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(reinterpret_cast<void *>(surfaceStateAddress->getSurfaceBaseAddress()), devicePtr);

    Kernel::fromHandle(kernelHandle)->destroy();

    device->getDriverHandle()->freeMem(devicePtr);
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

        mockCompiler = new MockCompilerInterfaceWithSpecConstants(moduleNumSpecConstants);
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
    MockCompilerInterfaceWithSpecConstants *mockCompiler;
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

using ModuleLinkingTest = Test<DeviceFixture>;

HWTEST_F(ModuleLinkingTest, givenFailureDuringLinkingWhenCreatingModuleThenModuleInitialiationFails) {
    auto mockCompiler = new MockCompilerInterface();
    auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

    auto mockTranslationUnit = new MockModuleTranslationUnit(device);

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->valid = false;

    mockTranslationUnit->programInfo.linkerInput = std::move(linkerInput);
    uint8_t spirvData{};

    ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = &spirvData;
    moduleDesc.inputSize = sizeof(spirvData);

    Module module(device, nullptr);
    module.translationUnit.reset(mockTranslationUnit);

    bool success = module.initialize(&moduleDesc, neoDevice);
    EXPECT_FALSE(success);
}

HWTEST_F(ModuleLinkingTest, givenRemainingUnresolvedSymbolsDuringLinkingWhenCreatingModuleThenModuleIsNotLinkedFully) {
    auto mockCompiler = new MockCompilerInterface();
    auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

    auto mockTranslationUnit = new MockModuleTranslationUnit(device);

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();

    NEO::LinkerInput::RelocationInfo relocation;
    relocation.symbolName = "unresolved";
    linkerInput->dataRelocations.push_back(relocation);
    linkerInput->traits.requiresPatchingOfGlobalVariablesBuffer = true;

    mockTranslationUnit->programInfo.linkerInput = std::move(linkerInput);
    uint8_t spirvData{};

    ze_module_desc_t moduleDesc = {ZE_MODULE_DESC_VERSION_CURRENT};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = &spirvData;
    moduleDesc.inputSize = sizeof(spirvData);

    Module module(device, nullptr);
    module.translationUnit.reset(mockTranslationUnit);

    bool success = module.initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(success);
    EXPECT_FALSE(module.isFullyLinked);
}
HWTEST_F(ModuleLinkingTest, givenNotFullyLinkedModuleWhenCreatingKernelThenErrorIsReturned) {
    Module module(device, nullptr);
    module.isFullyLinked = false;

    auto retVal = module.createKernel(nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, retVal);
}
using ModuleDynamicLinkTests = Test<ModuleFixture>;

HWTEST_F(ModuleDynamicLinkTests, givenCallToDynamicLinkThenUnsupportedFeatureIsReturned) {
    auto module0 = new Module(device, nullptr);
    auto module1 = new Module(device, nullptr);

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);

    delete module0;
    delete module1;
}

class MultiDeviceModuleSetArgBufferTest : public MultiDeviceModuleFixture, public ::testing::Test {
  public:
    void SetUp() override {
        MultiDeviceModuleFixture::SetUp();
    }

    void TearDown() override {
        MultiDeviceModuleFixture::TearDown();
    }

    void createKernelAndAllocMemory(uint32_t rootDeviceIndex, void **ptr, ze_kernel_handle_t *kernelHandle) {
        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.version = ZE_KERNEL_DESC_VERSION_CURRENT;
        kernelDesc.flags = ZE_KERNEL_FLAG_NONE;
        kernelDesc.pKernelName = kernelName.c_str();
        ze_result_t res = modules[rootDeviceIndex].get()->createKernel(&kernelDesc, kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        res = driverHandle->allocHostMem(ZE_HOST_MEM_ALLOC_FLAG_DEFAULT, 4096u, rootDeviceIndex, ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }
};

HWTEST_F(MultiDeviceModuleSetArgBufferTest,
         givenCallsToSetArgBufferThenAllocationIsSetForCorrectDevice) {

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        createModuleFromBinary(rootDeviceIndex);

        ze_kernel_handle_t kernelHandle;
        void *ptr = nullptr;
        createKernelAndAllocMemory(rootDeviceIndex, &ptr, &kernelHandle);

        L0::KernelImp *kernel = reinterpret_cast<L0::KernelImp *>(Kernel::fromHandle(kernelHandle));
        kernel->setArgBuffer(0, sizeof(ptr), &ptr);

        for (auto alloc : kernel->getResidencyContainer()) {
            if (alloc && alloc->getGpuAddress() == reinterpret_cast<uint64_t>(ptr)) {
                EXPECT_EQ(rootDeviceIndex, alloc->getRootDeviceIndex());
            }
        }

        driverHandle->freeMem(ptr);
        Kernel::fromHandle(kernelHandle)->destroy();
    }
}

} // namespace ult
} // namespace L0
