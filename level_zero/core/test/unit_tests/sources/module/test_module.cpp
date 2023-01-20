/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_warnings/compiler_warnings.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/debug_zebin.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/addressing_mode_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_source_level_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
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

    auto whiteboxModule = whiteboxCast(module.get());
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
    EXPECT_EQ(NEO::AllocationType::KERNEL_ISA, kernel->getIsaAllocation()->getAllocationType());
}

HWTEST_F(ModuleTest, givenBuiltinModuleWhenCreatedThenCorrectAllocationTypeIsUsedForIsa) {
    createModuleFromMockBinary(ModuleType::Builtin);
    createKernel();
    EXPECT_EQ(NEO::AllocationType::KERNEL_ISA_INTERNAL, kernel->getIsaAllocation()->getAllocationType());
}

HWTEST_F(ModuleTest, givenBlitterAvailableWhenCopyingPatchedSegmentsThenIsaIsTransferredToAllocationWithBlitter) {

    auto hwInfo = *NEO::defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    executionEnvironment->memoryManager.reset(new MockMemoryManager(false, true, false, *executionEnvironment));

    uint32_t blitterCalled = 0;
    auto mockBlitMemoryToAllocation = [&](const NEO::Device &device, NEO::GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                          Vec3<size_t> size) -> NEO::BlitOperationResult {
        memcpy(memory->getUnderlyingBuffer(), hostPtr, size.x);
        blitterCalled++;
        return BlitOperationResult::Success;
    };
    VariableBackup<NEO::BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup(
        &NEO::BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation);

    auto *neoMockDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0);
    Mock<L0::DeviceImp> device(neoMockDevice, neoMockDevice->getExecutionEnvironment());
    device.driverHandle = driverHandle.get();

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device.getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(&device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    module->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);
    module->initialize(&moduleDesc, neoDevice);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_TRUE(ki->isIsaCopiedToAllocation());
    }

    auto &productHelper = device.getProductHelper();
    if (productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *module->getKernelImmutableDataVector()[0]->getIsaGraphicsAllocation())) {
        EXPECT_EQ(zebinData->numOfKernels, blitterCalled);
    } else {
        EXPECT_EQ(0u, blitterCalled);
    }
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
    kernelDesc.pKernelName = "nonexistent_kernel";

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_NAME, res);
}

template <typename T1, typename T2>
struct ModuleSpecConstantsFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();

        mockCompiler = new MockCompilerInterfaceWithSpecConstants<T1, T2>(moduleNumSpecConstants);
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);

        mockTranslationUnit = new MockModuleTranslationUnit(device);
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    void runTest() {
        auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
        const auto &src = zebinData->storage;

        ASSERT_NE(nullptr, src.data());
        ASSERT_NE(0u, src.size());

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = src.data();
        moduleDesc.inputSize = src.size();

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
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module->initialize(&moduleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants / 2; i++) {
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT2[i]));
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i + 1]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT1[i]));
        }
        module->destroy();
    }

    void runTestStatic() {
        auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
        const auto &src = zebinData->storage;

        auto srcModule1 = src.data();
        auto srcModule2 = src.data();
        auto sizeModule1 = src.size();
        auto sizeModule2 = src.size();
        ASSERT_NE(0u, sizeModule1);
        ASSERT_NE(0u, sizeModule2);
        ASSERT_NE(nullptr, srcModule1);
        ASSERT_NE(nullptr, srcModule2);

        ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
        std::vector<const uint8_t *> inputSpirVs;
        std::vector<size_t> inputSizes;
        std::vector<const ze_module_constants_t *> specConstantsArray;
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
        inputSpirVs.push_back(srcModule1);
        inputSizes.push_back(sizeModule2);
        inputSpirVs.push_back(srcModule2);

        staticLinkModuleDesc.count = 2;
        staticLinkModuleDesc.inputSizes = inputSizes.data();
        staticLinkModuleDesc.pInputModules = inputSpirVs.data();
        staticLinkModuleDesc.pConstants = specConstantsArray.data();

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        for (uint32_t i = 0; i < mockCompiler->moduleNumSpecConstants / 2; i++) {
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT2[i]));
            EXPECT_EQ(static_cast<uint64_t>(module->translationUnit->specConstantsValues[mockCompiler->moduleSpecConstantsIds[2 * i + 1]]), static_cast<uint64_t>(mockCompiler->moduleSpecConstantsValuesT1[i]));
        }
        module->destroy();
    }

    const uint32_t moduleNumSpecConstants = 3 * 2;
    ze_module_constants_t specConstants;
    std::vector<const void *> specConstantsPointerValues;
    std::vector<uint32_t> specConstantsPointerIds;

    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
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
    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
    const auto &src = zebinData->storage;

    ASSERT_NE(nullptr, src.data());
    ASSERT_NE(0u, src.size());

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = src.data();
    moduleDesc.inputSize = src.size();

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
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = module->initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
    module->destroy();
}

TEST_F(ModuleSpecConstantsLongTests, givenSpecializationConstantsSetWhenUserPassTooMuchConstsIdsThenModuleInitFails) {
    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
    const auto &src = zebinData->storage;

    ASSERT_NE(nullptr, src.data());
    ASSERT_NE(0u, src.size());

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = src.data();
    moduleDesc.inputSize = src.size();

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
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = module->initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
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
    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
    const auto &src = zebinData->storage;

    auto sizeModule1 = src.size();
    auto sizeModule2 = src.size();
    auto srcModule1 = src.data();
    auto srcModule2 = src.data();

    ASSERT_NE(0u, sizeModule1);
    ASSERT_NE(0u, sizeModule2);
    ASSERT_NE(nullptr, srcModule1);
    ASSERT_NE(nullptr, srcModule2);

    ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    std::vector<const uint8_t *> inputSpirVs;
    std::vector<size_t> inputSizes;
    std::vector<const ze_module_constants_t *> specConstantsArray;
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
    inputSpirVs.push_back(srcModule1);
    inputSizes.push_back(sizeModule2);
    inputSpirVs.push_back(srcModule2);

    staticLinkModuleDesc.count = 2;
    staticLinkModuleDesc.inputSizes = inputSizes.data();
    staticLinkModuleDesc.pInputModules = inputSpirVs.data();
    staticLinkModuleDesc.pConstants = specConstantsArray.data();

    auto module = new Module(device, nullptr, ModuleType::User);
    module->translationUnit.reset(mockTranslationUnit);
    ze_result_t result = ZE_RESULT_SUCCESS;
    result = module->initialize(&combinedModuleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
    module->destroy();
}

struct ModuleStaticLinkFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    void loadModules(bool multiple) {
        auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
        const auto &storage = zebinData->storage;

        srcModule1 = storage.data();
        sizeModule1 = storage.size();
        if (multiple) {
            srcModule2 = storage.data();
            sizeModule2 = storage.size();
        }

        ASSERT_NE(0u, sizeModule1);
        ASSERT_NE(nullptr, srcModule1);
        if (multiple) {
            ASSERT_NE(0u, sizeModule2);
            ASSERT_NE(nullptr, srcModule2);
        }
    }

    void setupExpProgramDesc(ze_module_format_t format, bool multiple) {
        combinedModuleDesc.format = format;
        combinedModuleDesc.pNext = &staticLinkModuleDesc;

        inputSizes.push_back(sizeModule1);
        inputSpirVs.push_back(srcModule1);
        staticLinkModuleDesc.count = 1;
        if (multiple) {
            inputSizes.push_back(sizeModule2);
            inputSpirVs.push_back(srcModule2);
            staticLinkModuleDesc.count = 2;
        }
        staticLinkModuleDesc.inputSizes = inputSizes.data();
        staticLinkModuleDesc.pInputModules = inputSpirVs.data();
    }

    void runLinkFailureTest() {
        MockCompilerInterfaceLinkFailure *mockCompiler;
        mockCompiler = new MockCompilerInterfaceLinkFailure();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadModules(testMultiple);

        setupExpProgramDesc(ZE_MODULE_FORMAT_IL_SPIRV, testMultiple);

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);

        ze_result_t result = ZE_RESULT_SUCCESS;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
        module->destroy();
    }
    void runSpirvFailureTest() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadModules(testMultiple);

        setupExpProgramDesc(ZE_MODULE_FORMAT_NATIVE, testMultiple);

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);
        ze_result_t result = ZE_RESULT_SUCCESS;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ENUMERATION);
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
        ze_result_t result = ZE_RESULT_SUCCESS;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
        module->destroy();
    }
    void runSprivLinkBuildFlags() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadModules(testMultiple);

        setupExpProgramDesc(ZE_MODULE_FORMAT_IL_SPIRV, testMultiple);

        std::vector<const char *> buildFlags;
        std::string module1BuildFlags("-ze-opt-disable");
        std::string module2BuildFlags("-ze-opt-greater-than-4GB-buffer-required");
        buildFlags.push_back(module1BuildFlags.c_str());
        buildFlags.push_back(module2BuildFlags.c_str());

        staticLinkModuleDesc.pBuildFlags = buildFlags.data();

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        module->destroy();
    }
    void runSprivLinkBuildWithOneModule() {
        MockCompilerInterface *mockCompiler;
        mockCompiler = new MockCompilerInterface();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->compilerInterface.reset(mockCompiler);
        mockTranslationUnit = new MockModuleTranslationUnit(device);

        loadModules(testSingle);

        setupExpProgramDesc(ZE_MODULE_FORMAT_IL_SPIRV, testSingle);

        std::vector<const char *> buildFlags;
        std::string module1BuildFlags("-ze-opt-disable");
        buildFlags.push_back(module1BuildFlags.c_str());

        staticLinkModuleDesc.pBuildFlags = buildFlags.data();

        auto module = new Module(device, nullptr, ModuleType::User);
        module->translationUnit.reset(mockTranslationUnit);
        ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
        result = module->initialize(&combinedModuleDesc, neoDevice);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        module->destroy();
    }
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
    const std::string kernelName = "test";
    MockModuleTranslationUnit *mockTranslationUnit;
    const uint8_t *srcModule1;
    const uint8_t *srcModule2;
    size_t sizeModule1, sizeModule2 = 0;
    std::vector<const uint8_t *> inputSpirVs;
    std::vector<size_t> inputSizes;
    ze_module_desc_t combinedModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_program_exp_desc_t staticLinkModuleDesc = {ZE_STRUCTURE_TYPE_MODULE_PROGRAM_EXP_DESC};
    bool testMultiple = true;
    bool testSingle = false;
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

