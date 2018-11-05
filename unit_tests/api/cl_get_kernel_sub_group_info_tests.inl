/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/hello_world_fixture.h"

using namespace OCLRT;

struct KernelSubGroupInfoFixture : HelloWorldFixture<HelloWorldFixtureFactory> {
    typedef HelloWorldFixture<HelloWorldFixtureFactory> ParentClass;

    void SetUp() override {
        ParentClass::SetUp();
        maxSimdSize = static_cast<size_t>(pKernel->getKernelInfo().getMaxSimdSize());
        ASSERT_LE(8u, maxSimdSize);
        maxWorkDim = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkItemDimensions);
        ASSERT_EQ(3u, maxWorkDim);
        maxWorkGroupSize = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize);
        ASSERT_GE(1024u, maxWorkGroupSize);
        largestCompiledSIMDSize = static_cast<size_t>(pKernel->getKernelInfo().patchInfo.executionEnvironment->LargestCompiledSIMDSize);
        ASSERT_EQ(32u, largestCompiledSIMDSize);

        auto requiredWorkGroupSizeX = static_cast<size_t>(pKernel->getKernelInfo().patchInfo.executionEnvironment->RequiredWorkGroupSizeX);
        auto requiredWorkGroupSizeY = static_cast<size_t>(pKernel->getKernelInfo().patchInfo.executionEnvironment->RequiredWorkGroupSizeY);
        auto requiredWorkGroupSizeZ = static_cast<size_t>(pKernel->getKernelInfo().patchInfo.executionEnvironment->RequiredWorkGroupSizeZ);

        calculatedMaxWorkgroupSize = requiredWorkGroupSizeX * requiredWorkGroupSizeY * requiredWorkGroupSizeZ;
        if ((calculatedMaxWorkgroupSize == 0) || (calculatedMaxWorkgroupSize > static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize))) {
            calculatedMaxWorkgroupSize = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize);
        }
    }

    void TearDown() override {
        ParentClass::TearDown();
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
        KernelSubGroupInfoFixture::SetUp();
    }

    void TearDown() override {
        KernelSubGroupInfoFixture::TearDown();
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

TEST_P(KernelSubGroupInfoReturnSizeTest, ReturnMaxSubGroupSizeForNdrParam) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
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
            pKernel,
            pDevice,
            CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE,
            sizeof(size_t) * workDim,
            inputValue,
            sizeof(size_t),
            paramValue,
            &paramValueSizeRet);

        EXPECT_EQ(retVal, CL_SUCCESS);

        EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

        auto calculatedWGS = inputValue[0];
        if (workDim > 1) {
            calculatedWGS *= inputValue[1];
        }
        if (workDim > 2) {
            calculatedWGS *= inputValue[2];
        }

        if (calculatedWGS < maxSimdSize) {
            EXPECT_EQ(calculatedWGS, paramValue[0]);
        } else {
            EXPECT_EQ(maxSimdSize, paramValue[0]);
        }
    }
}

typedef KernelSubGroupInfoParamFixture<std::tuple<WorkSizeParam, size_t>> KernelSubGroupInfoReturnCountTest;

INSTANTIATE_TEST_CASE_P(wgs,
                        KernelSubGroupInfoReturnCountTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(KernelSubGroupInfoWGS),
                            ::testing::ValuesIn(WorkDimensions)));

TEST_P(KernelSubGroupInfoReturnCountTest, ReturnSubGroupCountForNdrParam) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
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
            pKernel,
            pDevice,
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
}

static size_t SubGroupsNumbers[] = {0, 1, 10, 12, 21, 33, 67, 99};

typedef KernelSubGroupInfoParamFixture<std::tuple<size_t, size_t>> KernelSubGroupInfoReturnLocalSizeTest;

INSTANTIATE_TEST_CASE_P(sgn,
                        KernelSubGroupInfoReturnLocalSizeTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(SubGroupsNumbers),
                            ::testing::ValuesIn(WorkDimensions)));

TEST_P(KernelSubGroupInfoReturnLocalSizeTest, ReturnLocalSizeForSubGroupCount) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        size_t subGroupsNum;
        size_t workDim;
        std::tie(subGroupsNum, workDim) = GetParam();

        inputValue[0] = subGroupsNum;

        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
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
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnMaxNumberTest;

TEST_F(KernelSubGroupInfoReturnMaxNumberTest, ReturnMaxNumberOfSubGroups) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            CL_KERNEL_MAX_NUM_SUB_GROUPS,
            0,
            nullptr,
            sizeof(size_t),
            paramValue,
            &paramValueSizeRet);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(paramValueSizeRet, sizeof(size_t));
        EXPECT_EQ(paramValue[0], (calculatedMaxWorkgroupSize + largestCompiledSIMDSize - 1) / largestCompiledSIMDSize);
    }
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnCompileNumberTest;

