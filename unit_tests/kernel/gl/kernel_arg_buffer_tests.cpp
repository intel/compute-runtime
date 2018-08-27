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

#include "config.h"
#include "CL/cl.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/kernel/kernel_arg_buffer_fixture.h"
#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "gtest/gtest.h"

#include <memory>

using namespace OCLRT;

typedef Test<KernelArgBufferFixture> KernelArgBufferTest;

TEST_F(KernelArgBufferTest, givenSharedBufferWhenSetArgIsCalledThenReportSharedObjUsage) {
    MockGlSharing glSharing;
    glSharing.uploadDataToBufferInfo(1, 0);
    pContext->setSharingFunctions(new GlSharingFunctionsMock());
    auto sharedBuffer = GlBuffer::createSharedGlBuffer(pContext, CL_MEM_READ_WRITE, 1);
    auto nonSharedBuffer = new MockBuffer;

    auto sharedMem = static_cast<cl_mem>(sharedBuffer);
    auto nonSharedMem = static_cast<cl_mem>(nonSharedBuffer);

    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());
    this->pKernel->setArg(0, sizeof(cl_mem *), &nonSharedMem);
    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());

    this->pKernel->setArg(0, sizeof(cl_mem *), &sharedMem);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());

    delete nonSharedBuffer;
    delete sharedBuffer;
}

HWTEST_F(KernelArgBufferTest, givenSharedBufferWhenSetArgStatefulIsCalledThenBufferSurfaceShouldBeUsed) {
    MockGlSharing glSharing;
    glSharing.uploadDataToBufferInfo(1, 0);
    pContext->setSharingFunctions(new GlSharingFunctionsMock());
    auto sharedBuffer = GlBuffer::createSharedGlBuffer(pContext, CL_MEM_READ_WRITE, 1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    sharedBuffer->setArgStateful(&surfaceState);

    auto surfType = surfaceState.getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfType);

    delete sharedBuffer;
}
