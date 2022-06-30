/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kernel_arg_buffer_fixture.h"

#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

void KernelArgBufferFixture::SetUp() {
    ClDeviceFixture::SetUp();
    cl_device_id device = pClDevice;
    ContextFixture::SetUp(1, &device);

    // define kernel info
    pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->heapInfo.pSsh = pSshLocal;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal);

    pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *), 0x0);

    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = ApiSpecificConfig::getBindlessConfiguration() ? KernelDescriptor::AddressingMode::BindlessAndStateless : KernelDescriptor::AddressingMode::BindfulAndStateless;

    pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

    pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);
}

void KernelArgBufferFixture::TearDown() {
    delete pKernel;

    delete pProgram;
    ContextFixture::TearDown();
    ClDeviceFixture::TearDown();
}
