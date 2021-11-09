/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/debug_zebin.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

#include "test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

using ::testing::Return;

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

HWTEST_F(ModuleTest, WhenCreatingKernelThenSuccessIsReturned) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    Kernel::fromHandle(kernelHandle)->destroy();
}

HWTEST_F(ModuleTest, givenZeroCountWhenGettingKernelNamesThenCountIsFilled) {
    uint32_t count = 0;
    auto result = module->getKernelNames(&count, nullptr);

    auto whiteboxModule = whitebox_cast(module.get());
    EXPECT_EQ(whiteboxModule->kernelImmDatas.size(), count);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(ModuleTest, givenNonZeroCountWhenGettingKernelNamesThenNamesAreReturned) {
    uint32_t count = 1;
    const char *kernelNames = nullptr;
    auto result = module->getKernelNames(&count, &kernelNames);

    EXPECT_EQ(1u, count);
    EXPECT_STREQ(this->kernelName.c_str(), kernelNames);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(ModuleTest, givenUserModuleTypeWhenCreatingModuleThenCorrectTypeIsSet) {
    WhiteBox<Module> module(device, nullptr, ModuleType::User);
    EXPECT_EQ(ModuleType::User, module.type);
}

HWTEST_F(ModuleTest, givenBuiltinModuleTypeWhenCreatingModuleThenCorrectTypeIsSet) {
    WhiteBox<Module> module(device, nullptr, ModuleType::Builtin);
    EXPECT_EQ(ModuleType::Builtin, module.type);
}

HWTEST_F(ModuleTest, givenUserModuleWhenCreatedThenCorrectAllocationTypeIsUsedForIsa) {
    createKernel();
    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, kernel->getIsaAllocation()->getAllocationType());
}

HWTEST_F(ModuleTest, givenBuiltinModuleWhenCreatedThenCorrectAllocationTypeIsUsedForIsa) {
    createModuleFromBinary(ModuleType::Builtin);
    createKernel();
    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::KERNEL_ISA_INTERNAL, kernel->getIsaAllocation()->getAllocationType());
}

using ModuleTestSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(ModuleTest, givenNonPatchedTokenThenSurfaceBaseAddressIsCorrectlySet, ModuleTestSupport) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
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
    EXPECT_EQ(devicePtr, reinterpret_cast<void *>(surfaceStateAddress->getSurfaceBaseAddress()));
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, surfaceStateAddress->getCoherencyType());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST_F(ModuleTest, givenStatefulBufferWhenOffsetIsPatchedThenAllocBaseAddressIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    uint32_t offset = 0x1234;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bufferOffset = 0;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bindful = 0x80;
    kernelImp->setBufferSurfaceState(argIndex, ptrOffset(devicePtr, offset), gpuAlloc);

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(devicePtr, reinterpret_cast<void *>(surfaceStateAddress->getSurfaceBaseAddress()));

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST_F(ModuleTest, givenBufferWhenOffsetIsNotPatchedThenPassedPtrIsSetAsBaseAddress) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    uint32_t offset = 0x1234;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bufferOffset = undefined<NEO::CrossThreadDataOffset>;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bindful = 0x80;

    kernelImp->setBufferSurfaceState(argIndex, ptrOffset(devicePtr, offset), gpuAlloc);

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(ptrOffset(devicePtr, offset), reinterpret_cast<void *>(surfaceStateAddress->getSurfaceBaseAddress()));

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST_F(ModuleTest, givenBufferWhenOffsetIsNotPatchedThenSizeIsDecereasedByOffset) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));
    auto allocSize = 16384u;
    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  allocSize,
                                  0u,
                                  &devicePtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    uint32_t offset = 0x1234;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bufferOffset = undefined<NEO::CrossThreadDataOffset>;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bindful = 0x80;

    kernelImp->setBufferSurfaceState(argIndex, ptrOffset(devicePtr, offset), gpuAlloc);

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    SURFACE_STATE_BUFFER_LENGTH length = {0};
    length.Length = static_cast<uint32_t>((gpuAlloc->getUnderlyingBufferSize() - offset) - 1);
    EXPECT_EQ(surfaceStateAddress->getWidth(), static_cast<uint32_t>(length.SurfaceState.Width + 1));
    EXPECT_EQ(surfaceStateAddress->getHeight(), static_cast<uint32_t>(length.SurfaceState.Height + 1));
    EXPECT_EQ(surfaceStateAddress->getDepth(), static_cast<uint32_t>(length.SurfaceState.Depth + 1));

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST_F(ModuleTest, givenUnalignedHostBufferWhenSurfaceStateProgrammedThenUnalignedSizeIsAdded) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *address = reinterpret_cast<void *>(0x23000);
    MockGraphicsAllocation mockGa;
    mockGa.gpuAddress = 0x23000;
    mockGa.size = 0xc;
    auto alignment = NEO::EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment();
    auto allocationOffset = alignment - 1;
    mockGa.allocationOffset = allocationOffset;

    uint32_t argIndex = 0u;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bufferOffset = undefined<NEO::CrossThreadDataOffset>;
    const_cast<KernelDescriptor *>(&(kernelImp->getImmutableData()->getDescriptor()))->payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>().bindful = 0x80;

    kernelImp->setBufferSurfaceState(argIndex, address, &mockGa);

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    SURFACE_STATE_BUFFER_LENGTH length = {0};
    length.Length = alignUp(static_cast<uint32_t>((mockGa.getUnderlyingBufferSize() + allocationOffset)), alignment) - 1;
    EXPECT_EQ(surfaceStateAddress->getWidth(), static_cast<uint32_t>(length.SurfaceState.Width + 1));
    EXPECT_EQ(surfaceStateAddress->getHeight(), static_cast<uint32_t>(length.SurfaceState.Height + 1));
    EXPECT_EQ(surfaceStateAddress->getDepth(), static_cast<uint32_t>(length.SurfaceState.Depth + 1));

    Kernel::fromHandle(kernelHandle)->destroy();
}

using ModuleUncachedBufferTest = Test<ModuleFixture>;

struct KernelImpUncachedTest : public KernelImp {
    using KernelImp::kernelRequiresUncachedMocsCount;
};

HWTEST2_F(ModuleUncachedBufferTest,
          givenKernelWithNonUncachedArgumentAndPreviouslyNotSetUncachedThenUncachedMocsNotSet, ModuleTestSupport) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    kernelImp->setArgBufferWithAlloc(argIndex, reinterpret_cast<uintptr_t>(devicePtr), gpuAlloc);
    EXPECT_FALSE(kernelImp->getKernelRequiresUncachedMocs());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST2_F(ModuleUncachedBufferTest,
          givenKernelWithNonUncachedArgumentAndPreviouslySetUncachedArgumentThenUncachedMocsNotSet, ModuleTestSupport) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<KernelImpUncachedTest *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    kernelImp->setKernelArgUncached(argIndex, true);
    kernelImp->kernelRequiresUncachedMocsCount++;
    kernelImp->setArgBufferWithAlloc(argIndex, reinterpret_cast<uintptr_t>(devicePtr), gpuAlloc);
    EXPECT_FALSE(kernelImp->getKernelRequiresUncachedMocs());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST2_F(ModuleUncachedBufferTest,
          givenKernelWithUncachedArgumentAndPreviouslyNotSetUncachedArgumentThenUncachedMocsIsSet, ModuleTestSupport) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    kernelImp->setArgBufferWithAlloc(argIndex, reinterpret_cast<uintptr_t>(devicePtr), gpuAlloc);
    EXPECT_TRUE(kernelImp->getKernelRequiresUncachedMocs());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST2_F(ModuleUncachedBufferTest,
          givenKernelWithUncachedArgumentAndPreviouslySetUncachedArgumentThenUncachedMocsIsSet, ModuleTestSupport) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<KernelImpUncachedTest *>(L0::Kernel::fromHandle(kernelHandle));

    void *devicePtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    res = context->allocDeviceMem(device->toHandle(),
                                  &deviceDesc,
                                  16384u,
                                  0u,
                                  &devicePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(devicePtr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gpuAlloc);

    uint32_t argIndex = 0u;
    kernelImp->setKernelArgUncached(argIndex, true);
    kernelImp->kernelRequiresUncachedMocsCount++;
    kernelImp->setArgBufferWithAlloc(argIndex, reinterpret_cast<uintptr_t>(devicePtr), gpuAlloc);
    EXPECT_TRUE(kernelImp->getKernelRequiresUncachedMocs());

    auto argInfo = kernelImp->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
    auto surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    auto surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(devicePtr, reinterpret_cast<void *>(surfaceStateAddress->getSurfaceBaseAddress()));

    auto gmmHelper = device->getNEODevice()->getGmmHelper();
    uint32_t expectedMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);

    EXPECT_EQ(expectedMocs, surfaceStateAddress->getMemoryObjectControlStateReserved());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST_F(ModuleTest, GivenIncorrectNameWhenCreatingKernelThenResultErrorInvalidArgumentErrorIsReturned) {
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "nonexistent_function";

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_NAME, res);
}

