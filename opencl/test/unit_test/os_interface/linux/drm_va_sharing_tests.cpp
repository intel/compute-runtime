/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/va/va_surface.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"
#include "opencl/test/unit_test/sharings/va/mock_va_sharing.h"
#include "test.h"

namespace NEO {

using DrmVaSharingTest = Test<DrmMemoryManagerFixture>;

TEST_F(DrmVaSharingTest, givenDrmMemoryManagerWhenSharedVaSurfaceIsImportedWithDrmPrimeFdToHandleThenDrmPrimeFdCanBeClosed) {
    mock->ioctl_expected.total = -1;

    MockContext context(device);
    MockVaSharing vaSharing;
    VASurfaceID vaSurfaceId = 0u;

    vaSharing.updateAcquiredHandle(1);
    std::unique_ptr<Image> sharedImage1(VASurface::createSharedVaSurface(&context, &vaSharing.sharingFunctions,
                                                                         CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, nullptr));
    EXPECT_EQ(1, closeCalledCount);
    EXPECT_EQ(1, closeInputFd);

    vaSharing.updateAcquiredHandle(2);
    std::unique_ptr<Image> sharedImage2(VASurface::createSharedVaSurface(&context, &vaSharing.sharingFunctions,
                                                                         CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, nullptr));
    EXPECT_EQ(2, closeCalledCount);
    EXPECT_EQ(2, closeInputFd);
}
} // namespace NEO
