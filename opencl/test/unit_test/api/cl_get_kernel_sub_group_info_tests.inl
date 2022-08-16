/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct KernelSubGroupInfoFixture : HelloWorldFixture<HelloWorldFixtureFactory> {
    typedef HelloWorldFixture<HelloWorldFixtureFactory> ParentClass;

    void setUp() {
        ParentClass::setUp();
        pKernel->maxKernelWorkGroupSize = static_cast<uint32_t>(pDevice->getDeviceInfo().maxWorkGroupSize / 2);
        maxSimdSize = static_cast<size_t>(pKernel->getKernelInfo().getMaxSimdSize());
        ASSERT_LE(8u, maxSimdSize);
        maxWorkDim = static_cast<size_t>(pClDevice->getDeviceInfo().maxWorkItemDimensions);
        ASSERT_EQ(3u, maxWorkDim);
        maxWorkGroupSize = static_cast<size_t>(pKernel->maxKernelWorkGroupSize);
        ASSERT_GE(1024u, maxWorkGroupSize);
        largestCompiledSIMDSize = static_cast<size_t>(pKernel->getKernelInfo().getMaxSimdSize());
        ASSERT_EQ(32u, largestCompiledSIMDSize);

        auto requiredWorkGroupSizeX = static_cast<size_t>(pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
        auto requiredWorkGroupSizeY = static_cast<size_t>(pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
        auto requiredWorkGroupSizeZ = static_cast<size_t>(pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);

        calculatedMaxWorkgroupSize = requiredWorkGroupSizeX * requiredWorkGroupSizeY * requiredWorkGroupSizeZ;
        if ((calculatedMaxWorkgroupSize == 0) || (calculatedMaxWorkgroupSize > static_cast<size_t>(pKernel->maxKernelWorkGroupSize))) {
            calculatedMaxWorkgroupSize = static_cast<size_t>(pKernel->maxKernelWorkGroupSize);
        }
    }

    void tearDown() {
        ParentClass::tearDown();
    }

    size_t inputValue[3];
    size_t paramValue[3];
    size_t paramValueSizeRet;
    size_t maxSimdSize;
    size_t maxWorkDim;
    size_t maxWorkGroupSize;
    size_t largestCompiledSIMDSize;
    size_t calculatedMaxWorkgroupSize;
};

namespace ULT {

typedef Test<KernelSubGroupInfoFixture> KernelSubGroupInfoTest;

template <typename ParamType>
struct KernelSubGroupInfoParamFixture : KernelSubGroupInfoFixture,
                                        ::testing::TestWithParam<ParamType> {
    void SetUp() override {
        KernelSubGroupInfoFixture::setUp();
    }

    void TearDown() override {
        KernelSubGroupInfoFixture::tearDown();
    }
};

static size_t WorkDimensions[] = {1, 2, 3};

static struct WorkSizeParam {
    size_t x;
    size_t y;
    size_t z;
} KernelSubGroupInfoWGS[] = {
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

typedef KernelSubGroupInfoParamFixture<std::tuple<WorkSizeParam, size_t>> KernelSubGroupInfoReturnSizeTest;

INSTANTIATE_TEST_CASE_P(wgs,
                        KernelSubGroupInfoReturnSizeTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(KernelSubGroupInfoWGS),
                            ::testing::ValuesIn(WorkDimensions)));

TEST_P(KernelSubGroupInfoReturnSizeTest, GivenWorkGroupSizeWhenGettingMaxSubGroupSizeThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    WorkSizeParam workSize;
    size_t workDim;
    std::tie(workSize, workDim) = GetParam();

    memset(inputValue, 0, sizeof(inputValue));
    inputValue[0] = workSize.x;
    if (workDim > 1) {
        inputValue[1] = workSize.y;
    }
    if (workDim > 2) {
        inputValue[2] = workSize.z;
    }
    paramValueSizeRet = 0;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE,
        sizeof(size_t) * workDim,
        inputValue,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

    EXPECT_EQ(maxSimdSize, paramValue[0]);
}

typedef KernelSubGroupInfoParamFixture<std::tuple<WorkSizeParam, size_t>> KernelSubGroupInfoReturnCountTest;

INSTANTIATE_TEST_CASE_P(wgs,
                        KernelSubGroupInfoReturnCountTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(KernelSubGroupInfoWGS),
                            ::testing::ValuesIn(WorkDimensions)));

TEST_P(KernelSubGroupInfoReturnCountTest, GivenWorkGroupSizeWhenGettingSubGroupCountThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    WorkSizeParam workSize;
    size_t workDim;
    std::tie(workSize, workDim) = GetParam();

    memset(inputValue, 0, sizeof(inputValue));
    inputValue[0] = workSize.x;
    if (workDim > 1) {
        inputValue[1] = workSize.y;
    }
    if (workDim > 2) {
        inputValue[2] = workSize.z;
    }
    paramValueSizeRet = 0;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE,
        sizeof(size_t) * workDim,
        inputValue,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(sizeof(size_t), paramValueSizeRet);

    auto calculatedWGS = workSize.x;
    if (workDim > 1) {
        calculatedWGS *= workSize.y;
    }
    if (workDim > 2) {
        calculatedWGS *= workSize.z;
    }

    if (calculatedWGS % maxSimdSize == 0) {
        EXPECT_EQ(calculatedWGS / maxSimdSize, paramValue[0]);
    } else {
        EXPECT_EQ((calculatedWGS / maxSimdSize) + 1, paramValue[0]);
    }
}

static size_t SubGroupsNumbers[] = {0, 1, 10, 12, 21, 33, 67, 99};

typedef KernelSubGroupInfoParamFixture<std::tuple<size_t, size_t>> KernelSubGroupInfoReturnLocalSizeTest;

INSTANTIATE_TEST_CASE_P(sgn,
                        KernelSubGroupInfoReturnLocalSizeTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(SubGroupsNumbers),
                            ::testing::ValuesIn(WorkDimensions)));

TEST_P(KernelSubGroupInfoReturnLocalSizeTest, GivenWorkGroupSizeWhenGettingLocalSizeThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    size_t subGroupsNum;
    size_t workDim;
    std::tie(subGroupsNum, workDim) = GetParam();

    inputValue[0] = subGroupsNum;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT,
        sizeof(size_t),
        inputValue,
        sizeof(size_t) * workDim,
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(sizeof(size_t) * workDim, paramValueSizeRet);

    size_t workGroupSize = subGroupsNum * largestCompiledSIMDSize;
    if (workGroupSize > calculatedMaxWorkgroupSize) {
        workGroupSize = 0;
    }

    EXPECT_EQ(workGroupSize, paramValue[0]);
    if (workDim > 1) {
        EXPECT_EQ(workGroupSize ? 1u : 0u, paramValue[1]);
    }
    if (workDim > 2) {
        EXPECT_EQ(workGroupSize ? 1u : 0u, paramValue[2]);
    }
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnMaxNumberTest;

TEST_F(KernelSubGroupInfoReturnMaxNumberTest, GivenWorkGroupSizeWhenGettingMaxNumSubGroupsThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_MAX_NUM_SUB_GROUPS,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));
    EXPECT_EQ(paramValue[0], Math::divideAndRoundUp(calculatedMaxWorkgroupSize, largestCompiledSIMDSize));
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnCompileNumberTest;

TEST_F(KernelSubGroupInfoReturnCompileNumberTest, GivenKernelWhenGettingCompileNumSubGroupThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_COMPILE_NUM_SUB_GROUPS,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSizeRet, sizeof(size_t));
    EXPECT_EQ(paramValue[0], static_cast<size_t>(pKernel->getKernelInfo().kernelDescriptor.kernelMetadata.compiledSubGroupsNumber));
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnCompileSizeTest;

TEST_F(KernelSubGroupInfoReturnCompileSizeTest, GivenKernelWhenGettingCompileSubGroupSizeThenReturnIsCalculatedCorrectly) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
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

    EXPECT_EQ(paramValue[0], requiredSubGroupSize);
}

