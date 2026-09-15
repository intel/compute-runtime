/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

class PatchedKernelTest : public ::testing::Test {
  public:
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
        context.reset(new MockContext(device.get()));

        mockKernelWithInternals = std::make_unique<MockKernelWithInternals>(*context);
        auto &kernelInfo = mockKernelWithInternals->kernelInfo;
        kernelInfo.kernelDescriptor.kernelAttributes.numArgsToPatch = 3;
        kernelInfo.addArgBuffer(0, 0, sizeof(uintptr_t), 64);
        kernelInfo.setAddressQualifier(0, KernelArgMetadata::AddrGlobal);
        kernelInfo.setAccessQualifier(0, KernelArgMetadata::AccessReadWrite);
        kernelInfo.addArgImmediate(1, sizeof(uint32_t), 8);
        kernelInfo.addArgBuffer(2, 16, sizeof(uintptr_t), 0);
        kernelInfo.setAddressQualifier(2, KernelArgMetadata::AddrGlobal);
        kernelInfo.setAccessQualifier(2, KernelArgMetadata::AccessReadWrite);
        mockKernelWithInternals->mockKernel->initialize();

        kernel.reset(*mockKernelWithInternals);
    }
    void TearDown() override {
        kernel.release();
        mockKernelWithInternals.reset();
        context.reset();
    }

    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternals;
    std::unique_ptr<Kernel> kernel;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(PatchedKernelTest, givenKernelWithoutPatchedArgsWhenIsPatchedIsCalledThenReturnsFalse) {
    EXPECT_FALSE(kernel->Kernel::isPatched());
}

TEST_F(PatchedKernelTest, givenKernelWithAllArgsSetWithBufferWhenIsPatchedIsCalledThenReturnsTrue) {
    auto buffer = clCreateBuffer(context.get(), CL_MEM_READ_ONLY, sizeof(int), nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    kernel->setArg(0, buffer);
    uint32_t immArgValue = 0x12345678;
    kernel->setArg(1, immArgValue);
    kernel->setArg(2, buffer);

    EXPECT_TRUE(kernel->Kernel::isPatched());
    clReleaseMemObject(buffer);
}

TEST_F(PatchedKernelTest, givenKernelWithoutAllArgsSetWhenIsPatchedIsCalledThenReturnsFalse) {
    auto buffer = clCreateBuffer(context.get(), CL_MEM_READ_ONLY, sizeof(int), nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto argsNum = kernel->getKernelArgsNumber();
    for (uint32_t i = 0; i < argsNum; i++) {
        kernel->setArg(0, buffer);
    }
    EXPECT_FALSE(kernel->Kernel::isPatched());
    clReleaseMemObject(buffer);
}

TEST_F(PatchedKernelTest, givenArgSvmAllocWhenArgIsSetThenArgIsPatched) {
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    EXPECT_FALSE(kernel->getKernelArguments()[0].isPatched);
    kernel->setArgSvmAlloc(0, nullptr, nullptr, 0u);
    EXPECT_TRUE(kernel->getKernelArguments()[0].isPatched);
}

TEST_F(PatchedKernelTest, givenArgSvmWhenArgIsSetThenArgIsPatched) {
    uint32_t size = sizeof(int);
    EXPECT_FALSE(kernel->getKernelArguments()[0].isPatched);
    kernel->setArgSvm(0, size, nullptr, nullptr, 0);
    EXPECT_TRUE(kernel->getKernelArguments()[0].isPatched);
}

TEST_F(PatchedKernelTest, givenKernelWithOneArgumentToPatchWhichIsNonzeroIndexedWhenThatArgumentIsSetThenKernelIsPatched) {
    uint32_t size = sizeof(int);
    MockKernelWithInternals mockKernel(*context);
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.numArgsToPatch = 1;
    mockKernel.kernelInfo.addArgBuffer(1, 0);

    kernel.release();
    kernel.reset(mockKernel.mockKernel);
    kernel->initialize();
    EXPECT_FALSE(kernel->Kernel::isPatched());
    kernel->setArgSvm(1, size, nullptr, nullptr, 0u);
    EXPECT_TRUE(kernel->Kernel::isPatched());
    kernel.release();
}
