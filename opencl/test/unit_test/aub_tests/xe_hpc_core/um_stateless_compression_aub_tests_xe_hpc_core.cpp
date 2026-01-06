/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

class UmStatelessCompression : public AUBFixture,
                               public ::testing::Test,
                               public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        compareCompressedMemory = GetParam();
        AUBFixture::setUp(defaultHwInfo.get());
    }
    void TearDown() override {
        AUBFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
    bool compareCompressedMemory = false;
};

XE_HPC_CORETEST_P(UmStatelessCompression, givenDeviceMemAllocWhenStatelessCompressionIsEnabledThenAllocationDataIsCompressed) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(this->context, this->device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    if (compareCompressedMemory) {
        expectCompressedMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize);
    } else {
        expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize);
    }

    auto hostMemAlloc = clHostMemAllocINTEL(this->context, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, hostMemAlloc);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, hostMemAlloc, deviceMemAlloc, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize);

    retVal = clMemFreeINTEL(this->context, deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(this->context, hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(,
                         UmStatelessCompression,
                         ::testing::Bool());

class UmStatelessCompressionWithBlitter : public MulticontextOclAubFixture,
                                          public ::testing::Test,
                                          public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        debugManager.flags.EnableBlitterOperationsSupport.set(1);
        debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        compareCompressedMemory = GetParam();

        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, true);
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
    bool compareCompressedMemory = false;
};

XE_HPC_CORETEST_P(UmStatelessCompressionWithBlitter, givenDeviceMemAllocWhenItIsAccessedWithBlitterThenProgramBlitterWithCompressionSettings) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(context.get(), tileDevices[0], nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    if (compareCompressedMemory) {
        expectMemoryCompressed<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize, 0, 0);
    } else {
        expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize, 0, 0);
    }

    auto hostMemAlloc = clHostMemAllocINTEL(context.get(), nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, hostMemAlloc);

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, hostMemAlloc, deviceMemAlloc, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize, 0, 0);

    retVal = clMemFreeINTEL(context.get(), deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(,
                         UmStatelessCompressionWithBlitter,
                         ::testing::Bool());

class UmStatelessCompressionWithStatefulAccess : public ProgramFixture,
                                                 public MulticontextOclAubFixture,
                                                 public ::testing::Test,
                                                 public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        debugManager.flags.EnableSharedSystemUsmSupport.set(0);
        debugManager.flags.FailBuildProgramWithStatefulAccess.set(0);

        compareCompressedMemory = GetParam();

        ProgramFixture::setUp();
        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, true);
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
        ProgramFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
    bool compareCompressedMemory = false;
};

XE_HPC_CORETEST_P(UmStatelessCompressionWithStatefulAccess, givenDeviceMemAllocWhenItIsAccessedStatefullyThenProgramStateWithCompressionSettings) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(context.get(), tileDevices[0], nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    if (compareCompressedMemory) {
        expectMemoryCompressed<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize, 0, 0);
    } else {
        expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize, 0, 0);
    }

    auto hostMemAlloc = clHostMemAllocINTEL(context.get(), nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, hostMemAlloc);

    createProgramFromBinary(context.get(), context->getDevices(), "stateful_copy_buffer");

    retVal = pProgram->build(context->getDevices(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *pKernelInfo = pProgram->getKernelInfo("StatefulCopyBuffer", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);

    auto pMultiDeviceKernel = clUniquePtr(MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pMultiDeviceKernel);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 0, deviceMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 1, hostMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = commandQueues[0][0]->enqueueKernel(pMultiDeviceKernel->getKernel(rootDeviceIndex), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    commandQueues[0][0]->finish(false);

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize, 0, 0);

    retVal = clMemFreeINTEL(context.get(), deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_SUITE_P(,
                         UmStatelessCompressionWithStatefulAccess,
                         ::testing::Bool());
