/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/kernel_max_cooperative_groups_count_fixture.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/module/module_imp.h"

namespace L0 {
namespace ult {
void KernelImpSuggestMaxCooperativeGroupCountFixture::setUp() {
    DeviceFixture::setUp();
    kernelInfo.kernelDescriptor = &kernelDescriptor;
    auto &hardwareInfo = device->getHwInfo();
    auto &helper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    availableThreadCount = helper.calculateAvailableThreadCount(hardwareInfo, numGrf);

    dssCount = hardwareInfo.gtSystemInfo.DualSubSliceCount;
    if (dssCount == 0) {
        dssCount = hardwareInfo.gtSystemInfo.SubSliceCount;
    }
    availableSlm = dssCount * KB * hardwareInfo.capabilityTable.slmSize;
    maxBarrierCount = static_cast<uint32_t>(helper.getMaxBarrierRegisterPerSlice());

    kernelInfo.kernelDescriptor->kernelAttributes.simdSize = simd;
    kernelInfo.kernelDescriptor->kernelAttributes.numGrfRequired = numGrf;
}

uint32_t KernelImpSuggestMaxCooperativeGroupCountFixture::getMaxWorkGroupCount() {
    kernelInfo.kernelDescriptor->kernelAttributes.slmInlineSize = usedSlm;
    kernelInfo.kernelDescriptor->kernelAttributes.barrierCount = usesBarriers;

    Mock<KernelImp> kernel;
    kernel.kernelImmData = &kernelInfo;
    auto module = std::make_unique<ModuleImp>(device, nullptr, ModuleType::User);
    kernel.module = module.get();

    kernel.groupSize[0] = lws[0];
    kernel.groupSize[1] = lws[1];
    kernel.groupSize[2] = lws[2];
    uint32_t totalGroupCount = 0;
    kernel.KernelImp::suggestMaxCooperativeGroupCount(&totalGroupCount, NEO::EngineGroupType::cooperativeCompute, true);
    return totalGroupCount;
}
} // namespace ult
} // namespace L0