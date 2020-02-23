/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

struct ProfilingTestsGen12LP : public CommandEnqueueFixture,
                               public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::SetUp(CL_QUEUE_PROFILING_ENABLE);
        mockKernelWithInternals = std::make_unique<MockKernelWithInternals>(*pClDevice, nullptr);
    }

    void TearDown() override {
        CommandEnqueueFixture::TearDown();
    }

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternals;
};

GEN12LPTEST_F(ProfilingTestsGen12LP, GivenCommandQueueWithProflingWhenWalkerIsDispatchedThenTwoMiStoreRegisterMemWithMmioRemapEnableArePresentInCS) {
    typedef typename FamilyType::MI_STORE_REGISTER_MEM MI_STORE_REGISTER_MEM;
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    uint32_t dimensions = 1;
    cl_event event;

    static_cast<CommandQueueHw<FamilyType> *>(pCmdQ)->enqueueKernel(
        *mockKernelWithInternals,
        dimensions,
        globalOffsets,
        workItems,
        nullptr,
        0,
        nullptr,
        &event);

    parseCommands<FamilyType>(*pCmdQ);

    // Find GPGPU_WALKER
    auto itorGPGPUWalkerCmd = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
    GenCmdList::reverse_iterator rItorGPGPUWalkerCmd(itorGPGPUWalkerCmd);
    ASSERT_NE(cmdList.end(), itorGPGPUWalkerCmd);

    // Check MI_STORE_REGISTER_MEMs
    auto itorBeforeMI = reverse_find<MI_STORE_REGISTER_MEM *>(rItorGPGPUWalkerCmd, cmdList.rbegin());
    ASSERT_NE(cmdList.rbegin(), itorBeforeMI);
    auto pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    pBeforeMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorBeforeMI);
    ASSERT_NE(nullptr, pBeforeMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pBeforeMI->getRegisterAddress());
    EXPECT_TRUE(pBeforeMI->getMmioRemapEnable());

    auto itorAfterMI = find<MI_STORE_REGISTER_MEM *>(itorGPGPUWalkerCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), itorAfterMI);
    auto pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    ASSERT_NE(nullptr, pAfterMI);
    EXPECT_EQ(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, pAfterMI->getRegisterAddress());
    EXPECT_TRUE(pAfterMI->getMmioRemapEnable());

    ++itorAfterMI;
    pAfterMI = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorAfterMI);
    EXPECT_EQ(nullptr, pAfterMI);

    clReleaseEvent(event);
}