TEST_F(ModuleStaticLinkTests, givenSingleModuleProvidedForSpirVStaticLinkAndBuildFlagsRequestedThenSuccessisReturned) {
    runSprivLinkBuildWithOneModule();
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

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_LINK_FAILURE);
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

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_FALSE(module.isFullyLinked);
}

HWTEST_F(ModuleLinkingTest, givenModuleCompiledThenCachingIsTrue) {
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

    EXPECT_FALSE(mockCompiler->cachingPassed);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(mockCompiler->cachingPassed);
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

    whiteboxCast(module.get())->unresolvedExternalsInfo.push_back({unresolvedRelocation});

    ze_module_property_flags_t expectedFlags = 0;
    expectedFlags |= ZE_MODULE_PROPERTY_FLAG_IMPORTS;

    ze_module_properties_t moduleProperties;
    moduleProperties.stype = ZE_STRUCTURE_TYPE_MODULE_PROPERTIES;
    moduleProperties.pNext = nullptr;

    ze_result_t res = module->getProperties(&moduleProperties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(expectedFlags, moduleProperties.flags);
}

struct ModuleFunctionPointerTests : public Test<ModuleFixture> {
    void SetUp() override {
        Test<ModuleFixture>::SetUp();
        module0 = std::make_unique<WhiteBox<::L0::Module>>(device, nullptr, ModuleType::User);
    }
    void TearDown() override {
        module0.reset(nullptr);
        Test<ModuleFixture>::TearDown();
    }
    std::unique_ptr<WhiteBox<::L0::Module>> module0;
};

struct ModuleDynamicLinkTests : public Test<DeviceFixture> {
    void SetUp() override {
        Test<DeviceFixture>::SetUp();
        module0 = std::make_unique<WhiteBox<::L0::Module>>(device, nullptr, ModuleType::User);
        module1 = std::make_unique<WhiteBox<::L0::Module>>(device, nullptr, ModuleType::User);
        module2 = std::make_unique<WhiteBox<::L0::Module>>(device, nullptr, ModuleType::User);
    }
    void TearDown() override {
        module0.reset(nullptr);
        module1.reset(nullptr);
        module2.reset(nullptr);
        Test<DeviceFixture>::TearDown();
    }
    std::unique_ptr<WhiteBox<::L0::Module>> module0;
    std::unique_ptr<WhiteBox<::L0::Module>> module1;
    std::unique_ptr<WhiteBox<::L0::Module>> module2;
};

TEST_F(ModuleDynamicLinkTests, givenCallToDynamicLinkOnModulesWithoutUnresolvedSymbolsThenSuccessIsReturned) {
    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolsNotPresentInOtherModulesWhenDynamicLinkThenLinkFailureIsReturned) {

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    module1->isFunctionSymbolExportEnabled = true;
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
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

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
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module1->symbols[unresolvedRelocation.symbolName] = relocatedSymbol;
    MockGraphicsAllocation alloc;
    module1->exportedFunctionsSurface = &alloc;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ((int)module0->kernelImmDatas[0]->getResidencyContainer().size(), 1);
    EXPECT_EQ(module0->kernelImmDatas[0]->getResidencyContainer().back(), &alloc);
}

TEST_F(ModuleDynamicLinkTests, givenMultipleModulesWithUnresolvedSymbolWhenTheEachModuleDefinesTheSymbolThenTheExportedFunctionSurfaceInBothModulesIsAddedToTheResidencyContainer) {

    uint64_t gpuAddress0 = 0x12345;
    uint64_t gpuAddress1 = 0x6789;
    uint64_t gpuAddress2 = 0x1479;
    uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::Linker::RelocationInfo unresolvedRelocationCircular;
    unresolvedRelocationCircular.symbolName = "unresolvedCircular";
    unresolvedRelocationCircular.offset = offset;
    unresolvedRelocationCircular.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternalCircular;
    unresolvedExternalCircular.unresolvedRelocation = unresolvedRelocationCircular;

    NEO::Linker::RelocationInfo unresolvedRelocationChained;
    unresolvedRelocationChained.symbolName = "unresolvedChained";
    unresolvedRelocationChained.offset = offset;
    unresolvedRelocationChained.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternalChained;
    unresolvedExternalChained.unresolvedRelocation = unresolvedRelocationChained;

    NEO::SymbolInfo module0SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module0RelocatedSymbol{module0SymbolInfo, gpuAddress0};

    NEO::SymbolInfo module1SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module1RelocatedSymbol{module1SymbolInfo, gpuAddress1};

    NEO::SymbolInfo module2SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module2RelocatedSymbol{module2SymbolInfo, gpuAddress2};

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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols[unresolvedRelocationCircular.symbolName] = module0RelocatedSymbol;
    MockGraphicsAllocation alloc0;
    module0->exportedFunctionsSurface = &alloc0;

    char kernelHeap2[MemoryConstants::pageSize] = {};

    auto kernelInfo2 = std::make_unique<NEO::KernelInfo>();
    kernelInfo2->heapInfo.pKernelHeap = kernelHeap2;
    kernelInfo2->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module1->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo2.release());

    auto linkerInput1 = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput1->traits.requiresPatchingOfInstructionSegments = true;

    module1->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput1);
    module1->unresolvedExternalsInfo.push_back({unresolvedRelocationCircular});
    module1->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;
    module1->unresolvedExternalsInfo.push_back({unresolvedRelocationChained});
    module1->unresolvedExternalsInfo[1].instructionsSegmentId = 0u;

    module1->symbols[unresolvedRelocation.symbolName] = module1RelocatedSymbol;
    MockGraphicsAllocation alloc1;
    module1->exportedFunctionsSurface = &alloc1;

    module2->symbols[unresolvedRelocationChained.symbolName] = module2RelocatedSymbol;
    MockGraphicsAllocation alloc2;
    module2->exportedFunctionsSurface = &alloc2;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle(), module2->toHandle()};
    ze_result_t res = module0->performDynamicLink(3, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ((int)module0->kernelImmDatas[0]->getResidencyContainer().size(), 3);
    EXPECT_TRUE(std::find(module0->kernelImmDatas[0]->getResidencyContainer().begin(), module0->kernelImmDatas[0]->getResidencyContainer().end(), &alloc0) != module0->kernelImmDatas[0]->getResidencyContainer().end());
    EXPECT_TRUE(std::find(module0->kernelImmDatas[0]->getResidencyContainer().begin(), module0->kernelImmDatas[0]->getResidencyContainer().end(), &alloc1) != module0->kernelImmDatas[0]->getResidencyContainer().end());
    EXPECT_TRUE(std::find(module0->kernelImmDatas[0]->getResidencyContainer().begin(), module0->kernelImmDatas[0]->getResidencyContainer().end(), &alloc2) != module0->kernelImmDatas[0]->getResidencyContainer().end());
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithInternalRelocationAndUnresolvedExternalSymbolWhenTheOtherModuleDefinesTheSymbolThenAllSymbolsArePatched) {
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->exportedFunctionsSegmentId = 0;

    uint32_t internalRelocationOffset = 0x10;
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, internalRelocationOffset, LinkerInput::RelocationInfo::Type::Address, SegmentType::Instructions}});
    uint32_t expectedInternalRelocationValue = sizeof(ImplicitArgs);

    uint32_t externalRelocationOffset = 0x20;
    constexpr auto externalSymbolName = "unresolved";
    uint64_t externalSymbolAddress = castToUint64(&externalRelocationOffset);
    linkerInput->textRelocations[0].push_back({externalSymbolName, externalRelocationOffset, LinkerInput::RelocationInfo::Type::Address, SegmentType::Instructions});

    char kernelHeap[MemoryConstants::cacheLineSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::cacheLineSize;
    kernelInfo->kernelDescriptor.kernelAttributes.flags.useStackCalls = true;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    module0->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput);

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::cacheLineSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    auto isaPtr = kernelImmData->getIsaGraphicsAllocation()->getUnderlyingBuffer();

    memset(isaPtr, 0, MemoryConstants::cacheLineSize);

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module1->symbols[externalSymbolName] = {{}, externalSymbolAddress};

    EXPECT_TRUE(module0->linkBinary());

    EXPECT_NE(expectedInternalRelocationValue, *reinterpret_cast<uint32_t *>(ptrOffset(isaPtr, internalRelocationOffset)));
    EXPECT_FALSE(module0->isFullyLinked);

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_TRUE(module0->isFullyLinked);
    EXPECT_EQ(expectedInternalRelocationValue, *reinterpret_cast<uint32_t *>(ptrOffset(isaPtr, internalRelocationOffset)));
    EXPECT_EQ(externalSymbolAddress, *reinterpret_cast<uint64_t *>(ptrOffset(isaPtr, externalRelocationOffset)));
}