template <typename T1, typename T2>
struct ModuleSpecConstantsFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();

        mockCompiler = new MockCompilerInterfaceWithSpecConstants<T1, T2>(moduleNumSpecConstants);
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

        mockTranslationUnit = new MockModuleTranslationUnit(device);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    void runTest() {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

        size_t size = 0;
        auto src = loadDataFromFile(testFile.c_str(), size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
        for (uint32_t i = mockCompiler->moduleNumSpecConstants / 2; i > 0; i--) {
            specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT1[i - 1]);
            specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT2[i - 1]);
        }
        for (uint32_t i = mockCompiler->moduleNumSpecConstants; i > 0; i--) {
            specConstantsPointerIds.push_back(mockCompiler->moduleSpecConstantsIds[i - 1]);
        }
        specConstants.pConstantIds = specConstantsPointerIds.data();
        specConstants.pConstantValues = specConstantsPointerValues.data();
        moduleDesc.pConstants = &specConstants;

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&moduleDesc, neoDevice);
        for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants / 2; i++) {
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT2[i]));
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i + 1]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT1[i]));
        }
        EXPECT_TRUE(success);
        module->destroy();
    }

    void runTestStatic() {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

        size_t sizeModule1, sizeModule2 = 0;
        auto srcModule1 = loadDataFromFile(testFile.c_str(), sizeModule1);
        auto srcModule2 = loadDataFromFile(testFile.c_str(), sizeModule2);

        ASSERT_NE(0u, sizeModule1);
        ASSERT_NE(0u, sizeModule2);
        ASSERT_NE(nullptr, srcModule1);
        ASSERT_NE(nullptr, srcModule2);

        ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
        std::vector<const uint8_t *> inputSpirVs;
        std::vector<size_t> inputSizes;
        std::vector<ze_module_constants_t *> specConstantsArray;
        combinedModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        combinedModuleDesc.pNext = &staticLinkModuleDesc;

        specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
        for (uint32_t i = mockCompiler->moduleNumSpecConstants / 2; i > 0; i--) {
            specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT1[i - 1]);
            specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT2[i - 1]);
        }
        for (uint32_t i = mockCompiler->moduleNumSpecConstants; i > 0; i--) {
            specConstantsPointerIds.push_back(mockCompiler->moduleSpecConstantsIds[i - 1]);
        }
        specConstants.pConstantIds = specConstantsPointerIds.data();
        specConstants.pConstantValues = specConstantsPointerValues.data();
        specConstantsArray.push_back(&specConstants);
        specConstantsArray.push_back(&specConstants);

        inputSizes.push_back(sizeModule1);
        inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule1.get()));
        inputSizes.push_back(sizeModule2);
        inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule2.get()));

        staticLinkModuleDesc.count = 2;
        staticLinkModuleDesc.inputSizes = inputSizes.data();
        staticLinkModuleDesc.pInputModules = inputSpirVs.data();
        staticLinkModuleDesc.pConstants = const_cast<const ze_module_constants_t **>(specConstantsArray.data());

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&combinedModuleDesc, neoDevice);
        for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants / 2; i++) {
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT2[i]));
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i + 1]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT1[i]));
        }
        EXPECT_TRUE(success);
        module->destroy();
    }

    const uint32_t moduleNumSpecConstants = 3 * 2;
    ze_module_constants_t specConstants;
    std::vector<const void *> specConstantsPointerValues;
    std::vector<uint32_t> specConstantsPointerIds;

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    MockCompilerInterfaceWithSpecConstants<T1, T2> *mockCompiler;
    MockModuleTranslationUnit *mockTranslationUnit;
};

template <typename T1, typename T2>
using ModuleSpecConstantsTests = Test<ModuleSpecConstantsFixture<T1, T2>>;

using ModuleSpecConstantsLongTests = ModuleSpecConstantsTests<uint32_t, uint64_t>;
TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWithLongSizeInDescriptorThenModuleCorrectlyPassesThemToTheCompiler) {
    runTest();
}
using ModuleSpecConstantsCharTests = ModuleSpecConstantsTests<char, uint32_t>;
TEST_F(ModuleSpecConstantsCharTests, givenSpecializationConstantsSetWithCharSizeInDescriptorThenModuleCorrectlyPassesThemToTheCompiler) {
    runTest();
}

TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWhenCompilerReturnsErrorThenModuleInitFails) {
    class FailingMockCompilerInterfaceWithSpecConstants : public MockCompilerInterfaceWithSpecConstants<uint32_t, uint64_t> {
      public:
        FailingMockCompilerInterfaceWithSpecConstants(uint32_t moduleNumSpecConstants) : MockCompilerInterfaceWithSpecConstants<uint32_t, uint64_t>(moduleNumSpecConstants) {}
        NEO::TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                               ArrayRef<const char> srcSpirV, NEO::SpecConstantInfo &output) override {
            return NEO::TranslationOutput::ErrorCode::CompilerNotAvailable;
        }
    };
    mockCompiler = new FailingMockCompilerInterfaceWithSpecConstants(moduleNumSpecConstants);
    auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
    for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants / 2; i++) {
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT2[i]);
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT1[i]);
    }

    specConstants.pConstantIds = mockCompiler->moduleSpecConstantsIds.data();
    specConstants.pConstantValues = specConstantsPointerValues.data();
    moduleDesc.pConstants = &specConstants;

    auto module = new Module(device, nullptr, ModuleType::User);
    module->translationUnit.reset(mockTranslationUnit);

    bool success = module->initialize(&moduleDesc, neoDevice);
    EXPECT_FALSE(success);
    module->destroy();
}

TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWhenUserPassTooMuchConstsIdsThenModuleInitFails) {
    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
    for (uint32_t i = mockCompiler->moduleNumSpecConstants / 2; i > 0; i--) {
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT1[i - 1]);
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT2[i - 1]);
    }
    for (uint32_t i = mockCompiler->moduleNumSpecConstants; i > 0; i--) {
        specConstantsPointerIds.push_back(mockCompiler->moduleSpecConstantsIds[i - 1]);
    }
    specConstantsPointerIds.push_back(0x1000);
    specConstants.numConstants += 1;

    specConstants.pConstantIds = specConstantsPointerIds.data();
    specConstants.pConstantValues = specConstantsPointerValues.data();
    moduleDesc.pConstants = &specConstants;

    auto module = new Module(device, nullptr, ModuleType::User);
    module->translationUnit.reset(mockTranslationUnit);

    bool success = module->initialize(&moduleDesc, neoDevice);
    EXPECT_FALSE(success);
    module->destroy();
}

