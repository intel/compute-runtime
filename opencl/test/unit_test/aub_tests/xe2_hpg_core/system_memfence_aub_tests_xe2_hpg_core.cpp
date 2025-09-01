/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
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

using SystemMemFenceViaMiMemFenceXe2HpgCore = SystemMemFenceViaMiMemFence;

XE2_HPG_CORETEST_F(SystemMemFenceViaMiMemFenceXe2HpgCore, WhenGeneratedAsMiMemFenceCommandInCommandStreamThenWritesToSystemMemoryAreGloballyObservable) {
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

using SystemMemFenceViaComputeWalkerXe2HpgCore = SystemMemFenceViaComputeWalker;

XE2_HPG_CORETEST_F(SystemMemFenceViaComputeWalkerXe2HpgCore, WhenGeneratedAsPostSyncOperationInWalkerThenWritesToSystemMemoryAreGloballyObservable) {
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

class SystemMemFenceWithBlitterXe2HpgCore : public MulticontextOclAubFixture,
                                            public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);

        debugManager.flags.EnableBlitterOperationsSupport.set(1);
        debugManager.flags.EnableBlitterForEnqueueOperations.set(1);
        MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
        if (!productHelper.obtainBlitterPreference(*defaultHwInfo.get())) {
            GTEST_SKIP();
        }

        MulticontextOclAubFixture::setUp(1, EnabledCommandStreamers::single, false);
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
    }

    DebugManagerStateRestore debugRestorer;
    cl_int retVal = CL_SUCCESS;
};

XE2_HPG_CORETEST_F(SystemMemFenceWithBlitterXe2HpgCore, givenSystemMemFenceWhenGeneratedAsMiMemFenceCmdInBCSThenWritesToSystemMemoryAreGloballyObservable) {
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

    retVal = clEnqueueMemcpyINTEL(commandQueues[0][0].get(), true, hostMemAlloc, deviceMemAlloc, bufferSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(hostMemAlloc, buffer.data(), bufferSize, 0, 0);

    retVal = clMemFreeINTEL(context.get(), deviceMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clMemFreeINTEL(context.get(), hostMemAlloc);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
