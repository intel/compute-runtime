/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "test.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/program/program_with_source.h"
#include "unit_tests/program/program_tests.h"

using namespace OCLRT;

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

        cl_device_id device = pPlatform->getDevice(0);
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
