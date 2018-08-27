/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/built_ins/built_ins.h"
#include "runtime/helpers/preamble.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

using namespace OCLRT;

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

TEST_F(EnqueueKernelTest, givenKernelWithSharedObjArgsWhenEnqueueIsCalledThenResetPatchAddress) {
    auto nonSharedBuffer = new MockBuffer;
    MockGlSharing glSharing;
    glSharing.uploadDataToBufferInfo(1, 0);
    pContext->setSharingFunctions(new GlSharingFunctionsMock());
    auto retVal = CL_SUCCESS;
    auto sharedBuffer = GlBuffer::createSharedGlBuffer(pContext, CL_MEM_READ_WRITE, 1, &retVal);
    auto sharedMem = static_cast<cl_mem>(sharedBuffer);
    auto nonSharedMem = static_cast<cl_mem>(nonSharedBuffer);

    pKernel->setArg(0, sizeof(cl_mem *), &sharedMem);
    pKernel->setArg(1, sizeof(cl_mem *), &nonSharedMem);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
    auto &kernelInfo = pKernel->getKernelInfo();

    auto pKernelArg =
        (uint32_t *)(pKernel->getCrossThreadData() + kernelInfo.kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset);

    auto address1 = static_cast<uint64_t>(*pKernelArg);
    EXPECT_EQ(sharedBuffer->getGraphicsAllocation()->getGpuAddress(), address1);

    // update address
    glSharing.uploadDataToBufferInfo(1, 1);
    pCmdQ->enqueueAcquireSharedObjects(1, &sharedMem, 0, nullptr, nullptr, CL_COMMAND_ACQUIRE_GL_OBJECTS);

    callOneWorkItemNDRKernel();

    auto address2 = static_cast<uint64_t>(*pKernelArg);
    EXPECT_NE(address1, address2);
    EXPECT_EQ(sharedBuffer->getGraphicsAllocation()->getGpuAddress(), address2);

    delete sharedBuffer;
    delete nonSharedBuffer;
}
