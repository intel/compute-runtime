/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

class SystemMemFenceViaMiMemFence : public AUBFixture,
                                    public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
        AUBFixture::setUp(defaultHwInfo.get());
    }
    void TearDown() override {
        AUBFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

HWTEST2_F(SystemMemFenceViaMiMemFence, WhenGeneratedAsMiMemFenceCommandInCommandStreamThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(this->context, this->device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize);

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

class SystemMemFenceViaComputeWalker : public AUBFixture,
                                       public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
        AUBFixture::setUp(defaultHwInfo.get());
    }
    void TearDown() override {
        AUBFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

HWTEST2_F(SystemMemFenceViaComputeWalker, WhenGeneratedAsPostSyncOperationInWalkerThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(this->context, this->device, nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize);

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

class SystemMemFenceBlitter : public MulticontextOclAubFixture,
                              public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);

        debugManager.flags.EnableBlitterOperationsSupport.set(1);
        debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        if (defaultHwInfo->platform.eProductFamily == IGFX_CRI) {
            debugManager.flags.BlitterEnableMaskOverride.set(6);
        }

        MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
        if (!productHelper.obtainBlitterPreference(*defaultHwInfo.get())) {
            GTEST_SKIP();
        }

        const auto &releaseHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getReleaseHelper();
        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, releaseHelper.getFtrXe2Compression());
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

HWTEST2_F(SystemMemFenceBlitter, givenSystemMemFenceWhenGeneratedAsMiMemFenceCmdInBCSThenWritesToSystemMemoryAreGloballyObservable, IsAtLeastXeHpcCore) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(context.get(), tileDevices[0], nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    if (!tileDevices[0]->getDevice().getReleaseHelper().getFtrXe2Compression()) {
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

class SystemMemFenceViaKernel : public ProgramFixture,
                                public MulticontextOclAubFixture,
                                public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);

        ProgramFixture::setUp();
        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, true);
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
        ProgramFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

HWTEST2_F(SystemMemFenceViaKernel, givenSystemMemFenceWhenKernelInstructionThenWritesToSystemMemoryAreGloballyObservable, IsXeHpcCore) {
    const size_t bufferSize = MemoryConstants::kiloByte;
    std::vector<char> buffer(bufferSize, 0x11);

    auto deviceMemAlloc = clDeviceMemAllocINTEL(context.get(), tileDevices[0], nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, deviceMemAlloc);

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, deviceMemAlloc, buffer.data(), bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(deviceMemAlloc, buffer.data(), bufferSize, 0, 0);

    auto hostMemAlloc = clHostMemAllocINTEL(context.get(), nullptr, bufferSize, 0, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, hostMemAlloc);

    createProgramFromBinary(context.get(), context->getDevices(), "system_memfence");

    retVal = pProgram->build(pProgram->getDevices(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    const KernelInfo *pKernelInfo = pProgram->getKernelInfo("SystemMemFence", rootDeviceIndex);
    ASSERT_NE(nullptr, pKernelInfo);

    auto pMultiDeviceKernel = clUniquePtr(MultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex), retVal));
    ASSERT_NE(nullptr, pMultiDeviceKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 0, deviceMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = clSetKernelArgSVMPointer(pMultiDeviceKernel.get(), 1, hostMemAlloc);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t globalWorkSize[3] = {bufferSize / sizeof(cl_int), 1, 1};
    retVal = commandQueues[0][0]->enqueueKernel(pMultiDeviceKernel->getKernel(rootDeviceIndex), 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    commandQueues[0][0]->finish(false);

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize, 0, 0);

    retVal = clMemFreeINTEL(context.get(), deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