using ModuleSpecConstantsLongTests = ModuleSpecConstantsTests<uint32_t, uint64_t>;
TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWithLongSizeInExpDescriptorThenStaticLinkedModuleCorrectlyPassesThemToTheCompiler) {
    runTestStatic();
}
using ModuleSpecConstantsCharTests = ModuleSpecConstantsTests<char, uint32_t>;
TEST_F(ModuleSpecConstantsCharTests, givenSpecializationConstantsSetWithCharSizeInExpDescriptorThenStaticLinkedModuleCorrectlyPassesThemToTheCompiler) {
    runTestStatic();
}

TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWhenCompilerReturnsErrorFromStaticLinkThenModuleInitFails) {
    class FailingMockCompilerInterfaceWithSpecConstants : public MockCompilerInterfaceWithSpecConstants<uint32_t, uint64_t> {
      public:
        FailingMockCompilerInterfaceWithSpecConstants(uint32_t moduleNumSpecConstants) : MockCompilerInterfaceWithSpecConstants<uint32_t, uint64_t>(moduleNumSpecConstants) {}
        NEO::TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                               ArrayRef<const char> srcSpirV, NEO::SpecConstantInfo &output) override {
            return NEO::TranslationOutput::ErrorCode::CompilerNotAvailable;
        }
    };
    mockCompiler = new FailingMockCompilerInterfaceWithSpecConstants(moduleNumSpecConstants);
    auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

    size_t sizeModule1, sizeModule2 = 0;
    auto srcModule1 = loadDataFromFile(testFile.c_str(), sizeModule1);
    auto srcModule2 = loadDataFromFile(testFile.c_str(), sizeModule2);

    ASSERT_NE(0u, sizeModule1);
    ASSERT_NE(0u, sizeModule2);
    ASSERT_NE(nullptr, srcModule1);
    ASSERT_NE(nullptr, srcModule2);

    ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    std::vector<const uint8_t *> inputSpirVs;
    std::vector<size_t> inputSizes;
    std::vector<ze_module_constants_t *> specConstantsArray;
    combinedModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    combinedModuleDesc.pNext = &staticLinkModuleDesc;

    specConstants.numConstants = mockCompiler->moduleNumSpecConstants;
    for (uint32_t i = mockCompiler->moduleNumSpecConstants / 2; i > 0; i--) {
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT1[i - 1]);
        specConstantsPointerValues.push_back(&mockCompiler->moduleSpecConstantsValuesT2[i - 1]);
    }
    for (uint32_t i = mockCompiler->moduleNumSpecConstants; i > 0; i--) {
        specConstantsPointerIds.push_back(mockCompiler->moduleSpecConstantsIds[i - 1]);
    }
    specConstants.pConstantIds = specConstantsPointerIds.data();
    specConstants.pConstantValues = specConstantsPointerValues.data();
    specConstantsArray.push_back(&specConstants);
    specConstantsArray.push_back(&specConstants);

    inputSizes.push_back(sizeModule1);
    inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule1.get()));
    inputSizes.push_back(sizeModule2);
    inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule2.get()));

    staticLinkModuleDesc.count = 2;
    staticLinkModuleDesc.inputSizes = inputSizes.data();
    staticLinkModuleDesc.pInputModules = inputSpirVs.data();
    staticLinkModuleDesc.pConstants = const_cast<const ze_module_constants_t **>(specConstantsArray.data());

    auto module = new Module(device, nullptr, ModuleType::User);
    module->translationUnit.reset(mockTranslationUnit);

    bool success = module->initialize(&combinedModuleDesc, neoDevice);
    EXPECT_FALSE(success);
    module->destroy();
}

struct ModuleStaticLinkFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }

    void loadMultipleModules() {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".spv");

        srcModule1 = loadDataFromFile(testFile.c_str(), sizeModule1);
        srcModule2 = loadDataFromFile(testFile.c_str(), sizeModule2);

        ASSERT_NE(0u, sizeModule1);
        ASSERT_NE(0u, sizeModule2);
        ASSERT_NE(nullptr, srcModule1);
        ASSERT_NE(nullptr, srcModule2);
    }

    void setupExpProgramDesc(ze_module_format_t format) {
        combinedModuleDesc.format = format;
        combinedModuleDesc.pNext = &staticLinkModuleDesc;

        inputSizes.push_back(sizeModule1);
        inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule1.get()));
        inputSizes.push_back(sizeModule2);
        inputSpirVs.push_back(reinterpret_cast<const uint8_t *>(srcModule2.get()));

        staticLinkModuleDesc.count = 2;
        staticLinkModuleDesc.inputSizes = inputSizes.data();
        staticLinkModuleDesc.pInputModules = inputSpirVs.data();
    }

    void runLinkFailureTest() {
        MockCompilerInterfaceLinkFailure *mockCompiler;
        mockCompiler = new MockCompilerInterfaceLinkFailure();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadMultipleModules();

        setupExpProgramDesc(ZE_MODULE_FORMAT_IL_SPIRV);

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_FALSE(success);
        module->destroy();
    }
    void runSpirvFailureTest() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadMultipleModules();

        setupExpProgramDesc(ZE_MODULE_FORMAT_NATIVE);

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_FALSE(success);
        module->destroy();
    }
    void runExpDescFailureTest() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        ze_module_desc_t invalidExpDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        combinedModuleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        combinedModuleDesc.pNext = &invalidExpDesc;

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_FALSE(success);
        module->destroy();
    }
    void runSprivLinkBuildFlags() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadMultipleModules();

        setupExpProgramDesc(ZE_MODULE_FORMAT_IL_SPIRV);

        std::vector<char *> buildFlags;
        std::string module1BuildFlags("-ze-opt-disable");
        std::string module2BuildFlags("-ze-opt-greater-than-4GB-buffer-required");
        buildFlags.push_back(const_cast<char *>(module1BuildFlags.c_str()));
        buildFlags.push_back(const_cast<char *>(module2BuildFlags.c_str()));

        staticLinkModuleDesc.pBuildFlags = const_cast<const char **>(buildFlags.data());

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        bool success = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_TRUE(success);
        module->destroy();
    }
    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    MockModuleTranslationUnit *mockTranslationUnit;
    std::unique_ptr<char[]> srcModule1;
    std::unique_ptr<char[]> srcModule2;
    size_t sizeModule1, sizeModule2 = 0;
    std::vector<const uint8_t *> inputSpirVs;
    std::vector<size_t> inputSizes;
    ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
};

using ModuleStaticLinkTests = Test<ModuleStaticLinkFixture>;

TEST_F(ModuleStaticLinkTests, givenMultipleModulesProvidedForSpirVStaticLinkAndCompilerFailsThenFailureIsReturned) {
    runLinkFailureTest();
}

TEST_F(ModuleStaticLinkTests, givenMultipleModulesProvidedForSpirVStaticLinkAndFormatIsNotSpirvThenFailureisReturned) {
    runSpirvFailureTest();
}

TEST_F(ModuleStaticLinkTests, givenInvalidExpDescForModuleCreateThenFailureisReturned) {
    runExpDescFailureTest();
}

TEST_F(ModuleStaticLinkTests, givenMultipleModulesProvidedForSpirVStaticLinkAndBuildFlagsRequestedThenSuccessisReturned) {
    runSprivLinkBuildFlags();
}

