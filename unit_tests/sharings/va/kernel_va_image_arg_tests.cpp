/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/va/va_surface.h"
#include "test.h"
#include "unit_tests/fixtures/kernel_arg_fixture.h"
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
