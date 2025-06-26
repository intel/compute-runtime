/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

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
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, surfaceStateAddress->getL1CacheControlCachePolicy());

    auto debugger = MockDebuggerL0Hw<FamilyType>::allocate(neoDevice);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->debugger.reset(debugger);
    kernelImp->setBufferSurfaceState(argIndex, devicePtr, gpuAlloc);
    surfaceStateAddressRaw = ptrOffset(kernelImp->getSurfaceStateHeapData(), argInfo.bindful);
    surfaceStateAddress = reinterpret_cast<RENDER_SURFACE_STATE *>(const_cast<unsigned char *>(surfaceStateAddressRaw));
    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP, surfaceStateAddress->getL1CacheControlCachePolicy());

    Kernel::fromHandle(kernelHandle)->destroy();

    context->freeMem(devicePtr);
}

} // namespace ult
} // namespace L0
