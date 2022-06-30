/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using XeHPComputeWorkgroupSizeTest = Test<ClDeviceFixture>;

XEHPTEST_F(XeHPComputeWorkgroupSizeTest, giveXeHpA0WhenKernelIsaIsBelowThresholdAndThereAreNoImageBarriersAndSlmThenSmallWorkgorupSizeIsSelected) {
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

    MockKernelWithInternals mockKernel(*pClDevice);
    auto &kernel = *mockKernel.mockKernel;

    kernel.initialize();

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{1024, 1, 1};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pClDevice, &kernel, 1, gws, elws, offset};

    auto maxWgSize = pClDevice->getSharedDeviceInfo().maxWorkGroupSize;

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    {
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(64u, expectedLws.x * expectedLws.y * expectedLws.z);
        EXPECT_EQ(64u, expectedLws.x);
    }

    mockKernel.mockKernel->slmTotalSize = 1000u;

    {
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(maxWgSize, expectedLws.x * expectedLws.y * expectedLws.z);
        EXPECT_EQ(maxWgSize, expectedLws.x);
    }

    mockKernel.mockKernel->slmTotalSize = 0u;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1u;

    {
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(maxWgSize, expectedLws.x * expectedLws.y * expectedLws.z);
        EXPECT_EQ(maxWgSize, expectedLws.x);
    }

    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 0u;
    //on B0 algorithm is disabled
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    {
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(maxWgSize, expectedLws.x * expectedLws.y * expectedLws.z);
        EXPECT_EQ(maxWgSize, expectedLws.x);
    }
}

XEHPTEST_F(XeHPComputeWorkgroupSizeTest, givenSmallKernelAndGwsThatIsNotDivisableBySmallerLimitWhenLwsIsComputedThenBigWorgkroupIsSelected) {
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

    MockKernelWithInternals mockKernel(*pClDevice);
    auto &kernel = *mockKernel.mockKernel;

    kernel.initialize();

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{636056, 1, 1};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pClDevice, &kernel, 1, gws, elws, offset};

    auto maxWgSize = pClDevice->getSharedDeviceInfo().maxWorkGroupSize;

    {
        auto calculatedLws = computeWorkgroupSize(dispatchInfo);

        auto expectedLws = (maxWgSize < 344) ? 8u : 344u;

        EXPECT_EQ(expectedLws, calculatedLws.x * calculatedLws.y * calculatedLws.z);
        EXPECT_EQ(expectedLws, calculatedLws.x);
    }
}
