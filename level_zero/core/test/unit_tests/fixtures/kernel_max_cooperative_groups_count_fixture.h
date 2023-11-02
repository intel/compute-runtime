/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

class KernelImpSuggestMaxCooperativeGroupCountTests : public Test<DeviceFixture> {
  public:
    const uint32_t numGrf = 128;
    const uint32_t simd = 8;
    const uint32_t lws[3] = {1, 1, 1};
    uint32_t usedSlm = 0;
    uint32_t usesBarriers = 0;

    uint32_t availableThreadCount;
    uint32_t dssCount;
    uint32_t availableSlm;
    uint32_t maxBarrierCount;
    WhiteBox<::L0::KernelImmutableData> kernelInfo;
    NEO::KernelDescriptor kernelDescriptor;

    void SetUp() override {
        Test<DeviceFixture>::SetUp();
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

    uint32_t getMaxWorkGroupCount() {
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
        kernel.KernelImp::suggestMaxCooperativeGroupCount(&totalGroupCount, NEO::EngineGroupType::CooperativeCompute, true);
        return totalGroupCount;
    }
};
} // namespace ult
} // namespace L0