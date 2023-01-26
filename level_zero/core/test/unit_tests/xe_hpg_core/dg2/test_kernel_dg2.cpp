/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {
using KernelTestDG2 = Test<ModuleFixture>;

HWTEST2_F(KernelTestDG2, givenKernelImpWhenSetBufferSurfaceStateCalledThenProgramCorrectL1CachePolicy, IsDG2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

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
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceStateAddress->getL1CachePolicyL1CacheControl());

    const_cast<NEO::DeviceInfo &>(device->getDeviceInfo()).debuggerActive = true;
    kernelImp->setBufferSurfaceState(argIndex, devicePtr, gpuAlloc);
    surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP, surfaceStateAddress->getL1CachePolicyL1CacheControl());

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
    const_cast<NEO::DeviceInfo &>(device->getDeviceInfo()).debuggerActive = false;
    kernelImp->setBufferSurfaceState(argIndex, devicePtr, gpuAlloc);
    surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP, surfaceStateAddress->getL1CachePolicyL1CacheControl());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

HWTEST2_F(KernelTestDG2, givenKernelImpWithDpasWhenSuggestingWGSizeThenSizeIsDifferntThanWithoutDpas, IsDG2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    reinterpret_cast<NEO::MockDevice *>(module->getDevice()->getNEODevice())->deviceInfo.maxNumEUsPerSubSlice = 16;
    reinterpret_cast<NEO::MockDevice *>(module->getDevice()->getNEODevice())->deviceInfo.numThreadsPerEU = 8;
    uint32_t groupSizeXDpas = 79u;
    uint32_t groupSizeYDpas = 14;
    uint32_t groupSizeZDpas = 1u;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    kernelImp->suggestGroupSize(groupSizeXDpas, groupSizeYDpas, groupSizeZDpas, &groupSizeXDpas, &groupSizeYDpas, &groupSizeZDpas);
    uint32_t groupSizeXNoDpas = 79u;
    uint32_t groupSizeYNoDpas = 14u;
    uint32_t groupSizeZNoDpas = 1u;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    kernelImp->suggestGroupSize(groupSizeXNoDpas, groupSizeYNoDpas, groupSizeZNoDpas, &groupSizeXNoDpas, &groupSizeYNoDpas, &groupSizeZNoDpas);
    EXPECT_TRUE(groupSizeXDpas != groupSizeXNoDpas || groupSizeYDpas != groupSizeYNoDpas);
    Kernel::fromHandle(kernelHandle)->destroy();
}

HWTEST2_F(KernelTestDG2, givenKernelImpWithFusedEuDisabledWhenSuggestingWGSizeThenSizeIsDifferntThanWithoutDpas, IsDG2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));

    reinterpret_cast<NEO::MockDevice *>(module->getDevice()->getNEODevice())->deviceInfo.maxNumEUsPerSubSlice = 16;
    reinterpret_cast<NEO::MockDevice *>(module->getDevice()->getNEODevice())->deviceInfo.numThreadsPerEU = 8;
    uint32_t groupSizeXEuFusionDisabled = 79u;
    uint32_t groupSizeYEuFusionDisabled = 14;
    uint32_t groupSizeZEuFusionDisabled = 1u;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = true;
    kernelImp->suggestGroupSize(groupSizeXEuFusionDisabled, groupSizeYEuFusionDisabled, groupSizeZEuFusionDisabled, &groupSizeXEuFusionDisabled, &groupSizeYEuFusionDisabled, &groupSizeZEuFusionDisabled);
    uint32_t groupSizeXNoEuFusionDisabled = 79u;
    uint32_t groupSizeYNoEuFusionDisabled = 14;
    uint32_t groupSizeZNoEuFusionDisabled = 1u;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = false;
    kernelImp->suggestGroupSize(groupSizeXNoEuFusionDisabled, groupSizeYNoEuFusionDisabled, groupSizeZNoEuFusionDisabled, &groupSizeXNoEuFusionDisabled, &groupSizeYNoEuFusionDisabled, &groupSizeZNoEuFusionDisabled);
    EXPECT_TRUE(groupSizeXEuFusionDisabled != groupSizeXNoEuFusionDisabled || groupSizeYEuFusionDisabled != groupSizeYNoEuFusionDisabled);
    Kernel::fromHandle(kernelHandle)->destroy();
}

HWTEST2_F(KernelTestDG2, givenKernelImpWithDpasWhenGetPreferredWorkGroupSizeThenReturnedSizeIsLowerThanSizeForKernelWithoutDpas, IsDG2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));
    ze_kernel_properties_t properties = {};
    ze_kernel_preferred_group_size_properties_t extProperties = {};
    extProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PREFERRED_GROUP_SIZE_PROPERTIES;
    properties.pNext = &extProperties;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    kernelImp->getProperties(&properties);
    auto sizeWithDpas = extProperties.preferredMultiple;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    kernelImp->getProperties(&properties);
    auto sizeWithoutDpas = extProperties.preferredMultiple;
    EXPECT_NE(sizeWithDpas, sizeWithoutDpas);
    Kernel::fromHandle(kernelHandle)->destroy();
}

HWTEST2_F(KernelTestDG2, givenKernelImpWithFusedEuDisabledWhenGetPreferredWorkGroupSizeThenReturnedSizeIsLowerThanSizeForKernelWithoutFusedEuEnabled, IsDG2) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    ze_kernel_handle_t kernelHandle;

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = kernelName.c_str();

    ze_result_t res = module->createKernel(&kernelDesc, &kernelHandle);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);

    auto kernelImp = reinterpret_cast<L0::KernelImp *>(L0::Kernel::fromHandle(kernelHandle));
    ze_kernel_properties_t properties = {};
    ze_kernel_preferred_group_size_properties_t extProperties = {};
    extProperties.stype = ZE_STRUCTURE_TYPE_KERNEL_PREFERRED_GROUP_SIZE_PROPERTIES;
    properties.pNext = &extProperties;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = true;
    kernelImp->getProperties(&properties);
    auto sizeWithDpas = extProperties.preferredMultiple;
    const_cast<NEO::KernelDescriptor &>(kernelImp->getImmutableData()->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = false;
    kernelImp->getProperties(&properties);
    auto sizeWithoutDpas = extProperties.preferredMultiple;
    EXPECT_NE(sizeWithDpas, sizeWithoutDpas);
    Kernel::fromHandle(kernelHandle)->destroy();
}

} // namespace ult
} // namespace L0
