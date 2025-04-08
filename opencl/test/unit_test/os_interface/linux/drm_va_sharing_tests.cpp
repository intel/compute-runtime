/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/va/va_surface.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/sharings/va/mock_va_sharing.h"

namespace NEO {

using DrmVaSharingTest = Test<DrmMemoryManagerFixture>;

HWTEST_TEMPLATED_F(DrmVaSharingTest, givenDrmMemoryManagerWhenSharedVaSurfaceIsImportedWithDrmPrimeFdToHandleThenDrmPrimeFdCanBeClosed) {
    mock->ioctlExpected.total = -1;
    device->incRefInternal();
    MockClDevice clDevice{device};
    MockContext context(&clDevice);
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
