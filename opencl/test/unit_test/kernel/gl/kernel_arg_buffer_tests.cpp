/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/kernel/kernel_arg_buffer_fixture.h"
#include "opencl/test/unit_test/mocks/gl/mock_gl_sharing.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "CL/cl.h"
#include "config.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

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
