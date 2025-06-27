/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <memory>

using namespace NEO;

class KernelArgInfoFixture {
  public:
    KernelArgInfoFixture(){};

    void setUp() {
        clDevice = std::make_unique<MockClDevice>(new MockDevice());
        program = std::make_unique<MockProgram>(toClDeviceVector(*clDevice));
        kernel = std::make_unique<MockKernel>(program.get(), kernelInfo, *clDevice);
        kernelDescriptor = &kernelInfo.kernelDescriptor;
        kernelDescriptor->payloadMappings.explicitArgs.resize(1);
        kernelDescriptor->explicitArgsExtendedMetadata.resize(1);
    }

    void tearDown() {}

    template <typename T>
    cl_int queryArgInfo(cl_kernel_arg_info paramName, T &paramValue) {
        return kernel->getArgInfo(0, paramName, sizeof(T), &paramValue, nullptr);
    }

    KernelInfo kernelInfo;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MockKernel> kernel;
    KernelDescriptor *kernelDescriptor;
};

using KernelArgInfoTest = Test<KernelArgInfoFixture>;

TEST_F(KernelArgInfoTest, GivenNullWhenGettingKernelInfoThenNullIsReturned) {
    auto retKernelInfo = program->getKernelInfo(nullptr, 0);
    EXPECT_EQ(nullptr, retKernelInfo);
}

TEST_F(KernelArgInfoTest, GivenArgIndexBiggerThanNumberOfArgsThenErrorIsReturned) {
    auto retVal = kernel->getArgInfo(1, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_INDEX, retVal);
}

TEST_F(KernelArgInfoTest, GivenValidArgIndexThenExplicitArgsMetadataIsPopulated) {
    kernel->getArgInfo(0, 0, 0, nullptr, nullptr);
    EXPECT_TRUE(program->wasPopulateZebinExtendedArgsMetadataOnceCalled);
}

TEST_F(KernelArgInfoTest, GivenInvalidParametersWhenGettingKernelArgInfoThenValueSizeRetIsNotUpdated) {
    auto retVal = kernel->getArgInfo(0, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(KernelArgInfoTest, GivenKernelArgAccessQualifierWhenQueryingArgInfoThenKernelArgAcessNoneIsReturned) {
    auto &argTraits = kernelDescriptor->payloadMappings.explicitArgs[0].getTraits();
    argTraits.accessQualifier = KernelArgMetadata::AccessNone;

    cl_kernel_arg_access_qualifier paramValue = 0;
    auto retVal = queryArgInfo<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_QUALIFIER, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE), paramValue);
}

TEST_F(KernelArgInfoTest, GivenKernelArgAddressQualifierWhenQueryingArgInfoThenKernelArgAddressGlobalIsReturned) {
    auto &argTraits = kernelDescriptor->payloadMappings.explicitArgs[0].getTraits();
    argTraits.addressQualifier = KernelArgMetadata::AddrGlobal;

    cl_kernel_arg_address_qualifier paramValue = 0;
    auto retVal = queryArgInfo<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_QUALIFIER, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), paramValue);
}

TEST_F(KernelArgInfoTest, GivenKernelArgTypeQualifierWhenQueryingArgInfoThenKernelArgTypeNoneIsReturned) {
    cl_kernel_arg_type_qualifier paramValue = 0;
    auto retVal = queryArgInfo<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_QUALIFIER, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_NONE), paramValue);
}

TEST_F(KernelArgInfoTest, GivenParamWhenGettingKernelTypeNameThenCorrectValueIsReturned) {
    const char expectedArgType[] = "uint*";
    kernelDescriptor->explicitArgsExtendedMetadata.at(0).type = expectedArgType;

    constexpr auto paramValueSize = sizeof(expectedArgType);
    auto paramValue = std::make_unique<char[]>(paramValueSize);
    auto retVal = kernel->getArgInfo(
        0,
        CL_KERNEL_ARG_TYPE_NAME,
        paramValueSize,
        paramValue.get(),
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strncmp(paramValue.get(), expectedArgType, sizeof(expectedArgType)));
}

TEST_F(KernelArgInfoTest, GivenParamWhenGettingKernelArgNameThenCorrectValueIsReturned) {
    const char expectedArgName[] = "src";
    kernelDescriptor->explicitArgsExtendedMetadata.at(0).argName = expectedArgName;

    constexpr auto paramValueSize = sizeof(expectedArgName);
    auto paramValue = std::make_unique<char[]>(paramValueSize);
    auto retVal = kernel->getArgInfo(
        0,
        CL_KERNEL_ARG_NAME,
        paramValueSize,
        paramValue.get(),
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(paramValue.get(), expectedArgName));
}