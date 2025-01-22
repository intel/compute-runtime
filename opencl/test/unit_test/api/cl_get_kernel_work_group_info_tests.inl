/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_decoder.h"

#include "opencl/test/unit_test/fixtures/kernel_work_group_info_fixture.h"

using namespace NEO;

namespace ULT {

TEST_P(ClGetKernelWorkGroupInfoTests, GivenValidParametersWhenGettingKernelWorkGroupInfoThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();

    size_t paramValueSizeRet;
    auto retVal = clGetKernelWorkGroupInfo(
        kernel,
        testedClDevice,
        GetParam(),
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, paramValueSizeRet);
}

TEST_F(ClGetKernelWorkGroupInfoTest, GivenInvalidDeviceWhenGettingWorkGroupInfoFromSingleDeviceKernelThenInvalidDeviceErrorIsReturned) {

    size_t paramValueSizeRet;
    auto retVal = clGetKernelWorkGroupInfo(
        pMultiDeviceKernel,
        reinterpret_cast<cl_device_id>(pKernel),
        CL_KERNEL_WORK_GROUP_SIZE,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClGetKernelWorkGroupInfoTest, GivenNullDeviceWhenGettingWorkGroupInfoFromSingleDeviceKernelThenSuccessIsReturned) {

    size_t paramValueSizeRet;
    auto retVal = clGetKernelWorkGroupInfo(
        pMultiDeviceKernel,
        nullptr,
        CL_KERNEL_WORK_GROUP_SIZE,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClGetKernelWorkGroupInfoTest, GivenNullDeviceWhenGettingWorkGroupInfoFromMultiDeviceKernelThenInvalidDeviceErrorIsReturned) {
    USE_REAL_FILE_SYSTEM();

    size_t paramValueSizeRet;
    MockUnrestrictiveContext context;
    auto mockProgram = std::make_unique<MockProgram>(&context, false, context.getDevices());
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(
        MockMultiDeviceKernel::create<MockKernel>(mockProgram.get(), MockKernel::toKernelInfoContainer(pKernel->getKernelInfo(), context.getDevice(0)->getRootDeviceIndex())));

    retVal = clGetKernelWorkGroupInfo(
        pMultiDeviceKernel.get(),
        nullptr,
        CL_KERNEL_WORK_GROUP_SIZE,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(ClGetKernelWorkGroupInfoTests, GivenKernelRequiringScratchSpaceForSpillWhenGettingKernelWorkGroupInfoThenCorrectSpillMemSizeIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong paramValue;
    auto pDevice = castToObject<ClDevice>(testedClDevice);

    MockKernelWithInternals mockKernel(*pDevice);
    auto spillMemorySize = 1024u;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.spillFillScratchMemorySize = spillMemorySize;

    retVal = clGetKernelWorkGroupInfo(
        mockKernel.mockMultiDeviceKernel,
        pDevice,
        CL_KERNEL_SPILL_MEM_SIZE_INTEL,
        sizeof(cl_ulong),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(paramValue, spillMemorySize);
}

HWTEST2_F(ClGetKernelWorkGroupInfoTests, givenKernelHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenProperSizeIsReturned, IsGen12LP) {
    size_t paramValueSizeRet;
    cl_ulong paramValue;
    auto pDevice = castToObject<ClDevice>(testedClDevice);

    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.setPrivateMemory(1024, false, 0, 0, 0);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel.mockMultiDeviceKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_ulong), paramValueSizeRet);
    EXPECT_EQ(1024U, paramValue);
}

TEST_F(ClGetKernelWorkGroupInfoTests, givenKernelNotHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenZeroIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong paramValue;
    auto pDevice = castToObject<ClDevice>(testedClDevice);

    MockKernelWithInternals mockKernel(*pDevice);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel.mockMultiDeviceKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(paramValue, 0u);
}

TEST_F(ClGetKernelWorkGroupInfoTests, whenWorkgroupSizeCheckedThenSizeLimitIs1kOrLess) {
    size_t paramValueSizeRet = 0;
    size_t paramValue = 0;

    retVal = clGetKernelWorkGroupInfo(
        kernel,
        testedClDevice,
        CL_KERNEL_WORK_GROUP_SIZE,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));
    EXPECT_LE(paramValue, CommonConstants::maxWorkgroupSize);
}

static cl_kernel_work_group_info paramNames[] = {
    CL_KERNEL_WORK_GROUP_SIZE,
    CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
    CL_KERNEL_LOCAL_MEM_SIZE,
    CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
    CL_KERNEL_SPILL_MEM_SIZE_INTEL,
    CL_KERNEL_PRIVATE_MEM_SIZE};

INSTANTIATE_TEST_SUITE_P(
    api,
    ClGetKernelWorkGroupInfoTests,
    testing::ValuesIn(paramNames));
} // namespace ULT
