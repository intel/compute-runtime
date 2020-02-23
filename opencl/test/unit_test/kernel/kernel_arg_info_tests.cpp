/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "opencl/test/unit_test/program/program_with_source.h"
#include "test.h"

using namespace NEO;

class KernelArgInfoTest : public ProgramFromSourceTest {
  public:
    KernelArgInfoTest() {
    }

    ~KernelArgInfoTest() override = default;

  protected:
    void SetUp() override {
        kbHelper = new KernelBinaryHelper("copybuffer", true);
        ProgramFromSourceTest::SetUp();
        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        cl_device_id device = pPlatform->getClDevice(0);
        retVal = pProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create(
            pProgram,
            *pProgram->getKernelInfo(KernelName),
            &retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);
    }

    void TearDown() override {
        delete pKernel;
        pKernel = nullptr;
        ProgramFromSourceTest::TearDown();
        delete kbHelper;
    }

    template <typename T>
    void queryArgInfo(cl_kernel_arg_info paramName, T &paramValue) {
        size_t paramValueSize = 0;
        size_t param_value_size_ret = 0;

        // get size
        retVal = pKernel->getArgInfo(
            0,
            paramName,
            paramValueSize,
            nullptr,
            &param_value_size_ret);
        EXPECT_NE(0u, param_value_size_ret);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // get the name
        paramValueSize = param_value_size_ret;

        retVal = pKernel->getArgInfo(
            0,
            paramName,
            paramValueSize,
            &paramValue,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    Kernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
    KernelBinaryHelper *kbHelper = nullptr;
};

TEST_P(KernelArgInfoTest, Create_Simple) {
    // included in the setup of fixture
}

TEST_P(KernelArgInfoTest, WhenQueryingWithNullptrKernelNameTheniReturnNullptr) {
    auto kernelInfo = this->pProgram->getKernelInfo(nullptr);
    EXPECT_EQ(nullptr, kernelInfo);
}

TEST_P(KernelArgInfoTest, getKernelArgAcessQualifier) {
    cl_kernel_arg_access_qualifier param_value = 0;
    queryArgInfo<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_QUALIFIER, param_value);
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE), param_value);
}

TEST_P(KernelArgInfoTest, getKernelAddressQulifier) {
    cl_kernel_arg_address_qualifier param_value = 0;
    queryArgInfo<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_QUALIFIER, param_value);
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), param_value);
}

TEST_P(KernelArgInfoTest, getKernelTypeQualifer) {
    cl_kernel_arg_type_qualifier param_value = 0;
    queryArgInfo<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_QUALIFIER, param_value);
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_NONE), param_value);
}

TEST_P(KernelArgInfoTest, getKernelTypeName) {
    cl_kernel_arg_info param_name = CL_KERNEL_ARG_TYPE_NAME;
    char *param_value = nullptr;
    size_t paramValueSize = 0;
    size_t param_value_size_ret = 0;

    // get size
    retVal = pKernel->getArgInfo(
        0,
        param_name,
        paramValueSize,
        nullptr,
        &param_value_size_ret);
    EXPECT_NE(0u, param_value_size_ret);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for name
    param_value = new char[param_value_size_ret];

    // get the name
    paramValueSize = param_value_size_ret;

    retVal = pKernel->getArgInfo(
        0,
        param_name,
        paramValueSize,
        param_value,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    const char expectedString[] = "uint*";
    auto result = strncmp(param_value, expectedString, sizeof(expectedString));
    EXPECT_EQ(0, result);
    delete[] param_value;
}

TEST_P(KernelArgInfoTest, getKernelArgName) {
    cl_kernel_arg_info param_name = CL_KERNEL_ARG_NAME;
    char *param_value = nullptr;
    size_t paramValueSize = 0;
    size_t param_value_size_ret = 0;

    // get size
    retVal = pKernel->getArgInfo(
        0,
        param_name,
        paramValueSize,
        nullptr,
        &param_value_size_ret);
    EXPECT_NE(0u, param_value_size_ret);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for name
    param_value = new char[param_value_size_ret];

    // get the name
    paramValueSize = param_value_size_ret;

    retVal = pKernel->getArgInfo(
        0,
        param_name,
        paramValueSize,
        param_value,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(param_value, "src"));
    delete[] param_value;
}

INSTANTIATE_TEST_CASE_P(KernelArgInfoTests,
                        KernelArgInfoTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(SourceFileNames),
                            ::testing::ValuesIn(BinaryForSourceFileNames),
                            ::testing::ValuesIn(KernelNames)));