TEST_F(ModuleDynamicLinkTests, givenMultipleModulesWithUnresolvedSymbolWhenTheEachModuleDefinesTheSymbolThenTheExportedFunctionSurfaceInBothModulesIsAddedToTheImportedSymbolAllocations) {

    uint64_t gpuAddress0 = 0x12345;
    uint64_t gpuAddress1 = 0x6789;
    uint64_t gpuAddress2 = 0x1479;
    uint32_t offset = 0x20;

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";
    unresolvedRelocation.offset = offset;
    unresolvedRelocation.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternal;
    unresolvedExternal.unresolvedRelocation = unresolvedRelocation;

    NEO::Linker::RelocationInfo unresolvedRelocationCircular;
    unresolvedRelocationCircular.symbolName = "unresolvedCircular";
    unresolvedRelocationCircular.offset = offset;
    unresolvedRelocationCircular.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternalCircular;
    unresolvedExternalCircular.unresolvedRelocation = unresolvedRelocationCircular;

    NEO::Linker::RelocationInfo unresolvedRelocationChained;
    unresolvedRelocationChained.symbolName = "unresolvedChained";
    unresolvedRelocationChained.offset = offset;
    unresolvedRelocationChained.type = NEO::Linker::RelocationInfo::Type::Address;
    NEO::Linker::UnresolvedExternal unresolvedExternalChained;
    unresolvedExternalChained.unresolvedRelocation = unresolvedRelocationChained;

    NEO::SymbolInfo module0SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module0RelocatedSymbol{module0SymbolInfo, gpuAddress0};

    NEO::SymbolInfo module1SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module1RelocatedSymbol{module1SymbolInfo, gpuAddress1};

    NEO::SymbolInfo module2SymbolInfo{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> module2RelocatedSymbol{module2SymbolInfo, gpuAddress2};

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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols[unresolvedRelocationCircular.symbolName] = module0RelocatedSymbol;
    MockGraphicsAllocation alloc0;
    module0->exportedFunctionsSurface = &alloc0;

    char kernelHeap2[MemoryConstants::pageSize] = {};

    auto kernelInfo2 = std::make_unique<NEO::KernelInfo>();
    kernelInfo2->heapInfo.pKernelHeap = kernelHeap2;
    kernelInfo2->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module1->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo2.release());

    auto linkerInput1 = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput1->traits.requiresPatchingOfInstructionSegments = true;

    module1->getTranslationUnit()->programInfo.linkerInput = std::move(linkerInput1);
    module1->unresolvedExternalsInfo.push_back({unresolvedRelocationCircular});
    module1->unresolvedExternalsInfo[0].instructionsSegmentId = 0u;
    module1->unresolvedExternalsInfo.push_back({unresolvedRelocationChained});
    module1->unresolvedExternalsInfo[1].instructionsSegmentId = 0u;

    module1->symbols[unresolvedRelocation.symbolName] = module1RelocatedSymbol;
    MockGraphicsAllocation alloc1;
    module1->exportedFunctionsSurface = &alloc1;

    module2->symbols[unresolvedRelocationChained.symbolName] = module2RelocatedSymbol;
    MockGraphicsAllocation alloc2;
    module2->exportedFunctionsSurface = &alloc2;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle(), module2->toHandle()};
    ze_result_t res = module0->performDynamicLink(3, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ((int)module0->importedSymbolAllocations.size(), 3);
    EXPECT_EQ((int)module1->importedSymbolAllocations.size(), 3);
    EXPECT_EQ((int)module2->importedSymbolAllocations.size(), 3);
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
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    NEO::SymbolInfo symbolInfo2{};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol2{symbolInfo2, gpuAddress};

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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

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
    EXPECT_GT(static_cast<int>(buildLogSize), 0);
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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));
    module0->isFunctionSymbolExportEnabled = true;
    module1->isFunctionSymbolExportEnabled = true;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), &dynLinkLog);
    EXPECT_EQ(ZE_RESULT_ERROR_MODULE_LINK_FAILURE, res);
    size_t buildLogSize;
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, nullptr);
    EXPECT_GT(static_cast<int>(buildLogSize), 0);
    char *logBuffer = new char[buildLogSize]();
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, logBuffer);
    EXPECT_NE(logBuffer, "");
    delete[] logBuffer;
    zeModuleBuildLogDestroy(dynLinkLog);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithUnresolvedSymbolsNotPresentInAnotherModuleWhenDynamicLinkWithoutRequiredFlagsThenLinkFailureIsReturnedAndLogged) {
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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));
    module0->isFunctionSymbolExportEnabled = false;
    module1->isFunctionSymbolExportEnabled = false;

    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};
    ze_result_t res = module0->performDynamicLink(2, hModules.data(), &dynLinkLog);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);
    size_t buildLogSize;
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, nullptr);
    EXPECT_GT(static_cast<int>(buildLogSize), 0);
    char *logBuffer = new char[buildLogSize]();
    zeModuleBuildLogGetString(dynLinkLog, &buildLogSize, logBuffer);
    EXPECT_NE(logBuffer, "");
    delete[] logBuffer;
    zeModuleBuildLogDestroy(dynLinkLog);
}

using ModuleDynamicLinkTest = Test<ModuleFixture>;
TEST_F(ModuleDynamicLinkTest, givenUnresolvedSymbolsWhenModuleIsCreatedThenIsaAllocationsAreNotCopied) {

    auto cip = new NEO::MockCompilerInterfaceCaptureBuildOptions();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->compilerInterface.reset(cip);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::make_unique<Module>(device, moduleBuildLog, ModuleType::User);
    ASSERT_NE(nullptr, module.get());

    NEO::Linker::RelocationInfo unresolvedRelocation;
    unresolvedRelocation.symbolName = "unresolved";

    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->dataRelocations.push_back(unresolvedRelocation);
    linkerInput->traits.requiresPatchingOfGlobalVariablesBuffer = true;
    module->unresolvedExternalsInfo.push_back({unresolvedRelocation});
    module->translationUnit->programInfo.linkerInput = std::move(linkerInput);
    module->initialize(&moduleDesc, neoDevice);

    for (auto &ki : module->getKernelImmutableDataVector()) {
        EXPECT_FALSE(ki->isIsaCopiedToAllocation());
    }

    EXPECT_FALSE(module->isFullyLinked);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithFunctionDependenciesWhenOtherModuleDefinesThisFunctionThenBarrierCountIsProperlyResolved) {
    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};

    auto linkerInput = new ::WhiteBox<NEO::LinkerInput>();
    linkerInput->extFunDependencies.push_back({"funMod1", "funMod0"});
    linkerInput->kernelDependencies.push_back({"funMod1", "kernel"});
    module0->translationUnit->programInfo.linkerInput.reset(linkerInput);

    module0->translationUnit->programInfo.externalFunctions.push_back({"funMod0", 1U, 128U, 8U});
    KernelInfo *ki = new KernelInfo();
    ki->kernelDescriptor.kernelMetadata.kernelName = "kernel";
    module0->translationUnit->programInfo.kernelInfos.push_back(ki);

    module1->translationUnit->programInfo.externalFunctions.push_back({"funMod1", 3U, 128U, 8U});

    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(3U, module0->translationUnit->programInfo.externalFunctions[0].barrierCount);
    EXPECT_EQ(3U, ki->kernelDescriptor.kernelAttributes.barrierCount);
}

TEST_F(ModuleDynamicLinkTests, givenModuleWithFunctionDependenciesWhenOtherModuleDoesNotDefineThisFunctionThenLinkFailureIsReturned) {
    std::vector<ze_module_handle_t> hModules = {module0->toHandle(), module1->toHandle()};

    auto linkerInput = new ::WhiteBox<NEO::LinkerInput>();
    linkerInput->extFunDependencies.push_back({"funMod1", "funMod0"});
    linkerInput->kernelDependencies.push_back({"funMod1", "kernel"});
    module0->translationUnit->programInfo.linkerInput.reset(linkerInput);

    module0->translationUnit->programInfo.externalFunctions.push_back({"funMod0", 1U, 128U, 8U});
    KernelInfo *ki = new KernelInfo();
    ki->kernelDescriptor.kernelMetadata.kernelName = "kernel";
    module0->translationUnit->programInfo.kernelInfos.push_back(ki);

    ze_result_t res = module0->performDynamicLink(2, hModules.data(), nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_MODULE_LINK_FAILURE, res);
}

TEST_F(ModuleFunctionPointerTests, givenModuleWithExportedSymbolThenGetFunctionPointerReturnsGpuAddressToFunction) {

    uint64_t gpuAddress = 0x12345;

    NEO::SymbolInfo symbolInfo{};
    symbolInfo.segment = NEO::SegmentType::Instructions;
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols["externalFunction"] = relocatedSymbol;

    void *functionPointer = nullptr;
    module0->isFunctionSymbolExportEnabled = true;
    ze_result_t res = module0->getFunctionPointer("externalFunction", &functionPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(reinterpret_cast<uint64_t>(functionPointer), gpuAddress);
}

TEST_F(ModuleFunctionPointerTests, givenModuleWithExportedSymbolButNoExportFlagsThenReturnsFailure) {

    uint64_t gpuAddress = 0x12345;

    NEO::SymbolInfo symbolInfo{};
    symbolInfo.segment = NEO::SegmentType::Instructions;
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());
    module0->isFunctionSymbolExportEnabled = false;
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    kernelImmData->kernelDescriptor = &kernelDescriptor;

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols["externalFunction"] = relocatedSymbol;

    void *functionPointer = nullptr;
    ze_result_t res = module0->getFunctionPointer("Invalid", &functionPointer);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, res);
}

