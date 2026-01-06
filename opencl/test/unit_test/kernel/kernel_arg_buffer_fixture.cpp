/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kernel_arg_buffer_fixture.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

void KernelArgBufferFixture::setUp() {
    ClDeviceFixture::setUp();
    cl_device_id device = pClDevice;
    ContextFixture::setUp(1, &device);

    // define kernel info
    pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->heapInfo.pSsh = pSshLocal;
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(pSshLocal);

    pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *), 0x0);

    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = debugManager.flags.UseBindlessMode.get() == 1 ? KernelDescriptor::AddressingMode::BindlessAndStateless : KernelDescriptor::AddressingMode::BindfulAndStateless;

    pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

    pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);
}

void KernelArgBufferFixture::tearDown() {
    delete pKernel;

    delete pProgram;
    ContextFixture::tearDown();
    ClDeviceFixture::tearDown();
}
