/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kernel_arg_buffer_fixture.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

void KernelArgBufferFixture::SetUp() {
    DeviceFixture::SetUp();
    cl_device_id device = pClDevice;
    ContextFixture::SetUp(1, &device);

    // define kernel info
    pKernelInfo = std::make_unique<KernelInfo>();

    // setup kernel arg offsets
    KernelArgPatchInfo kernelArgPatchInfo;

    kernelHeader.SurfaceStateHeapSize = sizeof(pSshLocal);
    pKernelInfo->heapInfo.pSsh = pSshLocal;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    pKernelInfo->kernelArgInfo.resize(1);
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].size = (uint32_t)sizeof(void *);

    pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false, nullptr);

    pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    pKernel->setKernelArgHandler(0, &Kernel::setArgBuffer);
}

void KernelArgBufferFixture::TearDown() {
    delete pKernel;

    delete pProgram;
    ContextFixture::TearDown();
    DeviceFixture::TearDown();
}
