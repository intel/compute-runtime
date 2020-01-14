/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kernel_arg_buffer_fixture.h"

#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

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

    pProgram = new MockProgram(*pDevice->getExecutionEnvironment(), pContext, false);

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
