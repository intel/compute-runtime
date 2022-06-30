/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

class UmStatelessCompression : public AUBFixture,
                               public ::testing::Test,
                               public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        compareCompressedMemory = GetParam();
        AUBFixture::SetUp(defaultHwInfo.get());
    }
    void TearDown() override {
        AUBFixture::TearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
    bool compareCompressedMemory = false;
};

XE_HPC_CORETEST_P(UmStatelessCompression, givenDeviceMemAllocWhenStatelessCompressionIsEnabledThenAllocationDataIsCompressed) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(this->context, this->device.get(), nullptr, bufferSize, 0, &retVal);
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

INSTANTIATE_TEST_CASE_P(,
                        UmStatelessCompression,
                        ::testing::Bool());

class UmStatelessCompressionWithBlitter : public MulticontextAubFixture,
                                          public ::testing::Test,
                                          public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        DebugManager.flags.EnableBlitterOperationsSupport.set(1);
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        compareCompressedMemory = GetParam();

        MulticontextAubFixture::SetUp(1, EnabledCommandStreamers::Single, true);
    }
    void TearDown() override {
        MulticontextAubFixture::TearDown();
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

INSTANTIATE_TEST_CASE_P(,
                        UmStatelessCompressionWithBlitter,
                        ::testing::Bool());

class UmStatelessCompressionWithStatefulAccess : public ProgramFixture,
                                                 public MulticontextAubFixture,
                                                 public ::testing::Test,
                                                 public ::testing::WithParamInterface<bool /*compareCompressedMemory*/> {
  public:
    void SetUp() override {
        DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);
        DebugManager.flags.EnableSharedSystemUsmSupport.set(0);
        compareCompressedMemory = GetParam();

        ProgramFixture::SetUp();
        MulticontextAubFixture::SetUp(1, EnabledCommandStreamers::Single, true);
    }
    void TearDown() override {
        MulticontextAubFixture::TearDown();
        ProgramFixture::TearDown();
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

    CreateProgramFromBinary(context.get(), context->getDevices(), "stateful_copy_buffer");

    retVal = pProgram->build(context->getDevices(), nullptr, false);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *pKernelInfo = pProgram->getKernelInfo("StatefulCopyBuffer", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);

    auto pMultiDeviceKernel = clUniquePtr(MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), &retVal));
    ASSERT_NE(nullptr, pMultiDeviceKernel);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 0, deviceMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 1, hostMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize, 1, 1};
    retVal = commandQueues[0][0]->enqueueKernel(pMultiDeviceKernel->getKernel(rootDeviceIndex), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    commandQueues[0][0]->finish();

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize, 0, 0);

    retVal = clMemFreeINTEL(context.get(), deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

INSTANTIATE_TEST_CASE_P(,
                        UmStatelessCompressionWithStatefulAccess,
                        ::testing::Bool());
