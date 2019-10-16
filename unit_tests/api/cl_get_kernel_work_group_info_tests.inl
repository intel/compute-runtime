/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/file_io.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "cl_api_tests.h"

using namespace NEO;

struct clGetKernelWorkGroupInfoTests : public api_fixture,
                                       public ::testing::TestWithParam<uint32_t /*cl_kernel_work_group_info*/> {
    typedef api_fixture BaseClass;

    void SetUp() override {
        BaseClass::SetUp();

        std::unique_ptr<char[]> pSource = nullptr;
        size_t sourceSize = 0;
        std::string testFile;

        kbHelper = new KernelBinaryHelper("CopyBuffer_simd8", false);
        testFile.append(clFiles);
        testFile.append("CopyBuffer_simd8.cl");
        ASSERT_EQ(true, fileExists(testFile));

        pSource = loadDataFromFile(
            testFile.c_str(),
            sourceSize);
        ASSERT_NE(0u, sourceSize);
        ASSERT_NE(nullptr, pSource);

        const char *sources[1] = {pSource.get()};
        pProgram = clCreateProgramWithSource(
            pContext,
            1,
            sources,
            &sourceSize,
            &retVal);
        EXPECT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        pSource.reset();

        retVal = clBuildProgram(
            pProgram,
            num_devices,
            devices,
            nullptr,
            nullptr,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        retVal = clReleaseKernel(kernel);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = clReleaseProgram(pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        delete kbHelper;
        BaseClass::TearDown();
    }

    cl_program pProgram = nullptr;
    cl_kernel kernel = nullptr;
    KernelBinaryHelper *kbHelper;
};

namespace ULT {

TEST_P(clGetKernelWorkGroupInfoTests, GivenValidParametersWhenGettingKernelWorkGroupInfoThenSuccessIsReturned) {

    size_t paramValueSizeRet;
    retVal = clGetKernelWorkGroupInfo(
        kernel,
        devices[0],
        GetParam(),
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, paramValueSizeRet);
}

TEST_F(clGetKernelWorkGroupInfoTests, GivenKernelRequiringScratchSpaceWhenGettingKernelWorkGroupInfoThenCorrectSpillMemSizeIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);
    SPatchMediaVFEState mediaVFEstate;
    mediaVFEstate.PerThreadScratchSpace = 1024; //whatever greater than 0
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;

    cl_ulong scratchSpaceSize = static_cast<cl_ulong>(mockKernel.mockKernel->getScratchSize());
    EXPECT_EQ(scratchSpaceSize, 1024u);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_SPILL_MEM_SIZE_INTEL,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, scratchSpaceSize);
}

TEST_F(clGetKernelWorkGroupInfoTests, givenKernelHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenProperSizeIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);
    SPatchAllocateStatelessPrivateSurface privateAllocation;
    privateAllocation.PerThreadPrivateMemorySize = 1024;
    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrivateSurface = &privateAllocation;

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, privateAllocation.PerThreadPrivateMemorySize);
}

TEST_F(clGetKernelWorkGroupInfoTests, givenKernelNotHavingPrivateMemoryAllocationWhenAskedForPrivateAllocationSizeThenZeroIsReturned) {
    size_t paramValueSizeRet;
    cl_ulong param_value;
    auto pDevice = castToObject<Device>(devices[0]);

    MockKernelWithInternals mockKernel(*pDevice);

    retVal = clGetKernelWorkGroupInfo(
        mockKernel,
        pDevice,
        CL_KERNEL_PRIVATE_MEM_SIZE,
        sizeof(cl_ulong),
        &param_value,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(cl_ulong));
    EXPECT_EQ(param_value, 0u);
}

static cl_kernel_work_group_info paramNames[] = {
    CL_KERNEL_WORK_GROUP_SIZE,
    CL_KERNEL_COMPILE_WORK_GROUP_SIZE,
    CL_KERNEL_LOCAL_MEM_SIZE,
    CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
    CL_KERNEL_SPILL_MEM_SIZE_INTEL,
    CL_KERNEL_PRIVATE_MEM_SIZE};

INSTANTIATE_TEST_CASE_P(
    api,
    clGetKernelWorkGroupInfoTests,
    testing::ValuesIn(paramNames));
} // namespace ULT
