/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_work_size.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

using LocalWorkSizeTestDG2 = ::testing::Test;

DG2TEST_F(LocalWorkSizeTestDG2, givenKernelWithDpasAndSlmWhenWorkSizeInfoCalculatedThenMinWGSizeIsLessThanForKernelWithoutDpas) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    auto threadsPerEu = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.EUCount;
    auto euPerSubSlice = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.MaxEuPerSubSlice;

    auto &deviceInfo = device.sharedDeviceInfo;
    deviceInfo.maxNumEUsPerSubSlice = euPerSubSlice;
    deviceInfo.numThreadsPerEU = threadsPerEu;
    kernel.mockKernel->slmTotalSize = 0x100;

    const_cast<NEO::KernelDescriptor &>(kernel.mockKernel->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = true;
    WorkSizeInfo workSizeInfoWithDpas = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    const_cast<NEO::KernelDescriptor &>(kernel.mockKernel->getDescriptor()).kernelAttributes.flags.usesSystolicPipelineSelectMode = false;
    WorkSizeInfo workSizeInfoWithoutDpas = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
    EXPECT_NE(workSizeInfoWithDpas.minWorkGroupSize, workSizeInfoWithoutDpas.minWorkGroupSize);
}

DG2TEST_F(LocalWorkSizeTestDG2, givenKernelWithFusedEuDisabledAndSlmWhenWorkSizeInfoCalculatedThenMinWGSizeIsLessThanForKernelWithoutDpas) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);
    DispatchInfo dispatchInfo;
    dispatchInfo.setClDevice(&device);
    dispatchInfo.setKernel(kernel.mockKernel);

    auto threadsPerEu = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.EUCount;
    auto euPerSubSlice = defaultHwInfo->gtSystemInfo.ThreadCount / defaultHwInfo->gtSystemInfo.MaxEuPerSubSlice;

    auto &deviceInfo = device.sharedDeviceInfo;
    deviceInfo.maxNumEUsPerSubSlice = euPerSubSlice;
    deviceInfo.numThreadsPerEU = threadsPerEu;
    kernel.mockKernel->slmTotalSize = 0x100;

    const_cast<NEO::KernelDescriptor &>(kernel.mockKernel->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = true;
    WorkSizeInfo workSizeInfoWithDpas = createWorkSizeInfoFromDispatchInfo(dispatchInfo);

    const_cast<NEO::KernelDescriptor &>(kernel.mockKernel->getDescriptor()).kernelAttributes.flags.requiresDisabledEUFusion = false;
    WorkSizeInfo workSizeInfoWithoutDpas = createWorkSizeInfoFromDispatchInfo(dispatchInfo);
    EXPECT_NE(workSizeInfoWithDpas.minWorkGroupSize, workSizeInfoWithoutDpas.minWorkGroupSize);
}