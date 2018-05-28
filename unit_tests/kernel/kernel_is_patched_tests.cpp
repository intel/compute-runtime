/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "gtest/gtest.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace OCLRT;

class PatchedKernelTest : public ::testing::Test {
  public:
    void SetUp() override {
        device.reset(MockDevice::create<MockDevice>(nullptr));
        context.reset(new MockContext(device.get()));
        program.reset(Program::create("FillBufferBytes", context.get(), *device.get(), true, &retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        cl_device_id clDevice = device.get();
        program->build(1, &clDevice, nullptr, nullptr, nullptr, false);
        kernel.reset(Kernel::create(program.get(), *program->getKernelInfo("FillBufferBytes"), &retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
    void TearDown() override {
        context.reset();
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<Program> program;
    std::unique_ptr<Kernel> kernel;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(PatchedKernelTest, givenKernelWithoutPatchedArgsWhenIsPatchedIsCalledThenReturnsFalse) {
    EXPECT_FALSE(kernel->isPatched());
}

TEST_F(PatchedKernelTest, givenKernelWithAllArgsSetWithBufferWhenIsPatchedIsCalledThenReturnsTrue) {
    auto buffer = clCreateBuffer(context.get(), CL_MEM_READ_ONLY, sizeof(int), nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto argsNum = kernel->getKernelArgsNumber();
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArg(i, buffer);
    }
    EXPECT_TRUE(kernel->isPatched());
    clReleaseMemObject(buffer);
}

TEST_F(PatchedKernelTest, givenKernelWithoutAllArgsSetWhenIsPatchedIsCalledThenReturnsFalse) {
    auto buffer = clCreateBuffer(context.get(), CL_MEM_READ_ONLY, sizeof(int), nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto argsNum = kernel->getKernelArgsNumber();
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArg(0, buffer);
    }
    EXPECT_FALSE(kernel->isPatched());
    clReleaseMemObject(buffer);
}

TEST_F(PatchedKernelTest, givenKernelWithAllArgsSetWithSvmAllocWhenIsPatchedIsCalledThenReturnsTrue) {
    auto argsNum = kernel->getKernelArgsNumber();
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArgSvmAlloc(0, nullptr, nullptr);
    }
    EXPECT_FALSE(kernel->isPatched());
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArgSvmAlloc(i, nullptr, nullptr);
    }
    EXPECT_TRUE(kernel->isPatched());
}

TEST_F(PatchedKernelTest, givenKernelWithAllArgsSetWithSvmWhenIsPatchedIsCalledThenReturnsTrue) {
    uint32_t size = sizeof(int);
    auto argsNum = kernel->getKernelArgsNumber();
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArgSvm(0, size, nullptr, nullptr);
    }
    EXPECT_FALSE(kernel->isPatched());
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArgSvm(i, size, nullptr, nullptr);
    }
    EXPECT_TRUE(kernel->isPatched());
}

TEST_F(PatchedKernelTest, givenKernelWithOneArgumentToPatchWhichIsNonzeroIndexedWhenThatArgumentIsSetThenKernelIsPatched) {
    uint32_t size = sizeof(int);
    MockKernelWithInternals mockKernel(*device.get(), context.get());
    EXPECT_EQ(0u, mockKernel.kernelInfo.argumentsToPatchNum);
    mockKernel.kernelInfo.storeKernelArgPatchInfo(1, 0, 0, 0, 0);
    EXPECT_EQ(1u, mockKernel.kernelInfo.argumentsToPatchNum);
    mockKernel.kernelInfo.storeKernelArgPatchInfo(1, 0, 0, 0, 0);
    EXPECT_EQ(1u, mockKernel.kernelInfo.argumentsToPatchNum);
    kernel.reset(mockKernel.mockKernel);
    kernel->initialize();
    EXPECT_FALSE(kernel->Kernel::isPatched());
    kernel->setArgSvm(1, size, nullptr, nullptr);
    EXPECT_TRUE(kernel->Kernel::isPatched());
    kernel.release();
}