TEST_F(ModuleFunctionPointerTests, givenInvalidFunctionNameAndModuleWithExportedSymbolThenGetFunctionPointerReturnsFailure) {

    uint64_t gpuAddress = 0x12345;

    NEO::SymbolInfo symbolInfo{};
    symbolInfo.segment = NEO::SegmentType::Instructions;
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());
    module0->isFunctionSymbolExportEnabled = true;
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    kernelImmData->kernelDescriptor = &kernelDescriptor;

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols["externalFunction"] = relocatedSymbol;

    void *functionPointer = nullptr;
    ze_result_t res = module0->getFunctionPointer("Invalid", &functionPointer);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_FUNCTION_NAME, res);
}

TEST_F(ModuleFunctionPointerTests, givenModuleWithExportedSymbolThenGetFunctionPointerReturnsGpuAddressToKernelFunction) {

    uint64_t gpuAddress = 0x12345;

    NEO::SymbolInfo symbolInfo{};
    symbolInfo.segment = NEO::SegmentType::Instructions;
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    char kernelHeap[MemoryConstants::pageSize] = {};

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    kernelInfo->heapInfo.pKernelHeap = kernelHeap;
    kernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";
    module0->isFunctionSymbolExportEnabled = true;
    module0->getTranslationUnit()->programInfo.kernelInfos.push_back(kernelInfo.release());
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelMetadata.kernelName = "kernelFunction";

    auto kernelImmData = std::make_unique<WhiteBox<::L0::KernelImmutableData>>(device);
    kernelImmData->isaGraphicsAllocation.reset(neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::KERNEL_ISA, neoDevice->getDeviceBitfield()}));

    kernelImmData->kernelDescriptor = &kernelDescriptor;

    module0->kernelImmDatas.push_back(std::move(kernelImmData));

    module0->symbols["externalFunction"] = relocatedSymbol;

    void *functionPointer = nullptr;
    ze_result_t res = module0->getFunctionPointer("kernelFunction", &functionPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(reinterpret_cast<uint64_t>(functionPointer), module0->kernelImmDatas[0]->getIsaGraphicsAllocation()->getGpuAddress());
}

class DeviceModuleSetArgBufferTest : public ModuleFixture, public ::testing::Test {
  public:
    void SetUp() override {
        ModuleFixture::setUp();
    }

    void TearDown() override {
        ModuleFixture::tearDown();
    }

    void createKernelAndAllocMemory(uint32_t rootDeviceIndex, void **ptr, ze_kernel_handle_t *kernelHandle) {
        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();
        ze_result_t res = module->createKernel(&kernelDesc, kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_host_mem_alloc_desc_t hostDesc = {};
        res = context->allocHostMem(&hostDesc, 4096u, rootDeviceIndex, ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }
};

HWTEST_F(DeviceModuleSetArgBufferTest,
         givenValidMemoryUsedinFirstCallToSetArgBufferThenNullptrSetOnTheSecondCallThenArgBufferisUpdatedInEachCallAndSuccessIsReturned) {
    uint32_t rootDeviceIndex = 0;
    createModuleFromMockBinary();

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
        MultiDeviceModuleFixture::setUp();
    }

    void TearDown() override {
        MultiDeviceModuleFixture::tearDown();
    }

    void createKernelAndAllocMemory(uint32_t rootDeviceIndex, void **ptr, ze_kernel_handle_t *kernelHandle) {
        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = kernelName.c_str();
        ze_result_t res = modules[rootDeviceIndex]->createKernel(&kernelDesc, kernelHandle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);

        ze_host_mem_alloc_desc_t hostDesc = {};
        res = context->allocHostMem(&hostDesc, 4096u, rootDeviceIndex, ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    }
};

HWTEST_F(MultiDeviceModuleSetArgBufferTest,
         givenCallsToSetArgBufferThenAllocationIsSetForCorrectDevice) {

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        createModuleFromMockBinary(rootDeviceIndex);

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

using ContextModuleCreateTest = Test<DeviceFixture>;

HWTEST_F(ContextModuleCreateTest, givenCallToCreateModuleThenModuleIsReturned) {
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

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

    ze_result_t buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                               const ze_module_constants_t *pConstants) override {
        wasBuildFromSpirVCalled = true;
        if (callRealBuildFromSpirv) {
            return L0::ModuleTranslationUnit::buildFromSpirV(input, inputSize, buildOptions, internalBuildOptions, pConstants);
        } else {
            return ZE_RESULT_SUCCESS;
        }
    }

    ze_result_t createFromNativeBinary(const char *input, size_t inputSize) override {
        wasCreateFromNativeBinaryCalled = true;
        return L0::ModuleTranslationUnit::createFromNativeBinary(input, inputSize);
    }

    bool callRealBuildFromSpirv = false;
    bool wasBuildFromSpirVCalled = false;
    bool wasCreateFromNativeBinaryCalled = false;
};

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildPrecompiledKernelsFlagAndFileWithoutIntermediateCodeWhenCreatingModuleFromNativeBinaryThenModuleIsNotRecompiled) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    Module module(device, nullptr, ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(tu->wasCreateFromNativeBinaryCalled);
    EXPECT_FALSE(tu->wasBuildFromSpirVCalled);
}

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildPrecompiledKernelsFlagAndFileWithoutIntermediateCodeWhenCreatingModuleFromNativeBinaryThenModuleHasSymbolSupportBooleans) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    Module module(device, nullptr, ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(tu->wasCreateFromNativeBinaryCalled);
    EXPECT_TRUE(module.isGlobalSymbolExportEnabled);
    EXPECT_TRUE(module.isFunctionSymbolExportEnabled);
}

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildPrecompiledKernelsFlagAndFileWithIntermediateCodeWhenCreatingModuleFromNativeBinaryThenModuleIsRecompiled) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    Module module(device, nullptr, ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_TRUE(tu->wasCreateFromNativeBinaryCalled);
    EXPECT_EQ(tu->irBinarySize != 0, tu->wasBuildFromSpirVCalled);
}

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildFlagWhenCreatingModuleFromNativeBinaryThenModuleRecompilationWarningIsIssued) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
    bool forceRecompilation = true;
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections, forceRecompilation);
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    std::unique_ptr<ModuleBuildLog> moduleBuildLog{ModuleBuildLog::create()};
    Module module(device, moduleBuildLog.get(), ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    size_t buildLogSize{};
    const auto querySizeResult{moduleBuildLog->getString(&buildLogSize, nullptr)};
    ASSERT_EQ(ZE_RESULT_SUCCESS, querySizeResult);

    std::string buildLog(buildLogSize, '\0');
    const auto queryBuildLogResult{moduleBuildLog->getString(&buildLogSize, buildLog.data())};
    ASSERT_EQ(ZE_RESULT_SUCCESS, queryBuildLogResult);

    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};
    EXPECT_TRUE(containsWarning);
}