using ModuleLinkingTest = Test<DeviceFixture>;

HWTEST_F(ModuleLinkingTest, whenExternFunctionsAllocationIsPresentThenItsBeingAddedToResidencyContainer) {
    Mock<Module> module(device, nullptr);
    MockGraphicsAllocation alloc;
    module.exportedFunctionsSurface = &alloc;

    uint8_t data{};
    KernelInfo kernelInfo{};
    kernelInfo.heapInfo.pKernelHeap = &data;
    kernelInfo.heapInfo.KernelHeapSize = sizeof(data);

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(&kernelInfo, device, 0, nullptr, nullptr, false);
    module.kernelImmDatas.push_back(std::move(kernelImmData));

    module.translationUnit->programInfo.linkerInput.reset(new NEO::LinkerInput);
    module.linkBinary();
    ASSERT_EQ(1U, module.kernelImmDatas[0]->getResidencyContainer().size());
    EXPECT_EQ(&alloc, module.kernelImmDatas[0]->getResidencyContainer()[0]);
}

HWTEST_F(ModuleLinkingTest, givenFailureDuringLinkingWhenCreatingModuleThenModuleInitialiationFails) {
    auto mockCompiler = new MockCompilerInterface();
    auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

    auto mockTranslationUnit = new MockModuleTranslationUnit(device);

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->valid = false;

    mockTranslationUnit->programInfo.linkerInput = std::move(linkerInput);
    uint8_t spirvData{};

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = &spirvData;
    moduleDesc.inputSize = sizeof(spirvData);

    Module module(device, nullptr, ModuleType::User);
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

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = &spirvData;
    moduleDesc.inputSize = sizeof(spirvData);

    Module module(device, nullptr, ModuleType::User);
    module.translationUnit.reset(mockTranslationUnit);

    bool success = module.initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(success);
    EXPECT_FALSE(module.isFullyLinked);
}
HWTEST_F(ModuleLinkingTest, givenNotFullyLinkedModuleWhenCreatingKernelThenErrorIsReturned) {
    Module module(device, nullptr, ModuleType::User);
    module.isFullyLinked = false;

    auto retVal = module.createKernel(nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED, retVal);
}

using ModulePropertyTest = Test<ModuleFixture>;

TEST_F(ModulePropertyTest, whenZeModuleGetPropertiesIsCalledThenGetPropertiesIsCalled) {
    Mock<Module> module(device, nullptr);
    ze_module_properties_t moduleProperties;
    moduleProperties.stype = ZE_STRUCTURE_TYPE_MODULE_PROPERTIES;
    moduleProperties.pNext = nullptr;

    // returning error code that is unlikely to be returned by the function
    module.getPropertiesResult = ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;

    ze_result_t res = zeModuleGetProperties(module.toHandle(), &moduleProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT, res);
}

TEST_F(ModulePropertyTest, givenCallToGetPropertiesWithoutUnresolvedSymbolsThenFlagIsNotSet) {
    ze_module_properties_t moduleProperties;

    ze_module_property_flags_t expectedFlags = 0;

    ze_result_t res = module->getProperties(&moduleProperties);
    moduleProperties.stype = ZE_STRUCTURE_TYPE_MODULE_PROPERTIES;
    moduleProperties.pNext = nullptr;

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(expectedFlags, moduleProperties.flags);
}

TEST_F(ModulePropertyTest, givenCallToGetPropertiesWithUnresolvedSymbolsThenFlagIsSet) {
    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    whitebox_cast(module.get())->unresolvedExternalsInfo.push_back({unresolvedRelocation});

    ze_module_property_flags_t expectedFlags = 0;
    expectedFlags |= ZE_MODULE_PROPERTY_FLAG_IMPORTS;

    ze_module_properties_t moduleProperties;
    moduleProperties.stype = ZE_STRUCTURE_TYPE_MODULE_PROPERTIES;
    moduleProperties.pNext = nullptr;

    ze_result_t res = module->getProperties(&moduleProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(expectedFlags, moduleProperties.flags);
}

struct ModuleDynamicLinkTests : public Test<ModuleFixture> {
    void SetUp() override {
        Test<ModuleFixture>::SetUp();
        module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
        module1 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    }
    std::unique_ptr<Module> module0;
    std::unique_ptr<Module> module1;
};

TEST_F(ModuleDynamicLinkTests, givenCallToDynamicLinkOnModulesWithoutUnresolvedSymbolsThenSuccessIsReturned) {
    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolsNotPresentInOtherModulesWhenDynamicLinkThenLinkFailureIsReturned) {

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_MODULE_LINK_FAILURE, res);
}

TEST_F(ModuleDynamicLinkTests, whenModuleIsAlreadyLinkedThenThereIsNoSymbolsVerification) {

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module0->isFullyLinked = true;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolWhenTheOtherModuleDefinesTheSymbolThenTheSegmentIsPatched) {

    uint64_t gpuAddress = 0x12345;
    uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::SymbolInfo symbolInfo{};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    module0->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module0->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    auto isaPtr = kernelImmData->getIsaGraphicsAllocation()->getUnderlyingBuffer();

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module1->symbols[unresolvedRelocation.symbolName] = relocatedSymbol;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(gpuAddress, *reinterpret_cast<uint64_t *>(ptrOffset(isaPtr, offset)));
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolWhenTheOtherModuleDefinesTheSymbolThenTheExportedFunctionSurfaceIntheExportModuleIsAddedToTheImportModuleResidencyContainer) {

    uint64_t gpuAddress = 0x12345;
    uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::SymbolInfo symbolInfo{};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    module0->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module0->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module1->symbols[unresolvedRelocation.symbolName] = relocatedSymbol;
    MockGraphicsAllocation alloc;
    module1->exportedFunctionsSurface = &alloc;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ((int)module0->kernelImmDatas[0]->getResidencyContainer().size(), 2);
    EXPECT_EQ(module0->kernelImmDatas[0]->getResidencyContainer().back(), &alloc);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolWhenTheOtherModuleDefinesTheSymbolThenTheBuildLogContainsTheSuccessfulLinkage) {

    uint64_t gpuAddress = 0x12345;
    uint32_t offset = 0x20;
    uint32_t offset2 = 0x40;

    ze_module_build_log_handle_t dynLinkLog;
    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::Linker::RelocationInfo unresolvedRelocation2;
    unresolvedRelocation2.symbolName = "unresolved2";
    unresolvedRelocation2.offset = offset2;
    unresolvedRelocation2.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal2;
    unresolvedExternal2.unresolvedRelocation = unresolvedRelocation2;

    NEO::SymbolInfo symbolInfo{};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    NEO::SymbolInfo symbolInfo2{};
    NEO::Linker::RelocatedSymbol relocatedSymbol2{symbolInfo2, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    module0->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation2});
    module0->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;
    module0->unresolvedExternalsInfo[1].instructionsSegmentId = 0u;

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module1->symbols[unresolvedRelocation.symbolName] = relocatedSymbol;

    module1->symbols[unresolvedRelocation2.symbolName] = relocatedSymbol2;

    MockGraphicsAllocation alloc;
    module1->exportedFunctionsSurface = &alloc;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), &dynLinkLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    size_t buildLogSize;
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, nullptr);
    EXPECT_GT((int)buildLogSize, 0);
    char *logBuffer = new char[buildLogSize]();
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, logBuffer);
    EXPECT_NE(logBuffer, "");
    delete[] logBuffer;
    zeModuleBuildLogDestroy(dynLinkLog);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolsNotPresentInAnotherModuleWhenDynamicLinkThenLinkFailureIsReturnedAndLogged) {
    uint32_t offset = 0x20;
    uint32_t offset2 = 0x40;

    ze_module_build_log_handle_t dynLinkLog;
    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::Linker::RelocationInfo unresolvedRelocation2;
    unresolvedRelocation2.symbolName = "unresolved2";
    unresolvedRelocation2.offset = offset2;
    unresolvedRelocation2.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal2;
    unresolvedExternal2.unresolvedRelocation = unresolvedRelocation2;

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    module0->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module0->unresolvedExternalsInfo.push_back({unresolvedRelocation2});
    module0->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;
    module0->unresolvedExternalsInfo[1].instructionsSegmentId = 0u;

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), &dynLinkLog);
    EXPECT_EQ(ZE_RESULT_ERROR_MODULE_LINK_FAILURE, res);
    size_t buildLogSize;
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, nullptr);
    EXPECT_GT((int)buildLogSize, 0);
    char *logBuffer = new char[buildLogSize]();
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, logBuffer);
    EXPECT_NE(logBuffer, "");
    delete[] logBuffer;
    zeModuleBuildLogDestroy(dynLinkLog);
}

