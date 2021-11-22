/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

using XeHpgCoreComputeWorkgroupSizeTest = Test<ClDeviceFixture>;
XE_HPG_CORETEST_F(XeHpgCoreComputeWorkgroupSizeTest, givenForceWorkgroupSize1x1x1FlagWhenComputeWorkgroupSizeIsCalledThenExpectedLwsIsReturned) {
    DebugManagerStateRestore dbgRestore;

    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

    MockKernelWithInternals mockKernel(*pClDevice);

    GraphicsAllocation localMemoryAllocation(0, GraphicsAllocation::AllocationType::BUFFER, nullptr, 123, 456, 789, MemoryPool::LocalMemory);
    MockBuffer buffer(localMemoryAllocation);
    cl_mem clMem = &buffer;
    auto &kernel = *mockKernel.mockKernel;
    auto &kernelInfo = mockKernel.kernelInfo;
    kernelInfo.addArgBuffer(0, 0);
    kernel.isBuiltIn = true;
    kernel.initialize();
    kernel.setArgBuffer(0, sizeof(cl_mem *), &clMem);

    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    EXPECT_FALSE(pDevice->isSimulation());

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pClDevice, &kernel, 3, gws, elws, offset};
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);

    {
        DebugManager.flags.ForceWorkgroupSize1x1x1.set(0);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        DebugManager.flags.ForceWorkgroupSize1x1x1.set(1);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        DebugManager.flags.ForceWorkgroupSize1x1x1.set(-1);
        hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
}