HWTEST_F(ModuleTranslationUnitTest, GivenRebuildFlagWhenCreatingModuleFromNativeBinaryAndWarningSuppressionIsPresentThenModuleRecompilationWarningIsNotIssued) {
    DebugManagerStateRestore dgbRestorer;
    NEO::DebugManager.flags.RebuildPrecompiledKernels.set(true);

    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::SPIRV};
    bool forceRecompilation = true;
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections, forceRecompilation);
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();
    moduleDesc.pBuildFlags = CompilerOptions::noRecompiledFromIr.data();

    std::unique_ptr<ModuleBuildLog> moduleBuildLog{ModuleBuildLog::create()};
    Module module(device, moduleBuildLog.get(), ModuleType::User);
    MockModuleTU *tu = new MockModuleTU(device);
    module.translationUnit.reset(tu);

    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module.initialize(&moduleDesc, neoDevice);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    size_t buildLogSize{};
    const auto querySizeResult{moduleBuildLog->getString(&buildLogSize, nullptr)};
    ASSERT_EQ(ZE_RESULT_SUCCESS, querySizeResult);

    std::string buildLog(buildLogSize, '\0');
    const auto queryBuildLogResult{moduleBuildLog->getString(&buildLogSize, buildLog.data())};
    ASSERT_EQ(ZE_RESULT_SUCCESS, queryBuildLogResult);

    const auto containsWarning{buildLog.find(CompilerWarnings::recompiledFromIr.data()) != std::string::npos};
    EXPECT_FALSE(containsWarning);
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromNativeBinaryThenSetsUpRequiredTargetProductProperly) {
    ZebinTestData::ValidEmptyProgram emptyProgram;

    auto copyHwInfo = device->getNEODevice()->getHardwareInfo();
    auto compilerProductHelper = NEO::CompilerProductHelper::get(copyHwInfo.platform.eProductFamily);

    compilerProductHelper->adjustHwInfoForIgc(copyHwInfo);

    emptyProgram.elfHeader->machine = copyHwInfo.platform.eProductFamily;
    L0::ModuleTranslationUnit moduleTuValid(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTuValid.createFromNativeBinary(reinterpret_cast<const char *>(emptyProgram.storage.data()), emptyProgram.storage.size());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    emptyProgram.elfHeader->machine = copyHwInfo.platform.eProductFamily;
    ++emptyProgram.elfHeader->machine;
    L0::ModuleTranslationUnit moduleTuInvalid(this->device);
    result = moduleTuInvalid.createFromNativeBinary(reinterpret_cast<const char *>(emptyProgram.storage.data()), emptyProgram.storage.size());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, result);
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromNativeBinaryThenSetsUpPackedTargetDeviceBinary) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    auto &productHelper = device->getProductHelper();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productHelper.getProductConfigFromHwInfo(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.aotConfig.value = productHelper.getProductConfigFromHwInfo(hwInfo);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    L0::ModuleTranslationUnit moduleTuValid(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTuValid.createFromNativeBinary(reinterpret_cast<const char *>(arData.data()), arData.size());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(moduleTuValid.packedDeviceBinarySize, arData.size());
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromZebinThenAppendAllowZebinFlagToBuildOptions) {
    ZebinTestData::ValidEmptyProgram zebin;

    auto copyHwInfo = device->getNEODevice()->getHardwareInfo();
    auto compilerProductHelper = NEO::CompilerProductHelper::get(copyHwInfo.platform.eProductFamily);

    compilerProductHelper->adjustHwInfoForIgc(copyHwInfo);

    zebin.elfHeader->machine = copyHwInfo.platform.eProductFamily;
    L0::ModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.createFromNativeBinary(reinterpret_cast<const char *>(zebin.storage.data()), zebin.storage.size());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto expectedOptions = " " + NEO::CompilerOptions::allowZebin.str();
    EXPECT_STREQ(expectedOptions.c_str(), moduleTu.options.c_str());
}

TEST_F(ModuleTranslationUnitTest, WhenCreatingFromZeBinaryAndGlobalsAreExportedThenTheirAllocationTypeIsUSMDevice) {
    std::string zeInfo = std::string("version :\'") + versionToString(zeInfoDecoderVersion) + R"===('
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
    EXPECT_EQ(retVal, ZE_RESULT_SUCCESS);
    EXPECT_EQ(AllocationType::BUFFER, moduleTu.globalConstBuffer->getAllocationType());
    EXPECT_EQ(AllocationType::BUFFER, moduleTu.globalVarBuffer->getAllocationType());

    auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto globalConstBufferAllocType = svmAllocsManager->getSVMAlloc(reinterpret_cast<void *>(moduleTu.globalConstBuffer->getGpuAddress()))->memoryType;
    auto globalVarBufferAllocType = svmAllocsManager->getSVMAlloc(reinterpret_cast<void *>(moduleTu.globalVarBuffer->getGpuAddress()))->memoryType;
    EXPECT_EQ(DEVICE_UNIFIED_MEMORY, globalConstBufferAllocType);
    EXPECT_EQ(DEVICE_UNIFIED_MEMORY, globalVarBufferAllocType);
}

HWTEST_F(ModuleTranslationUnitTest, WithNoCompilerWhenCallingBuildFromSpirvThenFailureReturned) {
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(nullptr);
    auto oldFclDllName = Os::frontEndDllName;
    auto oldIgcDllName = Os::igcDllName;
    Os::frontEndDllName = "_invalidFCL";
    Os::igcDllName = "_invalidIGC";

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.options = "abcd";

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    Os::igcDllName = oldIgcDllName;
    Os::frontEndDllName = oldFclDllName;
}

HWTEST_F(ModuleTranslationUnitTest, WithNoCompilerWhenCallingCompileGenBinaryThenFailureReturned) {
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(nullptr);
    auto oldFclDllName = Os::frontEndDllName;
    auto oldIgcDllName = Os::igcDllName;
    Os::frontEndDllName = "_invalidFCL";
    Os::igcDllName = "_invalidIGC";

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.options = "abcd";

    ze_result_t result = ZE_RESULT_SUCCESS;
    TranslationInput inputArgs = {IGC::CodeType::spirV, IGC::CodeType::oclGenBin};
    result = moduleTu.compileGenBinary(inputArgs, false);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    Os::igcDllName = oldIgcDllName;
    Os::frontEndDllName = oldFclDllName;
}

HWTEST_F(ModuleTranslationUnitTest, WithNoCompilerWhenCallingStaticLinkSpirVThenFailureReturned) {
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(nullptr);
    auto oldFclDllName = Os::frontEndDllName;
    auto oldIgcDllName = Os::igcDllName;
    Os::frontEndDllName = "_invalidFCL";
    Os::igcDllName = "_invalidIGC";

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.options = "abcd";

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<const char *> inputSpirVs;
    std::vector<uint32_t> inputModuleSizes;
    std::vector<const ze_module_constants_t *> specConstants;
    result = moduleTu.staticLinkSpirV(inputSpirVs, inputModuleSizes, "", "", specConstants);
    EXPECT_EQ(result, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    Os::igcDllName = oldIgcDllName;
    Os::frontEndDllName = oldFclDllName;
}

HWTEST_F(ModuleTranslationUnitTest, WhenCreatingFromZeBinaryWithoutSpirvDataIncludedAndLegacyDebuggerAttachedThenReturnError) {
    if (false == device->getHwInfo().capabilityTable.debuggerSupported) {
        GTEST_SKIP();
    }
    ZebinTestData::ValidEmptyProgram<> zebin;
    const auto &hwInfo = device->getNEODevice()->getHardwareInfo();
    zebin.elfHeader->machine = hwInfo.platform.eProductFamily;

    neoDevice->executionEnvironment->rootDeviceEnvironments[mockRootDeviceIndex]->debugger.reset(new MockActiveSourceLevelDebugger);
    EXPECT_NE(nullptr, neoDevice->getSourceLevelDebugger());

    MockModuleTU moduleTu{device};
    ASSERT_EQ(0u, moduleTu.irBinarySize);
    auto result = moduleTu.createFromNativeBinary(reinterpret_cast<const char *>(zebin.storage.data()), zebin.storage.size());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, result);
}

HWTEST_F(ModuleTranslationUnitTest, WhenBuildOptionsAreNullThenReuseExistingOptions) {

    auto *pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);

    L0::ModuleTranslationUnit moduleTu(this->device);
    moduleTu.options = "abcd";
    pMockCompilerInterface->failBuild = true;

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_NE(result, ZE_RESULT_SUCCESS);
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
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
}

HWTEST_F(ModuleTranslationUnitTest, givenInternalOptionsThenLSCCachePolicyIsSet) {
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    const auto &compilerProductHelper = rootDeviceEnvironment->getHelper<CompilerProductHelper>();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    auto expectedPolicy = compilerProductHelper.getCachingPolicyOptions(false);
    if (expectedPolicy != nullptr) {
        EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find(expectedPolicy), std::string::npos);
    } else {
        EXPECT_EQ(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default"), std::string::npos);
        EXPECT_EQ(pMockCompilerInterface->inputInternalOptions.find("-cl-load-cache-default"), std::string::npos);
    }
}

HWTEST2_F(ModuleTranslationUnitTest, givenDebugFlagSetToWbWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default=7 -cl-load-cache-default=4"), std::string::npos);
}

HWTEST2_F(ModuleTranslationUnitTest, givenDebugFlagSetForceAllResourcesUncachedWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    DebugManager.flags.ForceAllResourcesUncached.set(true);
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default=1 -cl-load-cache-default=1"), std::string::npos);
}