class DeviceModuleSetArgBufferTest : public ModuleFixture, public ::testing::Test {
  public:
    void SetUp() override {
        ModuleFixture::SetUp();
    }

    void TearDown() override {
        ModuleFixture::TearDown();
    }

    void createKernelAndAllocMemory(uint32_t rootDeviceIndex, void **ptr, ze_kernel_handle_t *kernelHandle) {
        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();
        ze_result_t res = module.get()->createKernel(&kernelDesc, kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_host_mem_alloc_desc_t hostDesc = {};
        res = context->allocHostMem(&hostDesc, 4096u, rootDeviceIndex, ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }
};

HWTEST_F(DeviceModuleSetArgBufferTest,
         givenValidMemoryUsedinFirstCallToSetArgBufferThenNullptrSetOnTheSecondCallThenArgBufferisUpdatedInEachCallAndSuccessIsReturned) {
    uint32_t rootDeviceIndex = 0;
    createModuleFromBinary();

    ze_kernel_handle_t kernelHandle;
    void *validBufferPtr = nullptr;
    createKernelAndAllocMemory(rootDeviceIndex, &validBufferPtr, &kernelHandle);

    L0::KernelImp *kernel = reinterpret_cast<L0::KernelImp *>(Kernel::fromHandle(kernelHandle));
    ze_result_t res = kernel->setArgBuffer(0, sizeof(validBufferPtr), &validBufferPtr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
    auto crossThreadData = kernel->getCrossThreadData();
    auto argBufferPtr = ptrOffset(crossThreadData, arg.stateless);
    auto argBufferValue = *reinterpret_cast<uint64_t *>(const_cast<uint8_t *>(argBufferPtr));
    EXPECT_EQ(argBufferValue, reinterpret_cast<uint64_t>(validBufferPtr));

    for (auto alloc : kernel->getResidencyContainer()) {
        if (alloc && alloc->getGpuAddress() == reinterpret_cast<uint64_t>(validBufferPtr)) {
            EXPECT_EQ(rootDeviceIndex, alloc->getRootDeviceIndex());
        }
    }

    res = kernel->setArgBuffer(0, sizeof(validBufferPtr), nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    arg = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
    crossThreadData = kernel->getCrossThreadData();
    argBufferPtr = ptrOffset(crossThreadData, arg.stateless);
    argBufferValue = *reinterpret_cast<uint64_t *>(const_cast<uint8_t *>(argBufferPtr));
    EXPECT_NE(argBufferValue, reinterpret_cast<uint64_t>(validBufferPtr));

    context->freeMem(validBufferPtr);
    Kernel::fromHandle(kernelHandle)->destroy();
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
        kernelDesc.pKernelName = kernelName.c_str();
        ze_result_t res = modules[rootDeviceIndex].get()->createKernel(&kernelDesc, kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_host_mem_alloc_desc_t hostDesc = {};
        res = context->allocHostMem(&hostDesc, 4096u, rootDeviceIndex, ptr);
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

        context->freeMem(ptr);
        Kernel::fromHandle(kernelHandle)->destroy();
    }
}

using ContextModuleCreateTest = Test<ContextFixture>;

HWTEST_F(ContextModuleCreateTest, givenCallToCreateModuleThenModuleIsReturned) {
    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, "test_kernel_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    ze_module_handle_t hModule;
    ze_device_handle_t hDevice = device->toHandle();
    ze_result_t res = context->createModule(hDevice, &moduleDesc, &hModule, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    L0::Module *pModule = L0::Module::fromHandle(hModule);
    res = pModule->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

using ModuleTranslationUnitTest = Test<DeviceFixture>;

struct MockModuleTU : public L0::ModuleTranslationUnit {
    MockModuleTU(L0::Device *device) : L0::ModuleTranslationUnit(device) {}

    bool buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                        const ze_module_constants_t *pConstants) override {
        wasBuildFromSpirVCalled = true;
        return true;
    }

    bool createFromNativeBinary(const char *input, size_t inputSize) override {
        wasCreateFromNativeBinaryCalled = true;
        return L0::ModuleTranslationUnit::createFromNativeBinary(input, inputSize);
    }

    bool wasBuildFromSpirVCalled = false;
    bool wasCreateFromNativeBinaryCalled = false;
};

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildPrecompiledKernelsFlagAndFileWithoutIntermediateCodeWhenCreatingModuleFromNativeBinaryThenModuleIsNotRecompiled) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, "test_kernel_", ".gen");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    Module module(device, nullptr, ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    bool success = module.initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(success);
    EXPECT_TRUE(tu->wasCreateFromNativeBinaryCalled);
    EXPECT_FALSE(tu->wasBuildFromSpirVCalled);
}

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildPrecompiledKernelsFlagAndFileWithIntermediateCodeWhenCreatingModuleFromNativeBinaryThenModuleIsRecompiled) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, "test_kernel_", ".bin");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    Module module(device, nullptr, ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    bool success = module.initialize(&moduleDesc, neoDevice);
    EXPECT_TRUE(success);
    EXPECT_TRUE(tu->wasCreateFromNativeBinaryCalled);
    EXPECT_EQ(tu->irBinarySize != 0, tu->wasBuildFromSpirVCalled);
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromNativeBinaryThenSetsUpRequiredTargetProductProperly) {
    ZebinTestData::ValidEmptyProgram emptyProgram;
    auto hwInfo = device->getNEODevice()->getHardwareInfo();

    emptyProgram.elfHeader->machine = hwInfo.platform.eProductFamily;
    L0::ModuleTranslationUnit moduleTuValid(this->device);
    bool success = moduleTuValid.createFromNativeBinary(reinterpret_cast<const char *>(emptyProgram.storage.data()), emptyProgram.storage.size());
    EXPECT_TRUE(success);

    emptyProgram.elfHeader->machine = hwInfo.platform.eProductFamily;
    ++emptyProgram.elfHeader->machine;
    L0::ModuleTranslationUnit moduleTuInvalid(this->device);
    success = moduleTuInvalid.createFromNativeBinary(reinterpret_cast<const char *>(emptyProgram.storage.data()), emptyProgram.storage.size());
    EXPECT_FALSE(success);
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromZeBinaryThenLinkerInputIsCreated) {
    std::string validZeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
    - name : some_other_kernel
      execution_env :
        simd_size : 32
)===";
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_other_kernel", {});

    auto hwInfo = device->getNEODevice()->getHardwareInfo();
    zebin.elfHeader->machine = hwInfo.platform.eProductFamily;

    L0::ModuleTranslationUnit moduleTuValid(this->device);
    bool success = moduleTuValid.createFromNativeBinary(reinterpret_cast<const char *>(zebin.storage.data()), zebin.storage.size());
    EXPECT_TRUE(success);

