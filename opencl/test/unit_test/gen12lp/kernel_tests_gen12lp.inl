/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using Gen12LpKernelTest = Test<ClDeviceFixture>;
GEN12LPTEST_F(Gen12LpKernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsFalse) {
    MockKernelWithInternals mockKernel(*pClDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_FALSE(retVal);
}

GEN12LPTEST_F(Gen12LpKernelTest, GivenKernelWhenNotUsingSharedObjArgsThenWaDisableRccRhwoOptimizationIsNotRequired) {
    MockKernelWithInternals kernel(*pClDevice);
    EXPECT_FALSE(kernel.mockKernel->requiresWaDisableRccRhwoOptimization());
}

GEN12LPTEST_F(Gen12LpKernelTest, GivenKernelWhenAtLeastOneArgIsMediaCompressedThenWaDisableRccRhwoOptimizationIsRequired) {
    MockKernelWithInternals kernel(*pClDevice);
    kernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(3);
    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.addArgImmediate(1);
    kernel.kernelInfo.addArgBuffer(2);

    kernel.mockKernel->initialize();

    MockBuffer buffer;
    auto allocation = buffer.getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto gmm1 = new MockGmm(pDevice->getGmmClientContext());
    allocation->setGmm(gmm1, 0);

    cl_mem clMem = &buffer;
    kernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);

    uint32_t immediateArg = 0;
    kernel.mockKernel->setArgImmediate(1, sizeof(uint32_t), &immediateArg);

    MockBuffer bufferMediaCompressed;
    bufferMediaCompressed.setSharingHandler(new SharingHandler());
    allocation = bufferMediaCompressed.getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto gmm2 = new MockGmm(pDevice->getGmmClientContext());
    allocation->setGmm(gmm2, 0);
    allocation->getGmm(0)->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = 1;
    cl_mem clMem2 = &bufferMediaCompressed;
    kernel.mockKernel->setArgBuffer(2, sizeof(cl_mem *), &clMem2);

    EXPECT_TRUE(kernel.mockKernel->requiresWaDisableRccRhwoOptimization());
}