HWTEST2_F(ModuleTranslationUnitTest, givenAtLeastXeHpgCoreWhenGetInternalOptionsThenCorrectBuildOptionIsSet, IsAtLeastXeHpgCore) {
    auto pMockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = this->neoDevice->executionEnvironment->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(pMockCompilerInterface);
    MockModuleTranslationUnit moduleTu(this->device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(pMockCompilerInterface->inputInternalOptions.find("-cl-store-cache-default=2 -cl-load-cache-default=4"), std::string::npos);
}

HWTEST_F(ModuleTranslationUnitTest, givenForceToStatelessRequiredWhenBuildingModuleThen4GbBuffersAreRequired) {
    auto mockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    MockModuleTranslationUnit moduleTu(device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    const auto &compilerProductHelper = rootDeviceEnvironment->getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        EXPECT_NE(mockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
    } else {
        EXPECT_EQ(mockCompilerInterface->inputInternalOptions.find("cl-intel-greater-than-4GB-buffer-required"), std::string::npos);
    }
}

HWTEST2_F(ModuleTranslationUnitTest, givenSourceLevelDebuggerAndAllowZebinBuildOptionWhenBuildWithSpirvThenModuleBuildFails, IsAtMostGen12lp) {

    if (device->getHwInfo().capabilityTable.debuggerSupported == false) {
        GTEST_SKIP();
    }

    auto mockCompilerInterface = new MockCompilerInterface;
    neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(new MockActiveSourceLevelDebugger);
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    MockModuleTranslationUnit moduleTu(device);
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto buildOption = NEO::CompilerOptions::allowZebin.str();

    result = moduleTu.buildFromSpirV("", 0U, buildOption.c_str(), "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_MODULE_BUILD_FAILURE);
}

HWTEST2_F(ModuleTranslationUnitTest, givenSourceLevelDebuggerAndAllowZebinBuildOptionWhenBuildWithSpirvThenModuleBuildsWithSuccess, IsAtLeastXeHpCore) {

    if (device->getHwInfo().capabilityTable.debuggerSupported == false) {
        GTEST_SKIP();
    }

    auto mockCompilerInterface = new MockCompilerInterface;
    neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(new MockActiveSourceLevelDebugger);
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    MockModuleTranslationUnit moduleTu(device);
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto buildOption = NEO::CompilerOptions::allowZebin.str();

    result = moduleTu.buildFromSpirV("", 0U, buildOption.c_str(), "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(mockCompilerInterface->receivedApiOptions.find(NEO::CompilerOptions::allowZebin.str()), std::string::npos);
    EXPECT_EQ(mockCompilerInterface->inputInternalOptions.find(NEO::CompilerOptions::disableZebin.str()), std::string::npos);
}

HWTEST_F(ModuleTranslationUnitTest, givenAllowZebinBuildOptionWhenBuildWithSpirvThenOptionsContainsAllowZebin) {
    auto mockCompilerInterface = new MockCompilerInterface;
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    MockModuleTranslationUnit moduleTu(device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    auto buildOption = NEO::CompilerOptions::allowZebin.str();

    result = moduleTu.buildFromSpirV("", 0U, buildOption.c_str(), "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_NE(mockCompilerInterface->receivedApiOptions.find(NEO::CompilerOptions::allowZebin.str()), std::string::npos);
    EXPECT_EQ(mockCompilerInterface->inputInternalOptions.find(NEO::CompilerOptions::disableZebin.str()), std::string::npos);
}

HWTEST_F(ModuleTranslationUnitTest, givenSourceLevelDebuggerWhenBuildWithSpirvThenModuleBuildsWithSuccess) {

    if (device->getHwInfo().capabilityTable.debuggerSupported == false) {
        GTEST_SKIP();
    }

    auto mockCompilerInterface = new MockCompilerInterface;
    neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(new MockActiveSourceLevelDebugger);
    auto &rootDeviceEnvironment = neoDevice->executionEnvironment->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()];
    rootDeviceEnvironment->compilerInterface.reset(mockCompilerInterface);

    MockModuleTranslationUnit moduleTu(device);
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;

    result = moduleTu.buildFromSpirV("", 0U, nullptr, "", nullptr);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

TEST(ModuleBuildLog, WhenGreaterBufferIsPassedToGetStringThenOutputSizeIsOverridden) {
    const auto infoLog{"[INFO] This is a log!"};
    const auto infoLogLength{strlen(infoLog)};
    const auto moduleBuildLog{ModuleBuildLog::create()};
    moduleBuildLog->appendString(infoLog, infoLogLength);

    size_t buildLogSize{0};
    const auto querySizeResult{moduleBuildLog->getString(&buildLogSize, nullptr)};
    EXPECT_EQ(ZE_RESULT_SUCCESS, querySizeResult);
    EXPECT_EQ(infoLogLength + 1, buildLogSize);

    const auto bufferSize{buildLogSize + 100};
    std::string buffer(bufferSize, '\0');

    buildLogSize = bufferSize;
    const auto queryBuildLogResult{moduleBuildLog->getString(&buildLogSize, buffer.data())};
    EXPECT_EQ(ZE_RESULT_SUCCESS, queryBuildLogResult);

    EXPECT_GT(bufferSize, buildLogSize);
    EXPECT_EQ(infoLogLength + 1, buildLogSize);
    EXPECT_STREQ(infoLog, buffer.c_str());

    const auto destroyResult{moduleBuildLog->destroy()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, destroyResult);
}

TEST(ModuleBuildLog, WhenTooSmallBufferIsPassedToGetStringThenErrorIsReturned) {
    const auto sampleLog{"Sample log!"};
    const auto moduleBuildLog{ModuleBuildLog::create()};
    moduleBuildLog->appendString(sampleLog, strlen(sampleLog));

    std::array<char, 4> buffer{};
    size_t buildLogSize{buffer.size()};

    const auto queryBuildLogResult{moduleBuildLog->getString(&buildLogSize, buffer.data())};
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, queryBuildLogResult);

    const auto destroyResult{moduleBuildLog->destroy()};
    EXPECT_EQ(ZE_RESULT_SUCCESS, destroyResult);
}

using PrintfModuleTest = Test<DeviceFixture>;

HWTEST_F(PrintfModuleTest, GivenModuleWithPrintfWhenKernelIsCreatedThenPrintfAllocationIsPlacedInResidencyContainer) {
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto module = std::unique_ptr<L0::Module>(Module::create(device, &moduleDesc, nullptr, ModuleType::User, &result));

    auto kernel = std::make_unique<Mock<Kernel>>();
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

TEST_F(ModuleTest, givenEnableLibraryCompileThenIsFunctionSymbolExportEnabledTrue) {
    auto module = std::make_unique<Module>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions(BuildOptions::enableLibraryCompile.str().c_str(), buildOptions, internalBuildOptions);

    EXPECT_TRUE(NEO::CompilerOptions::contains(buildOptions, BuildOptions::enableLibraryCompile));
    EXPECT_TRUE(module->isFunctionSymbolExportEnabled);
}

TEST_F(ModuleTest, givenEnableLibraryCompileThenIsGlobalSymbolExportEnabledTrue) {
    auto module = std::make_unique<Module>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string buildOptions;
    std::string internalBuildOptions;

    module->createBuildOptions(BuildOptions::enableGlobalVariableSymbols.str().c_str(), buildOptions, internalBuildOptions);

    EXPECT_TRUE(NEO::CompilerOptions::contains(buildOptions, BuildOptions::enableGlobalVariableSymbols));
    EXPECT_TRUE(module->isGlobalSymbolExportEnabled);
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

TEST_F(ModuleTest, givenSrcOptLevelInSrcNamesWhenMovingBuildOptionsThenOptionIsRemovedFromSrcNamesAndTranslatedOptionsStoredInDstNames) {
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::optLevel);
    srcNames += "=2";
    std::string dstNames;

    auto result = module->moveOptLevelOption(dstNames, srcNames);
    EXPECT_TRUE(result);

    EXPECT_EQ(NEO::CompilerOptions::optLevel.str() + std::string("2"), dstNames);
    EXPECT_EQ(std::string::npos, srcNames.find(BuildOptions::optLevel.str()));
    EXPECT_EQ(std::string::npos, srcNames.find(std::string("=2")));
}

TEST_F(ModuleTest, givenSrcOptLevelWithoutLevelIntegerInSrcNamesWhenMovingBuildOptionsThenFalseIsReturned) {
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::optLevel);
    std::string dstNames;

    auto result = module->moveOptLevelOption(dstNames, srcNames);
    EXPECT_FALSE(result);

    ASSERT_NE(std::string::npos, srcNames.find(BuildOptions::optLevel.str()));
    EXPECT_EQ(std::string::npos, dstNames.find(NEO::CompilerOptions::optLevel.str()));
}

TEST_F(ModuleTest, givenSrcProfileFlagsInSrcNamesWhenMovingBuildOptionsThenOptionIsRemovedFromSrcNamesAndTranslatedOptionsStoredInDstNames) {
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::profileFlags);
    srcNames += " 2";
    std::string dstNames;

    auto result = module->moveProfileFlagsOption(dstNames, srcNames);
    EXPECT_TRUE(result);

    EXPECT_EQ(BuildOptions::profileFlags.str() + std::string(" 2"), dstNames);
    EXPECT_EQ(std::string::npos, srcNames.find(BuildOptions::profileFlags.str()));
    EXPECT_EQ(std::string::npos, srcNames.find(std::string(" 2")));
}

TEST_F(ModuleTest, givenSrcProfileFlagsWithoutFlagValueInSrcNamesWhenMovingBuildOptionsThenFalseIsReturned) {
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    ASSERT_NE(nullptr, module);

    std::string srcNames = NEO::CompilerOptions::concatenate(NEO::CompilerOptions::fastRelaxedMath, BuildOptions::profileFlags);
    std::string dstNames;

    auto result = module->moveProfileFlagsOption(dstNames, srcNames);
    EXPECT_FALSE(result);

    ASSERT_NE(std::string::npos, srcNames.find(BuildOptions::profileFlags.str()));
    EXPECT_EQ(std::string::npos, dstNames.find(BuildOptions::profileFlags.str()));
}

TEST_F(ModuleTest, GivenInjectInternalBuildOptionsWhenBuildingUserModuleThenInternalOptionsAreAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set(" -abc");

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

TEST_F(ModuleTest, GivenInjectApiBuildOptionsWhenBuildingUserModuleThenApiOptionsAreAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectApiBuildOptions.set(" -abc");

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

    EXPECT_TRUE(CompilerOptions::contains(cip->buildOptions, "-abc"));
};

TEST_F(ModuleTest, GivenInjectInternalBuildOptionsWhenBuildingBuiltinModuleThenInternalOptionsAreNotAppended) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.InjectInternalBuildOptions.set(" -abc");

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
    class MockModuleImp : public ModuleImp {
      public:
        using ModuleImp::isFullyLinked;
        using ModuleImp::ModuleImp;
        using ModuleImp::translationUnit;

        bool linkBinary() override {
            return true;
        }

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
        ze_result_t createFromNativeBinary(const char *input, size_t inputSize) override { return ZE_RESULT_SUCCESS; }
    };

    const auto &compilerProductHelper = neoDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (!compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restorer;
    auto zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
    const auto &src = zebinData->storage;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
    moduleDesc.inputSize = src.size();

    std::array<std::tuple<ze_result_t, bool, ModuleType, int32_t>, 5> testParams = {{
        {ZE_RESULT_SUCCESS, false, ModuleType::Builtin, -1},
        {ZE_RESULT_SUCCESS, true, ModuleType::Builtin, 0},
        {ZE_RESULT_SUCCESS, true, ModuleType::User, 0},
        {ZE_RESULT_SUCCESS, true, ModuleType::Builtin, 1},
        {ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, true, ModuleType::User, 1},
    }};

    for (auto &[expectedResult, isStateful, moduleType, debugKey] : testParams) {
        MockModuleImp module(device, nullptr, moduleType);
        module.translationUnit = std::make_unique<MyMockModuleTU>(device);
        DebugManager.flags.FailBuildProgramWithStatefulAccess.set(debugKey);
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
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});
    module->translationUnit->globalConstBuffer = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});

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
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->symbols["symbol"] = relocatedSymbol;
    module0->isGlobalSymbolExportEnabled = true;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("symbol", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1024u, size);
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(ptr));
}

TEST_F(ModuleTest, givenModuleWithSymbolWhenGettingGlobalPointerThatIsAnInstuctionThenFailureReturned) {
    uint64_t gpuAddress = 0x12345000;

    NEO::SymbolInfo symbolInfo{0, 1024u, SegmentType::Instructions};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->symbols["symbol"] = relocatedSymbol;
    module0->isGlobalSymbolExportEnabled = true;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("symbol", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GLOBAL_NAME, result);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ModuleTest, givenModuleWithoutSymbolsThenFailureReturned) {
    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->isGlobalSymbolExportEnabled = true;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("symbol", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GLOBAL_NAME, result);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ModuleTest, givenModuleWithoutSymbolExportEnabledThenFailureReturned) {
    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->isGlobalSymbolExportEnabled = false;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("symbol", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
    EXPECT_EQ(0u, size);
    EXPECT_EQ(nullptr, ptr);
}

TEST_F(ModuleTest, givenModuleWithSymbolWhenGettingGlobalPointerWithNullptrInputsThenSuccessIsReturned) {
    uint64_t gpuAddress = 0x12345000;

    NEO::SymbolInfo symbolInfo{0, 1024u, SegmentType::GlobalVariables};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<Module>(device, nullptr, ModuleType::User);
    module0->symbols["symbol"] = relocatedSymbol;
    module0->isGlobalSymbolExportEnabled = true;

    auto result = module0->getGlobalPointer("symbol", nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(ModuleTest, givenModuleWithGlobalSymbolMapWhenGettingGlobalPointerByHostSymbolNameExistingInMapThenCorrectPointerAndSuccessIsReturned) {
    std::unordered_map<std::string, std::string> mapping;
    mapping["devSymbolOne"] = "hostSymbolOne";
    mapping["devSymbolTwo"] = "hostSymbolTwo";

    size_t symbolsSize = 1024u;
    uint64_t globalVarGpuAddress = 0x12345000;
    NEO::SymbolInfo globalVariablesSymbolInfo{0, static_cast<uint32_t>(symbolsSize), SegmentType::GlobalVariables};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> globalVariablesRelocatedSymbol{globalVariablesSymbolInfo, globalVarGpuAddress};

    uint64_t globalConstGpuAddress = 0x12347000;
    NEO::SymbolInfo globalConstantsSymbolInfo{0, static_cast<uint32_t>(symbolsSize), SegmentType::GlobalConstants};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> globalConstansRelocatedSymbol{globalConstantsSymbolInfo, globalConstGpuAddress};

    auto module0 = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    module0->symbols["devSymbolOne"] = globalVariablesRelocatedSymbol;
    module0->symbols["devSymbolTwo"] = globalConstansRelocatedSymbol;

    auto success = module0->populateHostGlobalSymbolsMap(mapping);
    EXPECT_TRUE(success);
    EXPECT_TRUE(module0->getTranslationUnit()->buildLog.empty());
    module0->isGlobalSymbolExportEnabled = true;

    size_t size = 0;
    void *ptr = nullptr;
    auto result = module0->getGlobalPointer("hostSymbolOne", &size, &ptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(symbolsSize, size);
    EXPECT_EQ(globalVarGpuAddress, reinterpret_cast<uint64_t>(ptr));

    result = module0->getGlobalPointer("hostSymbolTwo", &size, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(symbolsSize, size);
    EXPECT_EQ(globalConstGpuAddress, reinterpret_cast<uint64_t>(ptr));
}

TEST_F(ModuleTest, givenModuleWithGlobalSymbolsMapWhenPopulatingMapWithSymbolNotFoundInRelocatedSymbolsMapThenPrintErrorStringAndReturnFalse) {
    std::unordered_map<std::string, std::string> mapping;
    std::string notFoundDevSymbolName = "anotherDevSymbolOne";
    mapping[notFoundDevSymbolName] = "anotherHostSymbolOne";

    auto module0 = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    EXPECT_EQ(0u, module0->symbols.count(notFoundDevSymbolName));

    auto result = module0->populateHostGlobalSymbolsMap(mapping);
    EXPECT_FALSE(result);
    std::string expectedErrorOutput = "Error: No symbol found with given device name: " + notFoundDevSymbolName + ".\n";
    EXPECT_STREQ(expectedErrorOutput.c_str(), module0->getTranslationUnit()->buildLog.c_str());
}

TEST_F(ModuleTest, givenModuleWithGlobalSymbolsMapWhenPopulatingMapWithSymbolFromIncorrectSegmentThenPrintErrorStringAndReturnFalse) {
    std::unordered_map<std::string, std::string> mapping;
    std::string incorrectDevSymbolName = "incorrectSegmentDevSymbolOne";
    mapping[incorrectDevSymbolName] = "incorrectSegmentHostSymbolOne";

    size_t symbolSize = 1024u;
    uint64_t gpuAddress = 0x12345000;
    NEO::SymbolInfo symbolInfo{0, static_cast<uint32_t>(symbolSize), SegmentType::Instructions};
    NEO::Linker::RelocatedSymbol<NEO::SymbolInfo> relocatedSymbol{symbolInfo, gpuAddress};

    auto module0 = std::make_unique<MockModule>(device, nullptr, ModuleType::User);
    module0->symbols[incorrectDevSymbolName] = relocatedSymbol;

    auto result = module0->populateHostGlobalSymbolsMap(mapping);
    EXPECT_FALSE(result);
    std::string expectedErrorOutput = "Error: Symbol with given device name: " + incorrectDevSymbolName + " is not in .data segment.\n";
    EXPECT_STREQ(expectedErrorOutput.c_str(), module0->getTranslationUnit()->buildLog.c_str());
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

    NEO::Linker::PatchableSegments segments{{data, 0u, 1, std::string{}}};

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
    linkerInput->textRelocations.push_back({{".str", 0x8, LinkerInput::RelocationInfo::Type::Address, SegmentType::Instructions}});
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

TEST_F(ModuleTests, givenImplicitArgsRelocationAndStackCallsWhenLinkingBuiltinModuleThenSegmentIsNotPatchedAndImplicitArgsAreNotRequired) {
    auto pModule = std::make_unique<WhiteBox<Module>>(device, nullptr, ModuleType::Builtin);

    pModule->maxGroupSize = 32;

    char data[64]{};
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, nullptr, nullptr, true);

    kernelImmData->kernelDescriptor->kernelAttributes.flags.useStackCalls = true;
    auto isaCpuPtr = reinterpret_cast<char *>(kernelImmData->isaGraphicsAllocation->getUnderlyingBuffer());
    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);
    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);

    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelInfo->kernelDescriptor.kernelMetadata.kernelName.c_str();

    ze_result_t res = pModule->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(0u, *reinterpret_cast<uint32_t *>(ptrOffset(isaCpuPtr, 0x8)));
    Kernel::fromHandle(kernelHandle)->destroy();
}

TEST_F(ModuleTests, givenFullyLinkedModuleAndSlmSizeExceedingLocalMemorySizeWhenCreatingKernelThenDebugMsgErrIsPrintedAndOutOfDeviceMemoryIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto pModule = std::make_unique<WhiteBox<Module>>(device, nullptr, ModuleType::Builtin);
    pModule->maxGroupSize = 32;

    char data[64]{};
    std::unique_ptr<KernelInfo> kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    auto localMemSize = static_cast<uint32_t>(this->device->getNEODevice()->getDeviceInfo().localMemSize);
    kernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize = localMemSize + 10u;
    auto slmInlineSizeCopy = kernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, true);

    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    ::testing::internal::CaptureStderr();

    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = pModule->translationUnit->programInfo.kernelInfos[0]->kernelDescriptor.kernelMetadata.kernelName.c_str();

    ze_result_t res = pModule->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    std::string output = testing::internal::GetCapturedStderr();
    const std::string expectedPart = "Size of SLM (" + std::to_string(slmInlineSizeCopy) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_TRUE(output.find(expectedPart));

    Kernel::fromHandle(kernelHandle)->destroy();
}

TEST_F(ModuleTests, givenFullyLinkedModuleWhenCreatingKernelThenDebugMsgOnPrivateAndScratchUsageIsPrinted) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    auto pModule = std::make_unique<WhiteBox<Module>>(device, nullptr, ModuleType::Builtin);
    pModule->maxGroupSize = 32;

    char data[64]{};
    std::unique_ptr<KernelInfo> kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->heapInfo.KernelHeapSize = 64;
    kernelInfo->heapInfo.pKernelHeap = data;

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo.get(), device, 0, nullptr, nullptr, true);

    pModule->kernelImmDatas.push_back(std::move(kernelImmData));
    pModule->translationUnit->programInfo.kernelInfos.push_back(kernelInfo.release());
    auto linkerInput = std::make_unique<::WhiteBox<NEO::LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    ::testing::internal::CaptureStderr();

    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = pModule->translationUnit->programInfo.kernelInfos[0]->kernelDescriptor.kernelMetadata.kernelName.c_str();

    ze_result_t res = pModule->createKernel(&kernelDesc, &kernelHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    std::string output = testing::internal::GetCapturedStderr();
    std::ostringstream expectedOutput;
    expectedOutput << "computeUnits for each thread: " << std::to_string(this->device->getDeviceInfo().computeUnitsUsedForScratch) << "\n"
                   << "perHwThreadPrivateMemorySize: 0\t totalPrivateMemorySize: 0\n"
                   << "perHwThreadScratchSize: 0\t totalScratchSize: 0\n"
                   << "perHwThreadPrivateScratchSize: 0\t totalPrivateScratchSize: 0\n";
    EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());

    Kernel::fromHandle(kernelHandle)->destroy();
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
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
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
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
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
    linkerInput->textRelocations.push_back({{implicitArgsRelocationSymbolName, 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    pModule->translationUnit->programInfo.linkerInput = std::move(linkerInput);

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    auto status = pModule->linkBinary();
    EXPECT_TRUE(status);

    EXPECT_EQ(0u, *reinterpret_cast<uint32_t *>(ptrOffset(isaCpuPtr, 0x8)));

    EXPECT_FALSE(kernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(ModuleTests, givenModuleWithGlobalAndConstAllocationsWhenGettingModuleAllocationsThenAllAreReturned) {
    std::unique_ptr<MockModule> module = std::make_unique<MockModule>(device,
                                                                      nullptr,
                                                                      ModuleType::User);
    module->translationUnit = std::make_unique<MockModuleTranslationUnit>(device);

    module->translationUnit->globalVarBuffer = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});
    module->translationUnit->globalConstBuffer = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), MemoryConstants::pageSize, NEO::AllocationType::BUFFER, neoDevice->getDeviceBitfield()});

    uint32_t kernelHeap = 0;
    auto kernelInfo = new KernelInfo();
    kernelInfo->heapInfo.KernelHeapSize = 1;
    kernelInfo->heapInfo.pKernelHeap = &kernelHeap;

    // pass kernelInfo ownership to programInfo
    module->translationUnit->programInfo.kernelInfos.push_back(kernelInfo);

    std::unique_ptr<WhiteBox<::L0::KernelImmutableData>> kernelImmData{new WhiteBox<::L0::KernelImmutableData>(this->device)};
    kernelImmData->initialize(kernelInfo, device, 0, module->translationUnit->globalConstBuffer, module->translationUnit->globalVarBuffer, false);
    module->kernelImmDatas.push_back(std::move(kernelImmData));

    const auto allocs = module->getModuleAllocations();

    EXPECT_EQ(3u, allocs.size());

    auto iter = std::find(allocs.begin(), allocs.end(), module->translationUnit->globalConstBuffer);
    EXPECT_NE(allocs.end(), iter);

    iter = std::find(allocs.begin(), allocs.end(), module->translationUnit->globalVarBuffer);
    EXPECT_NE(allocs.end(), iter);

    iter = std::find(allocs.begin(), allocs.end(), module->kernelImmDatas[0]->getIsaGraphicsAllocation());
    EXPECT_NE(allocs.end(), iter);
}

using ModuleIsaCopyTest = Test<ModuleImmutableDataFixture>;

TEST_F(ModuleIsaCopyTest, whenModuleIsInitializedThenIsaIsCopied) {
    MockImmutableMemoryManager *mockMemoryManager = static_cast<MockImmutableMemoryManager *>(device->getNEODevice()->getMemoryManager());

    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;
    bool isInternal = false;

    std::unique_ptr<MockImmutableData> mockKernelImmData = std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    uint32_t previouscopyMemoryToAllocationCalledTimes = mockMemoryManager->copyMemoryToAllocationCalledTimes;

    auto additionalSections = {ZebinTestData::appendElfAdditionalSection::GLOBAL};
    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, isInternal, mockKernelImmData.get(), additionalSections);

    uint32_t numOfKernels = static_cast<uint32_t>(module->getKernelImmutableDataVector().size());
    const uint32_t numOfGlobalBuffers = 1;

    EXPECT_EQ(previouscopyMemoryToAllocationCalledTimes + numOfGlobalBuffers + numOfKernels, mockMemoryManager->copyMemoryToAllocationCalledTimes);

    for (auto &kid : module->getKernelImmutableDataVector()) {
        EXPECT_TRUE(kid->isIsaCopiedToAllocation());
    }
}