    EXPECT_NE(nullptr, moduleTuValid.programInfo.linkerInput.get());
}

TEST_F(ModuleTranslationUnitTest, WhenCreatingFromZeBinaryAndGlobalsAreExportedThenTheirAllocationTypeIsSVM) {
    std::string zeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : kernel
      execution_env :
        simd_size : 8
)===";
    MockElfEncoder<> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_ZEBIN_EXE;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "kernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConst, std::string{"12345678"});
    auto dataConstSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataGlobal, std::string{"12345678"});
    auto dataGlobalSectionIndex = elfEncoder.getLastSectionHeaderIndex();

    NEO::Elf::ElfSymbolEntry<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64> symbolTable[2] = {};
    symbolTable[0].name = decltype(symbolTable[0].name)(elfEncoder.appendSectionName("const.data"));
    symbolTable[0].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_OBJECT | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbolTable[0].shndx = decltype(symbolTable[0].shndx)(dataConstSectionIndex);
    symbolTable[0].size = 4;
    symbolTable[0].value = 0;

    symbolTable[1].name = decltype(symbolTable[1].name)(elfEncoder.appendSectionName("global.data"));
    symbolTable[1].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_OBJECT | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbolTable[1].shndx = decltype(symbolTable[1].shndx)(dataGlobalSectionIndex);
    symbolTable[1].size = 4;
    symbolTable[1].value = 0;
    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab,
                             ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(symbolTable), sizeof(symbolTable)));
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, zeInfo);
    auto zebin = elfEncoder.encode();

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.unpackedDeviceBinarySize = zebin.size();
    moduleTu.unpackedDeviceBinary = std::make_unique<char[]>(moduleTu.unpackedDeviceBinarySize);
    memcpy_s(moduleTu.unpackedDeviceBinary.get(), moduleTu.unpackedDeviceBinarySize,
             zebin.data(), zebin.size());
    auto retVal = moduleTu.processUnpackedBinary();
    EXPECT_TRUE(retVal);
    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_ZERO_COPY, moduleTu.globalConstBuffer->getAllocationType());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SVM_ZERO_COPY, moduleTu.globalVarBuffer->getAllocationType());
}

HWTEST_F(ModuleTranslationUnitTest, WhenBuildOptionsAreNullThenReuseExistingOptions) {

    auto *pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.options = "abcd";
    pMockCompilerInterface->failBuild = true;

    auto ret = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_FALSE(ret);
    EXPECT_STREQ("abcd", moduleTu.options.c_str());
    EXPECT_STREQ("abcd", pMockCompilerInterface->receivedApiOptions.c_str());
}

HWTEST_F(ModuleTranslationUnitTest, WhenBuildOptionsAreNullThenReuseExistingOptions2) {
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);

    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableStatelessToStatefulOptimization.set(1);

    MockModuleTranslationUnit moduleTu(this->device);
    auto ret = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_TRUE(ret);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
}

HWTEST_F(ModuleTranslationUnitTest, givenSystemSharedAllocationAllowedWhenBuildingModuleThen4GbBuffersAreRequired) {
    auto mockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    {
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 1;

        MockModuleTranslationUnit moduleTu(device);
        auto ret = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
        EXPECT_TRUE(ret);

        EXPECT_NE(mockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
    }

    {
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 0;

        MockModuleTranslationUnit moduleTu(device);
        auto ret = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
        EXPECT_TRUE(ret);

        EXPECT_EQ(mockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
    }
}

using PrintfModuleTest = Test<DeviceFixture>;

HWTEST_F(PrintfModuleTest, GivenModuleWithPrintfWhenKernelIsCreatedThenPrintfAllocationIsPlacedInResidencyContainer) {
    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, "test_kernel_", ".gen");

    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);

    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;

    auto module = std::unique_ptr<L0::Module>(Module::create(device, &moduleDesc, nullptr, ModuleType::User));

    auto kernel = std::make_unique<::testing::NiceMock<Mock<Kernel>>>();
    ASSERT_NE(nullptr, kernel);

    kernel->module = module.get();
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "test";
    kernel->initialize(&kernelDesc);

    auto &container = kernel->residencyContainer;
    auto printfPos = std::find(container.begin(), container.end(), kernel->getPrintfBufferAllocation());
    EXPECT_NE(container.end(), printfPos);
    bool correctPos = printfPos >= container.begin() + kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs.size();
    EXPECT_TRUE(correctPos);
}

TEST(BuildOptions, givenNoSrcOptionNameInSrcNamesWhenMovingBuildOptionsThenFalseIsReturned) {
    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, NEO::CompilerOptions::finiteMathOnly);
    std::string dstNames;

    auto result = moveBuildOption(dstNames, srcNames, BuildOptions::optDisable, NEO::CompilerOptions::optDisable);
    EXPECT_FALSE(result);
}

TEST(BuildOptions, givenSrcOptionNameInSrcNamesWhenMovingBuildOptionsThenOptionIsRemovedFromSrcNamesAndTranslatedOptionsStoredInDstNames) {
    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, NEO::CompilerOptions::optDisable);
    std::string dstNames;

    auto result = moveBuildOption(dstNames, srcNames, BuildOptions::optDisable, NEO::CompilerOptions::optDisable);
    EXPECT_TRUE(result);

    EXPECT_EQ(BuildOptions::optDisable, dstNames);
    EXPECT_EQ(std::string::npos, srcNames.find(NEO::CompilerOptions::optDisable.str()));
}

TEST(BuildOptions, givenSrcOptLevelInSrcNamesWhenMovingBuildOptionsThenOptionIsRemovedFromSrcNamesAndTranslatedOptionsStoredInDstNames) {
    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::optLevel);
    srcNames += "=2";
    std::string dstNames;

    auto result = moveBuildOption(dstNames, srcNames, NEO::CompilerOptions::optLevel, BuildOptions::optLevel);
    EXPECT_TRUE(result);

    EXPECT_EQ(NEO::CompilerOptions::optLevel.str() + std::string("2"), dstNames);
    EXPECT_EQ(std::string::npos, srcNames.find(BuildOptions::optLevel.str()));
    EXPECT_EQ(std::string::npos, srcNames.find(std::string("=2")));
}

TEST(BuildOptions, givenSrcOptLevelWithoutLevelIntegerInSrcNamesWhenMovingBuildOptionsThenFalseIsReturned) {
    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::optLevel);
    std::string dstNames;

    auto result = moveBuildOption(dstNames, srcNames, NEO::CompilerOptions::optLevel, BuildOptions::optLevel);
    EXPECT_FALSE(result);
}

TEST_F(ModuleTest, givenInternalOptionsWhenBindlessEnabledThenBindlesOptionsPassed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions("", buildOptions, internalBuildOptions);

    EXPECT_TRUE(NEO::CompilerOptions::contains(internalBuildOptions, NEO::CompilerOptions::bindlessMode));
}

TEST_F(ModuleTest, givenInternalOptionsWhenBuildFlagsIsNullPtrAndBindlessEnabledThenBindlesOptionsPassed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions(nullptr, buildOptions, internalBuildOptions);

    EXPECT_TRUE(NEO::CompilerOptions::contains(internalBuildOptions, NEO::CompilerOptions::bindlessMode));
}

TEST_F(ModuleTest, givenInternalOptionsWhenBindlessDisabledThenBindlesOptionsNotPassed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(0);
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions("", buildOptions, internalBuildOptions);

    EXPECT_FALSE(NEO::CompilerOptions::contains(internalBuildOptions, NEO::CompilerOptions::bindlessMode));
}