TEST_F(KernelSubGroupInfoReturnCompileNumberTest, ReturnCompileNumberOfSubGroups) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            CL_KERNEL_COMPILE_NUM_SUB_GROUPS,
            0,
            nullptr,
            sizeof(size_t),
            paramValue,
            &paramValueSizeRet);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(paramValueSizeRet, sizeof(size_t));
        EXPECT_EQ(paramValue[0], static_cast<size_t>(pKernel->getKernelInfo().patchInfo.executionEnvironment->CompiledSubGroupsNumber));
    }
}

typedef KernelSubGroupInfoParamFixture<WorkSizeParam> KernelSubGroupInfoReturnCompileSizeTest;

TEST_F(KernelSubGroupInfoReturnCompileSizeTest, ReturnCompileSizeOfSubGroups) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL,
            0,
            nullptr,
            sizeof(size_t),
            paramValue,
            &paramValueSizeRet);

        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(paramValueSizeRet, sizeof(size_t));

        size_t requiredSubGroupSize = 0;
        auto start = pKernel->getKernelInfo().attributes.find("intel_reqd_sub_group_size(");
        if (start != std::string::npos) {
            start += strlen("intel_reqd_sub_group_size(");
            auto stop = pKernel->getKernelInfo().attributes.find(")", start);
            requiredSubGroupSize = stoi(pKernel->getKernelInfo().attributes.substr(start, stop - start));
        }

        EXPECT_EQ(paramValue[0], requiredSubGroupSize);
    }
}

TEST_F(KernelSubGroupInfoTest, InvalidKernel) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            nullptr,
            pDevice,
            0,
            0,
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    }
}

TEST_F(KernelSubGroupInfoTest, InvalidDevice) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_INVALID_DEVICE, retVal);
    }
}

TEST_F(KernelSubGroupInfoTest, InvalidParamName) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            0,
            sizeof(size_t),
            inputValue,
            sizeof(size_t),
            paramValue,
            nullptr);

        EXPECT_EQ(CL_INVALID_VALUE, retVal);
    }
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

TEST_P(KernelSubGroupInfoInputParamsTest, InvalidOpenCLVersion) {
    bool requireOpenCL21 = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ||
                           (GetParam() == CL_KERNEL_MAX_NUM_SUB_GROUPS) ||
                           (GetParam() == CL_KERNEL_COMPILE_NUM_SUB_GROUPS);
    if (requireOpenCL21) {
        DebugManager.flags.ForceOCLVersion.set(20);
        pDevice->initializeCaps();

        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            0,
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        DebugManager.flags.ForceOCLVersion.set(0);
        pDevice->initializeCaps();
    }
}

TEST_P(KernelSubGroupInfoInputParamsTest, InvalidInput) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        bool requireInput = (GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                            (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE) ||
                            (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
        size_t workDim = ((GetParam() == CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE) ||
                          (GetParam() == CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE))
                             ? maxWorkDim
                             : 1;

        // work dim == 0
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            0,
            inputValue,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);

        // work dim % sizeof(size_t) != 0
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            (sizeof(size_t) * workDim) - 1,
            inputValue,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);

        // work dim > MaxWorkDim
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t) * (workDim + 1),
            inputValue,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);

        // inputValue == null
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t) * (workDim),
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(requireInput ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
    }
}

TEST_P(KernelSubGroupInfoInputParamsTest, InvalidOutput) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        bool requireOutputArray = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);
        size_t workDim = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT) ? maxWorkDim : 1;

        // paramValue size == 0
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t),
            inputValue,
            0,
            paramValue,
            nullptr);

        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        // paramValue size % sizeof(size_t) != 0
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t),
            inputValue,
            (sizeof(size_t) * workDim) - 1,
            paramValue,
            nullptr);

        EXPECT_EQ(CL_INVALID_VALUE, retVal);

        // paramValue size / sizeof(size_t) > MaxWorkDim
        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t),
            inputValue,
            sizeof(size_t) * (workDim + 1),
            paramValue,
            nullptr);

        EXPECT_EQ(requireOutputArray ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
    }
}

TEST_P(KernelSubGroupInfoInputParamsTest, SuccessWhenNoReturnRequired) {
    if (std::string(pDevice->getDeviceInfo().clVersion).find("OpenCL 2.1") != std::string::npos) {
        bool requireOutputArray = (GetParam() == CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT);

        retVal = clGetKernelSubGroupInfo(
            pKernel,
            pDevice,
            GetParam(),
            sizeof(size_t),
            inputValue,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(requireOutputArray ? CL_INVALID_VALUE : CL_SUCCESS, retVal);
    }
}
} // namespace ULT