using ModuleWithZebinTest = Test<ModuleWithZebinFixture>;
TEST_F(ModuleWithZebinTest, givenNoZebinThenSegmentsAreEmpty) {
    auto segments = module->getZebinSegments();

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.constData.address);
    EXPECT_EQ(0ULL, segments.constData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.varData.address);
    EXPECT_EQ(0ULL, segments.varData.size);

    EXPECT_EQ(std::numeric_limits<uintptr_t>::max(), segments.stringData.address);
    EXPECT_EQ(0ULL, segments.stringData.size);

    EXPECT_TRUE(segments.nameToSegMap.empty());
}

TEST_F(ModuleWithZebinTest, givenZebinSegmentsThenSegmentsArePopulated) {
    module->addSegments();
    auto segments = module->getZebinSegments();

    auto checkGPUSeg = [](NEO::GraphicsAllocation *alloc, NEO::Debug::Segments::Segment segment) {
        EXPECT_EQ(alloc->getGpuAddress(), segment.address);
        EXPECT_EQ(alloc->getUnderlyingBufferSize(), segment.size);
    };
    checkGPUSeg(module->translationUnit->globalConstBuffer, segments.constData);
    checkGPUSeg(module->translationUnit->globalConstBuffer, segments.varData);
    checkGPUSeg(module->kernelImmDatas[0]->getIsaGraphicsAllocation(), segments.nameToSegMap[ZebinTestData::ValidEmptyProgram<>::kernelName]);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(module->translationUnit->programInfo.globalStrings.initData), segments.stringData.address);
    EXPECT_EQ(module->translationUnit->programInfo.globalStrings.size, segments.stringData.size);
}