TEST_F(ModuleTest, GivenInjectInternalBuildOptionsWhenBuildingUserModuleThenInternalOptionsAreAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set(" -abc");

    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device->getNEODevice());

    EXPECT_TRUE(CompilerOptions::contains(cip->buildInternalOptions, "-abc"));
};

TEST_F(ModuleTest, GivenInjectInternalBuildOptionsWhenBuildingBuiltinModuleThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set(" -abc");

    NEO::MockCompilerEnableGuard mock(true);
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

    uint8_t binary[10];
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = binary;
    moduleDesc.inputSize = 10;

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::Builtin));
    ASSERT_NE(nullptr, module.get());
    module->initialize(&moduleDesc, device->getNEODevice());

    EXPECT_FALSE(CompilerOptions::contains(cip->buildInternalOptions, "-abc"));
};

TEST_F(ModuleTest, givenSharedSystemAllocationsSupportWhenGenerateCompilerOptionsThenOptionsAreCorrect) {
    auto areSharedSystemAllocationsSupported = device->getNEODevice()->areSharedSystemAllocationsAllowed();
    if (!areSharedSystemAllocationsSupported) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);
    auto moduleTranslationUnit = module->getTranslationUnit();
    ASSERT_NE(nullptr, moduleTranslationUnit);
    std::string buildOptions;
    std::string internalBuildOptions;

    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(-1);
        buildOptions = "";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required"));
    }
    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(-1);
        buildOptions = "-ze-opt-smaller-than-4GB-buffers-only";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::Not(testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required")));
    }
    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(0);
        buildOptions = "";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required"));
    }
    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(0);
        buildOptions = "-ze-opt-smaller-than-4GB-buffers-only";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required"));
    }
    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(1);
        buildOptions = "";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::Not(testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required")));
    }
    {
        NEO::DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(1);
        buildOptions = "-ze-opt-smaller-than-4GB-buffers-only";
        auto internalOptions = moduleTranslationUnit->generateCompilerOptions(buildOptions.c_str(), internalBuildOptions.c_str());
        EXPECT_THAT(internalOptions, testing::Not(testing::HasSubstr("-cl-intel-greater-than-4GB-buffer-required")));
    }
}

TEST_F(ModuleTest, whenContainsStatefulAccessIsCalledThenResultIsCorrect) {
    class MyModuleImpl : public ModuleImp {
      public:
        using ModuleImp::ModuleImp;
    };

    std::vector<std::tuple<bool, SurfaceStateHeapOffset, CrossThreadDataOffset>> testParams = {
        {false, undefined<SurfaceStateHeapOffset>, undefined<CrossThreadDataOffset>},
        {true, 0x40, undefined<CrossThreadDataOffset>},
        {true, undefined<SurfaceStateHeapOffset>, 0x40},
        {true, 0x40, 0x40},
    };

    for (auto &[expectedResult, surfaceStateHeapOffset, crossThreadDataOffset] : testParams) {
        auto module = std::make_unique<MyModuleImpl>(device, nullptr, ModuleType::User);
        ASSERT_NE(nullptr, module);
        auto moduleTranslationUnit = module->getTranslationUnit();
        ASSERT_NE(nullptr, moduleTranslationUnit);
        auto kernelInfo = std::make_unique<KernelInfo>();
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
        auto argDescriptor = ArgDescriptor(ArgDescriptor::ArgTPointer);
        argDescriptor.as<ArgDescPointer>().bindful = surfaceStateHeapOffset;
        argDescriptor.as<ArgDescPointer>().bindless = crossThreadDataOffset;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
        moduleTranslationUnit->programInfo.kernelInfos.clear();
        moduleTranslationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());

        EXPECT_EQ(expectedResult, NEO::AddressingModeHelper::containsStatefulAccess(moduleTranslationUnit->programInfo.kernelInfos));
    }
}

using ModuleInitializeTest = Test<DeviceFixture>;

TEST_F(ModuleInitializeTest, whenModuleInitializeIsCalledThenCorrectResultIsReturned) {
    DebugManagerStateRestore restorer;
    class MockModuleImp : public ModuleImp {
      public:
        using ModuleImp::ModuleImp;
        using ModuleImp::translationUnit;

        void setAddressingMode(bool isStateful) {
            auto kernelInfo = std::make_unique<KernelInfo>();
            kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.clear();
            auto argDescriptor = ArgDescriptor(ArgDescriptor::ArgTPointer);
            if (isStateful) {
                argDescriptor.as<ArgDescPointer>().bindful = 0x40;
                argDescriptor.as<ArgDescPointer>().bindless = 0x40;
            } else {
                argDescriptor.as<ArgDescPointer>().bindful = undefined<SurfaceStateHeapOffset>;
                argDescriptor.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
            }
            kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
            kernelInfo->heapInfo.KernelHeapSize = 0x1;
            kernelInfo->heapInfo.pKernelHeap = reinterpret_cast<void *>(0xffff);

            this->translationUnit->programInfo.kernelInfos.clear();
            this->translationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());
        }
    };

    class MyMockModuleTU : public MockModuleTU {
      public:
        using MockModuleTU::MockModuleTU;
        bool createFromNativeBinary(const char *input, size_t inputSize) { return true; }
    };

    std::string testFile;
    retrieveBinaryKernelFilenameNoRevision(testFile, "test_kernel_", ".bin");
    size_t size = 0;
    auto src = loadDataFromFile(testFile.c_str(), size);
    ASSERT_NE(0u, size);
    ASSERT_NE(nullptr, src);
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
    moduleDesc.inputSize = size;
    device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities = 1;

    std::array<std::tuple<bool, bool, ModuleType, int32_t>, 4> testParams = {{{true, false, ModuleType::Builtin, -1},
                                                                              {true, true, ModuleType::Builtin, 1},
                                                                              {true, true, ModuleType::Builtin, 0},
                                                                              {false, true, ModuleType::User, 0}}};

    for (auto &[expectedResult, isStateful, moduleType, debugKey] : testParams) {
        MockModuleImp module(device, nullptr, moduleType);
        module.translationUnit = std::make_unique<MyMockModuleTU>(device);
        DebugManager.flags.UseSmallerThan4gbBuffersOnly.set(debugKey);
        module.setAddressingMode(isStateful);
        EXPECT_EQ(expectedResult, module.initialize(&moduleDesc, device->getNEODevice()));
    }
}

using ModuleDebugDataTest = Test<DeviceFixture>;
TEST_F(ModuleDebugDataTest, GivenDebugDataWithRelocationsWhenCreatingRelocatedDebugDataThenRelocationsAreApplied) {
    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->compilerInterface.reset(cip);

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

    module->translationUnit->globalVarBuffer = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});
    module->translationUnit->globalConstBuffer = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::GraphicsAllocation::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;
    kernelInfo->kernelDescriptor.external.debugData = std::make_unique<NEO::DebugData>();

    auto debugData = MockElfEncoder<>::createRelocateableDebugDataElf();

    kernelInfo->kernelDescriptor.external.debugData->vIsaSize = static_cast<uint32_t>(debugData.size());
    kernelInfo->kernelDescriptor.external.debugData->vIsa = reinterpret_cast<char *>(debugData.data());

    // pass kernelInfo ownership to programInfo
    module->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, module->translationUnit->globalConstBuffer, module->translationUnit->globalVarBuffer, false);
    kernelImmData->createRelocatedDebugData(module->translationUnit->globalConstBuffer, module->translationUnit->globalVarBuffer);

    module->kernelImmDatas.push_back(std::move(kernelImmData));

    EXPECT_NE(nullptr, kernelInfo->kernelDescriptor.external.relocatedDebugData);

    uint64_t *relocAddress = reinterpret_cast<uint64_t *>(kernelInfo->kernelDescriptor.external.relocatedDebugData.get() + 600);
    auto expectedValue = module->kernelImmDatas[0]->getIsaGraphicsAllocation()->getGpuAddress() + 0x1a8;
    EXPECT_EQ(expectedValue, *relocAddress);
}

