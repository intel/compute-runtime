/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using KernelTestsPvc = ::testing::Test;

PVCTEST_F(KernelTestsPvc, givenDispatchInfoWhenWorkSizeInfoIsCreatedThenUseNonFusedMinWorkGroupSize) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.barrierCount = 1;
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    const uint32_t maxBarriersPerHSlice = 32;
    const uint32_t nonFusedMinWorkGroupSize = static_cast<uint32_t>(device.getSharedDeviceInfo().maxNumEUsPerSubSlice) *
                                              device.getSharedDeviceInfo().numThreadsPerEU *
                                              static_cast<uint32_t>(kernel.mockKernel->getKernelInfo().getMaxSimdSize()) /
                                              maxBarriersPerHSlice;
    WorkSizeInfo workSizeInfo = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    EXPECT_EQ(nonFusedMinWorkGroupSize, workSizeInfo.minWorkGroupSize);
}
