/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

using XeHPComputeWorkgroupSizeTest = Test<ClDeviceFixture>;

XEHPTEST_F(XeHPComputeWorkgroupSizeTest, givenXeHPAndForceWorkgroupSize1x1x1FlagWhenComputeWorkgroupSizeIsCalledThenExpectedLwsIsReturned) {
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
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(pDevice->isSimulation());

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{128, 128, 128};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pClDevice, &kernel, 3, gws, elws, offset};

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
        hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
        hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_EQ(1u, expectedLws.x * expectedLws.y * expectedLws.z);
        DebugManager.flags.ForceLocalMemoryAccessMode.set(-1);
    }

    {
        hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        hwInfo->platform.usRevId = hwHelper.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
        hwInfo->featureTable.ftrSimulationMode = true;
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
        EXPECT_TRUE(pDevice->isSimulation());
    }
    {
        hwInfo->featureTable.ftrSimulationMode = false;
        kernel.setAuxTranslationDirection(AuxTranslationDirection::NonAuxToAux);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.clear();
        kernelInfo.addArgImage(0, 0);
        kernel.setAuxTranslationDirection(AuxTranslationDirection::None);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
    {
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.clear();
        kernelInfo.addArgImmediate(0);
        auto expectedLws = computeWorkgroupSize(dispatchInfo);
        EXPECT_NE(1u, expectedLws.x * expectedLws.y * expectedLws.z);
    }
}
XEHPTEST_F(XeHPComputeWorkgroupSizeTest, giveXeHpWhenKernelIsaIsBelowThresholdAndThereAreNoImageBarriersAndSlmThenSmallWorkgorupSizeIsSelected) {
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

    MockKernelWithInternals mockKernel(*pClDevice);
    auto &kernel = *mockKernel.mockKernel;

    kernel.initialize();

    Vec3<size_t> elws{0, 0, 0};
    Vec3<size_t> gws{1024, 1, 1};
    Vec3<size_t> offset{0, 0, 0};
    DispatchInfo dispatchInfo{pClDevice, &kernel, 1, gws, elws, offset};

    auto maxWgSize = pClDevice->getSharedDeviceInfo().maxWorkGroupSize;

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
    pClDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = REVISION_B;
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