TEST_F(ModuleTest, givenModuleWithSymbolWhenGettingGlobalPointerThenSizeAndPointerAreReurned) {
    uint64_t gpuAddress = 0x12345000;

    NEO::SymbolInfo symbolInfo{0, 1024u, SegmentType::GlobalVariables};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->symbols["symbol"] = relocatedSymbol;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("symbol", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1024u, size);
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(ptr));
}

TEST_F(ModuleTest, givenModuleWithSymbolWhenGettingGlobalPointerWithNullptrInputsThenSuccessIsReturned) {
    uint64_t gpuAddress = 0x12345000;

    NEO::SymbolInfo symbolInfo{0, 1024u, SegmentType::GlobalVariables};
    NEO::Linker::RelocatedSymbol relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->symbols["symbol"] = relocatedSymbol;

    auto result = module0->getGlobalPointer("symbol", nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using ModuleTests = Test<DeviceFixture>;
TEST_F(ModuleTests, whenCopyingPatchedSegmentsThenAllocationsAreSetWritableForTbxAndAub) {
    auto pModule = std::make_unique<Module>(device, nullptr, ModuleType::User);

    char data[1]{};
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, false);

    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    NEO::Linker::PatchableSegments segments{{data, 1}};

    auto allocation = pModule->kernelImmDatas[0]->getIsaGraphicsAllocation();

    allocation->setTbxWritable(false, std::numeric_limits<uint32_t>::max());
    allocation->setAubWritable(false, std::numeric_limits<uint32_t>::max());

    pModule->copyPatchedSegments(segments);

    EXPECT_TRUE(allocation->isTbxWritable(std::numeric_limits<uint32_t>::max()));
    EXPECT_TRUE(allocation->isAubWritable(std::numeric_limits<uint32_t>::max()));
}

TEST_F(ModuleTests, givenConstDataStringSectionWhenLinkingModuleThenSegmentIsPatched) {
    auto pModule = std::make_unique<Module>(device, nullptr, ModuleType::User);

    char data[64]{};
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, nullptr, nullptr, false);
    auto patchAddr = reinterpret_cast<uintptr_t>(ptrOffset(kernelImmData->isaGraphicsAllocation->getUnderlyingBuffer(), 0x8));
    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->relocations.push_back({{".str", 0x8, LinkerInput::RelocationInfo::Type::Address, SegmentType::Instructions}});
    linkerInput->symbols.insert({".str", {0x0, 0x8, SegmentType::GlobalStrings}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    const char constStringData[] = "Hello World!\n";
    auto stringsAddr = reinterpret_cast<uintptr_t>(constStringData);
    pModule->translationUnit->programInfo.globalStrings.initData = constStringData;
    pModule->translationUnit->programInfo.globalStrings.size = sizeof(constStringData);

    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);
    EXPECT_EQ(static_cast<size_t>(stringsAddr), *reinterpret_cast<size_t *>(patchAddr));
}

TEST_F(ModuleTests, givenImplicitArgsRelocationAndStackCallsWhenLinkingModuleThenSegmentIsPatchedAndImplicitArgsAreRequired) {
    auto pModule = std::make_unique<Module>(device, nullptr, ModuleType::User);

    char data[64]{};
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, nullptr, nullptr, false);

    kernelImmData->kernelDescriptor->kernelAttributes.flags.useStackCalls = true;
    auto isaCpuPtr = reinterpret_cast<char *>(kernelImmData->isaGraphicsAllocation->getUnderlyingBuffer());
    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    EXPECT_EQ(sizeof(ImplicitArgs), *reinterpret_cast<uint32_t *>(ptrOffset(isaCpuPtr, 0x8)));

    EXPECT_TRUE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(ModuleTests, givenImplicitArgsRelocationAndDebuggerEnabledWhenLinkingModuleThenSegmentIsPatchedAndImplicitArgsAreRequired) {
    if (!defaultHwInfo->capabilityTable.debuggerSupported) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableMockSourceLevelDebugger.set(1);
    auto pModule = std::make_unique<Module>(device, nullptr, ModuleType::User);
    device->getNEODevice()->getRootDeviceEnvironmentRef().initDebugger();

    EXPECT_NE(nullptr, neoDevice->getDebugger());

    char data[64]{};
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, nullptr, nullptr, false);

    kernelImmData->kernelDescriptor->kernelAttributes.flags.useStackCalls = false;
    auto isaCpuPtr = reinterpret_cast<char *>(kernelImmData->isaGraphicsAllocation->getUnderlyingBuffer());
    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    EXPECT_EQ(sizeof(ImplicitArgs), *reinterpret_cast<uint32_t *>(ptrOffset(isaCpuPtr, 0x8)));

    EXPECT_TRUE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(ModuleTests, givenImplicitArgsRelocationAndNoDebuggerOrStackCallsWhenLinkingModuleThenSegmentIsPatchedAndImplicitArgsAreNotRequired) {
    auto pModule = std::make_unique<Module>(device, nullptr, ModuleType::User);
    EXPECT_EQ(nullptr, neoDevice->getDebugger());

    char data[64]{};
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, nullptr, nullptr, false);

    kernelImmData->kernelDescriptor->kernelAttributes.flags.useStackCalls = false;
    auto isaCpuPtr = reinterpret_cast<char *>(kernelImmData->isaGraphicsAllocation->getUnderlyingBuffer());
    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    EXPECT_EQ(0u, *reinterpret_cast<uint32_t *>(ptrOffset(isaCpuPtr, 0x8)));

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

using ModuleWithZebinTest = Test<ModuleWithZebinFixture>;
TEST_F(ModuleWithZebinTest, givenNoZebinThenSegmentsAreEmpty) {
    auto segments = module->getZebinSegments();

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.constData.address);
    EXPECT_TRUE(segments.constData.data.empty());

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.varData.address);
    EXPECT_TRUE(segments.varData.data.empty());

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.stringData.address);
    EXPECT_TRUE(segments.stringData.data.empty());

    EXPECT_TRUE(segments.nameToSegMap.empty());
}

TEST_F(ModuleWithZebinTest, givenZebinSegmentsThenSegmentsArePopulated) {
    module->addSegments();
    auto segments = module->getZebinSegments();

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Debug::Segments::Segment segment) {
        EXPECT_EQ(static_cast<uintptr_t>(alloc->getGpuAddressToPatch()), segment.address);
        EXPECT_EQ(reinterpret_cast<uint8_t *>(alloc->getUnderlyingBuffer()), segment.data.begin());
        EXPECT_EQ(static_cast<size_t>(alloc->getUnderlyingBufferSize()), segment.data.size());
    };
    checkGPUSeg(module->translationUnit->globalConstBuffer, segments.constData);
    checkGPUSeg(module->translationUnit->globalConstBuffer, segments.varData);
    checkGPUSeg(module->kernelImmDatas[0]->getIsaGraphicsAllocation(), segments.nameToSegMap["kernel"]);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(module->translationUnit->programInfo.globalStrings.initData), segments.stringData.address);
    EXPECT_EQ(reinterpret_cast<const uint8_t *>(module->translationUnit->programInfo.globalStrings.initData), segments.stringData.data.begin());
    EXPECT_EQ(module->translationUnit->programInfo.globalStrings.size, segments.stringData.data.size());
}

} // namespace ult
} // namespace L0
