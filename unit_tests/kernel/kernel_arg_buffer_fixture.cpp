/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "CL/cl.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"
#include "kernel_arg_buffer_fixture.h"

#include <memory>

using namespace OCLRT;

void KernelArgBufferFixture::SetUp() {
    DeviceFixture::SetUp();
    cl_device_id device = pDevice;
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

    pKernel = new MockKernel(pProgram, *pKernelInfo, *pDevice);
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
