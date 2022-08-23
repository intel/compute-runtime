/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {
using KernelDebugSurfaceDG2Test = Test<ModuleFixture>;

HWTEST2_F(KernelDebugSurfaceDG2Test, givenDebuggerAndBindfulKernelWhenAppendingKernelToCommandListThenCachePolicyIsWBP, IsDG2) {
    NEO::MockCompilerEnableGuard mock(true);
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    const_cast<DeviceInfo &>(neoDevice->getDeviceInfo()).debuggerActive = true;
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

    EXPECT_EQ(RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP, debugSurfaceState->getL1CachePolicyL1CacheControl());
}
} // namespace ult
} // namespace L0