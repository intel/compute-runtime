/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

using namespace NEO;

struct KernelSubGroupInfoKhrFixture : HelloWorldFixture<HelloWorldFixtureFactory> {
    typedef HelloWorldFixture<HelloWorldFixtureFactory> ParentClass;

    void SetUp() override {
        ParentClass::SetUp();
        MaxSimdSize = static_cast<size_t>(pKernel->getKernelInfo().getMaxSimdSize());
        ASSERT_GE(MaxSimdSize, 8u);
        MaxWorkDim = static_cast<size_t>(pClDevice->getDeviceInfo().maxWorkItemDimensions);
        ASSERT_EQ(MaxWorkDim, 3u);
    }

    void TearDown() override {
        ParentClass::TearDown();
    }

    size_t inputValue[3];
    size_t paramValue;
    size_t paramValueSizeRet;
    size_t MaxSimdSize;
    size_t CalculatedWGS;
    size_t MaxWorkDim;
};

namespace ULT {

typedef Test<KernelSubGroupInfoKhrFixture> KernelSubGroupInfoKhrTest;

template <typename ParamType>
struct KernelSubGroupInfoKhrParamFixture : KernelSubGroupInfoKhrFixture,
                                           ::testing::TestWithParam<ParamType> {
    void SetUp() override {
        KernelSubGroupInfoKhrFixture::SetUp();
    }

    void TearDown() override {
        KernelSubGroupInfoKhrFixture::TearDown();
    }
};

struct TestParam {
    size_t gwsX;
    size_t gwsY;
    size_t gwsZ;
} KernelSubGroupInfoKhrWGS[] = {
    {0, 0, 0},
    {1, 1, 1},
    {1, 5, 1},
    {8, 1, 1},
    {16, 1, 1},
    {32, 1, 1},
    {64, 1, 1},
    {1, 190, 1},
    {1, 510, 1},
    {512, 1, 1}};

typedef KernelSubGroupInfoKhrParamFixture<TestParam> KernelSubGroupInfoKhrReturnSizeTest;

INSTANTIATE_TEST_CASE_P(wgs,
                        KernelSubGroupInfoKhrReturnSizeTest,
                        ::testing::ValuesIn(KernelSubGroupInfoKhrWGS));

TEST_P(KernelSubGroupInfoKhrReturnSizeTest, GivenLwsParameterWhenGettingMaxSubGroupSizeThenCorrectValueIsReturned) {
    paramValueSizeRet = 0;

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE,
        sizeof(size_t) * 3,
        inputValue,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

    EXPECT_EQ(paramValue, MaxSimdSize);
}

typedef KernelSubGroupInfoKhrParamFixture<TestParam> KernelSubGroupInfoKhrReturnCountTest;

INSTANTIATE_TEST_CASE_P(wgs,
                        KernelSubGroupInfoKhrReturnCountTest,
                        ::testing::ValuesIn(KernelSubGroupInfoKhrWGS));

TEST_P(KernelSubGroupInfoKhrReturnCountTest, GivenLwsParameterWhenGettingSubGroupCountThenCorrectValueIsReturned) {
    paramValueSizeRet = 0;
    inputValue[0] = GetParam().gwsX;
    inputValue[1] = GetParam().gwsY;
    inputValue[2] = GetParam().gwsZ;
    CalculatedWGS = inputValue[0] * inputValue[1] * inputValue[2];

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE,
        sizeof(size_t) * 3,
        inputValue,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

    if (CalculatedWGS % MaxSimdSize == 0) {
        EXPECT_EQ(paramValue, CalculatedWGS / MaxSimdSize);
    } else {
        EXPECT_EQ(paramValue, (CalculatedWGS / MaxSimdSize) + 1);
    }
}

typedef KernelSubGroupInfoKhrParamFixture<TestParam> KernelSubGroupInfoKhrReturnCompileSizeTest;

TEST_F(KernelSubGroupInfoKhrReturnCompileSizeTest, GivenKernelWhenGettingRequiredSubGroupSizeThenCorrectValueIsReturned) {

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

    size_t requiredSubGroupSize = 0;
    auto start = pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelLanguageAttributes.find("intel_reqd_sub_group_size(");
    if (start != std::string::npos) {
        start += strlen("intel_reqd_sub_group_size(");
        auto stop = pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelLanguageAttributes.find(")", start);
        requiredSubGroupSize = stoi(pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelLanguageAttributes.substr(start, stop - start));
    }

    EXPECT_EQ(paramValue, requiredSubGroupSize);
}

TEST_F(KernelSubGroupInfoKhrTest, GivenNullKernelWhenGettingKernelSubGroupInfoThenInvalidKernelErrorIsReturned) {
    retVal = clGetKernelSubGroupInfoKHR(
        nullptr,
        pClDevice,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_KERNEL);
}

TEST_F(KernelSubGroupInfoKhrTest, GivenInvalidDeviceWhenGettingSubGroupInfoFromSingleDeviceKernelThenInvalidDeviceErrorIsReturned) {

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        reinterpret_cast<cl_device_id>(pKernel),
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(KernelSubGroupInfoKhrTest, GivenNullDeviceWhenGettingSubGroupInfoFromSingleDeviceKernelThenSuccessIsReturned) {

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        nullptr,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(KernelSubGroupInfoKhrTest, GivenNullDeviceWhenGettingSubGroupInfoFromMultiDeviceKernelThenInvalidDeviceErrorIsReturned) {

    MockUnrestrictiveContext context;
    auto mockProgram = std::make_unique<MockProgram>(&context, false, context.getDevices());
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(
        MultiDeviceKernel::create<MockKernel>(mockProgram.get(), this->pMultiDeviceKernel->getKernelInfos(), nullptr));

    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel.get(),
        nullptr,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(KernelSubGroupInfoKhrTest, GivenInvalidParamNameWhenGettingKernelSubGroupInfoThenInvalidValueErrorIsReturned) {
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        0,
        sizeof(size_t),
        inputValue,
        sizeof(size_t),
        &paramValue,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

uint32_t /*cl_kernel_sub_group_info_khr*/ KernelSubGroupInfoKhrInputParams[] = {
    CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR,
    CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR};

typedef KernelSubGroupInfoKhrParamFixture<uint32_t /*cl_kernel_sub_group_info_khr*/> KernelSubGroupInfoKhrInputParamsTest;

INSTANTIATE_TEST_CASE_P(KernelSubGroupInfoKhrInputParams,
                        KernelSubGroupInfoKhrInputParamsTest,
                        ::testing::ValuesIn(KernelSubGroupInfoKhrInputParams));

TEST_P(KernelSubGroupInfoKhrInputParamsTest, GivenInvalidInputWhenGettingKernelSubGroupInfoThenInvalidValueErrorIsReturned) {
    // work dim == 0
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        0,
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);

    // work dim % sizeof(size_t) != 0
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        (sizeof(size_t) * MaxWorkDim) - 1,
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);

    // work dim > MaxWorkDim
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t) * (MaxWorkDim + 1),
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);

    // null input_value
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t) * (MaxWorkDim),
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_P(KernelSubGroupInfoKhrInputParamsTest, GivenInvalidParamSizeWhenGettingKernelSubGroupInfoThenInvalidValueErrorIsReturned) {
    //param_value_size < sizeof(size_t)
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        sizeof(size_t) - 1,
        &paramValue,
        nullptr);

    EXPECT_EQ(retVal, CL_INVALID_VALUE);
}

TEST_P(KernelSubGroupInfoKhrInputParamsTest, GivenNoReturnPointerWhenGettingKernelSubGroupInfoThenSuccessIsReturned) {
    retVal = clGetKernelSubGroupInfoKHR(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(retVal, CL_SUCCESS);
}
} // namespace ULT
