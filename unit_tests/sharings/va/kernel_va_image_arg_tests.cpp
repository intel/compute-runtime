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

#include "runtime/sharings/va/va_surface.h"
#include "unit_tests/fixtures/kernel_arg_fixture.h"
#include "unit_tests/gen_common/test.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/sharings/va/mock_va_sharing.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST_F(KernelImageArgTest, givenSharedImageWhenSetArgIsCalledThenReportSharedObjUsage) {
    MockVaSharing vaSharing;
    VASurfaceID vaSurfaceId = 0u;
    vaSharing.updateAcquiredHandle(1u);
    std::unique_ptr<Image> sharedImage(VASurface::createSharedVaSurface(context.get(), &vaSharing.m_sharingFunctions,
                                                                        CL_MEM_READ_WRITE, &vaSurfaceId, 0, nullptr));

    auto sharedMem = static_cast<cl_mem>(sharedImage.get());
    auto nonSharedMem = static_cast<cl_mem>(image.get());

    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());
    this->pKernel->setArg(0, sizeof(cl_mem *), &nonSharedMem);
    EXPECT_FALSE(pKernel->isUsingSharedObjArgs());

    this->pKernel->setArg(0, sizeof(cl_mem *), &sharedMem);
    EXPECT_TRUE(pKernel->isUsingSharedObjArgs());
}