TEST(KernelArgMetadata, WhenParseAccessQualifierIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(AccessQualifier::None, KernelArgMetadata::parseAccessQualifier(""));
    EXPECT_EQ(AccessQualifier::None, KernelArgMetadata::parseAccessQualifier("NONE"));
    EXPECT_EQ(AccessQualifier::ReadOnly, KernelArgMetadata::parseAccessQualifier("read_only"));
    EXPECT_EQ(AccessQualifier::WriteOnly, KernelArgMetadata::parseAccessQualifier("write_only"));
    EXPECT_EQ(AccessQualifier::ReadWrite, KernelArgMetadata::parseAccessQualifier("read_write"));
    EXPECT_EQ(AccessQualifier::ReadOnly, KernelArgMetadata::parseAccessQualifier("__read_only"));
    EXPECT_EQ(AccessQualifier::WriteOnly, KernelArgMetadata::parseAccessQualifier("__write_only"));
    EXPECT_EQ(AccessQualifier::ReadWrite, KernelArgMetadata::parseAccessQualifier("__read_write"));

    EXPECT_EQ(AccessQualifier::Unknown, KernelArgMetadata::parseAccessQualifier("re"));
    EXPECT_EQ(AccessQualifier::Unknown, KernelArgMetadata::parseAccessQualifier("read"));
    EXPECT_EQ(AccessQualifier::Unknown, KernelArgMetadata::parseAccessQualifier("write"));
}

TEST(KernelArgMetadata, WhenParseAddressQualifierIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(AddressSpaceQualifier::Global, KernelArgMetadata::parseAddressSpace(""));
    EXPECT_EQ(AddressSpaceQualifier::Global, KernelArgMetadata::parseAddressSpace("__global"));
    EXPECT_EQ(AddressSpaceQualifier::Local, KernelArgMetadata::parseAddressSpace("__local"));
    EXPECT_EQ(AddressSpaceQualifier::Private, KernelArgMetadata::parseAddressSpace("__private"));
    EXPECT_EQ(AddressSpaceQualifier::Constant, KernelArgMetadata::parseAddressSpace("__constant"));
    EXPECT_EQ(AddressSpaceQualifier::Private, KernelArgMetadata::parseAddressSpace("not_specified"));

    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("wrong"));
    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("__glob"));
    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("__loc"));
    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("__priv"));
    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("__const"));
    EXPECT_EQ(AddressSpaceQualifier::Unknown, KernelArgMetadata::parseAddressSpace("not"));
}

TEST(KernelArgMetadata, WhenParseTypeQualifiersIsCalledThenQualifierIsProperlyParsed) {
    using namespace NEO::KernelArgMetadata;

    TypeQualifiers qual = {};
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("").packed);

    qual = {};
    qual.constQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("const").packed);

    qual = {};
    qual.volatileQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("volatile").packed);

    qual = {};
    qual.restrictQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("restrict").packed);

    qual = {};
    qual.pipeQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe").packed);

    qual = {};
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("inval").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("cons").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("volat").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("restr").packed);
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pip").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("const volatile").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    qual.restrictQual = true;
    qual.pipeQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe const restrict volatile").packed);

    qual = {};
    qual.constQual = true;
    qual.volatileQual = true;
    qual.restrictQual = true;
    qual.pipeQual = true;
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, KernelArgMetadata::parseTypeQualifiers("pipe const restrict volatile some").packed);
}

TEST(KernelArgMetadata, WhenParseLimitedStringIsCalledThenReturnedStringDoesntContainExcessiveTrailingZeroes) {
    char str1[] = "abcd\0\0\0after\0";
    EXPECT_STREQ("abcd", NEO::parseLimitedString(str1, sizeof(str1)).c_str());
    EXPECT_EQ(4U, NEO::parseLimitedString(str1, sizeof(str1)).size());

    EXPECT_STREQ("ab", NEO::parseLimitedString(str1, 2).c_str());
    EXPECT_EQ(2U, NEO::parseLimitedString(str1, 2).size());

    char str2[] = {'a', 'b', 'd', 'e', 'f'};
    EXPECT_STREQ("abdef", NEO::parseLimitedString(str2, sizeof(str2)).c_str());
    EXPECT_EQ(5U, NEO::parseLimitedString(str2, sizeof(str2)).size());
}