TEST_F(KernelSubGroupInfoTest, GivenNullKernelWhenGettingSubGroupInfoThenInvalidKernelErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        nullptr,
        pClDevice,
        0,
        0,
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}
TEST_F(KernelSubGroupInfoTest, GivenInvalidDeviceWhenGettingSubGroupInfoFromSingleDeviceKernelThenInvalidDeviceErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        reinterpret_cast<cl_device_id>(pKernel),
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(KernelSubGroupInfoTest, GivenNullDeviceWhenGettingSubGroupInfoFromSingleDeviceKernelThenSuccessIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        nullptr,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(KernelSubGroupInfoTest, GivenNullDeviceWhenGettingSubGroupInfoFromMultiDeviceKernelThenInvalidDeviceErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    MockUnrestrictiveContext context;
    auto mockProgram = std::make_unique<MockProgram>(&context, false, context.getDevices());
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MultiDeviceKernel::create<MockKernel>(mockProgram.get(), this->pMultiDeviceKernel->getKernelInfos(), nullptr));

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel.get(),
        nullptr,
        CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
        0,
        nullptr,
        sizeof(size_t),
        paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(KernelSubGroupInfoTest, GivenInvalidParamNameWhenGettingSubGroupInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        0,
        sizeof(size_t),
        inputValue,
        sizeof(size_t),
        paramValue,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

uint32_t /*cl_kernel_sub_group_info*/ KernelSubGroupInfoInputParams[] = {
    CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE,
    CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE,
    CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT,
    CL_KERNEL_MAX_NUM_SUB_GROUPS,
    CL_KERNEL_COMPILE_NUM_SUB_GROUPS,
    CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL};

typedef KernelSubGroupInfoParamFixture<uint32_t /*cl_kernel_sub_group_info*/> KernelSubGroupInfoInputParamsTest;

INSTANTIATE_TEST_CASE_P(KernelSubGroupInfoInputParams,
                        KernelSubGroupInfoInputParamsTest,
                        ::testing::ValuesIn(KernelSubGroupInfoInputParams));

TEST_P(KernelSubGroupInfoInputParamsTest, GivenOpenClVersionLowerThan21WhenGettingKenrelSubGroupInfoThenInvalidOperationErrorIsReturned) {
    bool requireOpenCL21 = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ||
                           (GetParam() == CL_KERNEL_MAX_NUM_SUB_GROUPS) ||
                           (GetParam() == CL_KERNEL_COMPILE_NUM_SUB_GROUPS);
    if (requireOpenCL21) {
        DebugManager.flags.ForceOCLVersion.set(20);
        pDevice->initializeCaps();
        pClDevice->initializeCaps();

        retVal = clGetKernelSubGroupInfo(
            pMultiDeviceKernel,
            pClDevice,
            GetParam(),
            0,
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_INVALID_OPERATION, retVal);

        DebugManager.flags.ForceOCLVersion.set(0);
        pDevice->initializeCaps();
        pClDevice->initializeCaps();
    }
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenWorkDimZeroWhenGettingSubGroupInfoThenSuccessOrErrorIsCorrectlyReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireInput = (GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        0,
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenIndivisibleWorkDimWhenGettingSubGroupInfoThenSuccessOrErrorIsCorrectlyReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireInput = (GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
    size_t workDim = ((GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                      (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE))
                         ? maxWorkDim
                         : 1;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        (sizeof(size_t) * workDim) - 1,
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenWorkDimGreaterThanMaxWorkDimWhenGettingSubGroupInfoThenSuccessOrErrorIsCorrectlyReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireInput = (GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
    size_t workDim = ((GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                      (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE))
                         ? maxWorkDim
                         : 1;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t) * (workDim + 1),
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenInputValueIsNullWhenGettingSubGroupInfoThenSuccessOrErrorIsCorrectlyReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireInput = (GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE) ||
                        (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
    size_t workDim = ((GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                      (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE))
                         ? maxWorkDim
                         : 1;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t) * (workDim),
        nullptr,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenParamValueSizeZeroWhenGettingSubGroupInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        0,
        paramValue,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenUnalignedParamValueSizeWhenGettingSubGroupInfoThenInvalidValueErrorIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    size_t workDim = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ? maxWorkDim : 1;

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        (sizeof(size_t) * workDim) - 1,
        paramValue,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenTooLargeParamValueSizeWhenGettingSubGroupInfoThenCorrectRetValIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireOutputArray = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
    size_t workDim = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ? maxWorkDim : 1;

    // paramValue size / sizeof(size_t) > MaxWorkDim
    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        sizeof(size_t) * (workDim + 1),
        paramValue,
        nullptr);

    EXPECT_EQ(requireOutputArray ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}

TEST_P(KernelSubGroupInfoInputParamsTest, GivenNullPtrForReturnWhenGettingKernelSubGroupInfoThenSuccessIsReturned) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    bool requireOutputArray = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);

    retVal = clGetKernelSubGroupInfo(
        pMultiDeviceKernel,
        pClDevice,
        GetParam(),
        sizeof(size_t),
        inputValue,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(requireOutputArray ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
}
} // namespace ULT