TEST_F(ModuleWithZebinTest, givenValidZebinWhenGettingDebugInfoThenDebugZebinIsCreatedAndReturned) {
    module->addEmptyZebin();
    module->addSegments();
    size_t debugDataSize;
    module->getDebugInfo(&debugDataSize, nullptr);
    auto debugData = std::make_unique<uint8_t[]>(debugDataSize);
    ze_result_t retCode = module->getDebugInfo(&debugDataSize, debugData.get());
    ASSERT_NE(nullptr, module->translationUnit->debugData.get());
    EXPECT_EQ(0, memcmp(module->translationUnit->debugData.get(), debugData.get(), debugDataSize));
    EXPECT_EQ(retCode, ZE_RESULT_SUCCESS);
}

TEST_F(ModuleWithZebinTest, givenValidZebinAndPassedDataSmallerThanDebugDataThenErrorIsReturned) {
    module->addEmptyZebin();
    module->addSegments();
    size_t debugDataSize;
    module->getDebugInfo(&debugDataSize, nullptr);
    auto debugData = std::make_unique<uint8_t[]>(debugDataSize);
    debugDataSize = 0;
    ze_result_t errorCode = module->getDebugInfo(&debugDataSize, debugData.get());
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, errorCode);
}

TEST_F(ModuleWithZebinTest, givenNonZebinaryFormatWhenGettingDebugInfoThenDebugZebinIsNotCreated) {
    size_t mockProgramSize = sizeof(Elf::ElfFileHeader<Elf::EI_CLASS_64>);
    module->translationUnit->unpackedDeviceBinary = std::make_unique<char[]>(mockProgramSize);
    module->translationUnit->unpackedDeviceBinarySize = mockProgramSize;
    module->isZebinBinary = false;
    size_t debugDataSize;
    ze_result_t retCode = module->getDebugInfo(&debugDataSize, nullptr);
    EXPECT_EQ(debugDataSize, 0u);
    EXPECT_EQ(retCode, ZE_RESULT_SUCCESS);
}

HWTEST_F(ModuleWithZebinTest, givenZebinWithKernelCallingExternalFunctionThenUpdateKernelsBarrierCount) {
    ZebinTestData::ZebinWithExternalFunctionsInfo zebin;

    auto copyHwInfo = device->getHwInfo();
    auto compilerProductHelper = NEO::CompilerProductHelper::get(copyHwInfo.platform.eProductFamily);

    compilerProductHelper->adjustHwInfoForIgc(copyHwInfo);

    zebin.setProductFamily(static_cast<uint16_t>(copyHwInfo.platform.eProductFamily));

    ze_module_desc_t moduleDesc = {};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = zebin.storage.data();
    moduleDesc.inputSize = zebin.storage.size();

    ModuleBuildLog *moduleBuildLog = nullptr;

    auto module = std::unique_ptr<L0::ModuleImp>(new L0::ModuleImp(device, moduleBuildLog, ModuleType::User));
    ASSERT_NE(nullptr, module.get());
    ze_result_t result = ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    result = module->initialize(&moduleDesc, device->getNEODevice());
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    const auto &kernImmData = module->getKernelImmutableData("kernel");
    ASSERT_NE(nullptr, kernImmData);
    EXPECT_EQ(zebin.barrierCount, kernImmData->getDescriptor().kernelAttributes.barrierCount);
}
} // namespace ult
} // namespace L0
