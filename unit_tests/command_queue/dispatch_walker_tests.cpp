/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/libult/mock_gfx_family.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "hw_cmds.h"

using namespace OCLRT;

struct DispatchWalkerTest : public CommandQueueFixture, public DeviceFixture, public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(nullptr, pDevice, 0);

        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());

        memset(&kernelHeader, 0, sizeof(kernelHeader));
        kernelHeader.KernelHeapSize = sizeof(kernelIsa);

        memset(&dataParameterStream, 0, sizeof(dataParameterStream));
        dataParameterStream.DataParameterStreamSize = sizeof(crossThreadData);

        executionEnvironment = {};
        memset(&executionEnvironment, 0, sizeof(executionEnvironment));
        executionEnvironment.CompiledSIMD32 = 1;
        executionEnvironment.LargestCompiledSIMDSize = 32;

        memset(&threadPayload, 0, sizeof(threadPayload));
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;

        samplerArray.BorderColorOffset = 0;
        samplerArray.Count = 1;
        samplerArray.Offset = 4;
        samplerArray.Size = 2;
        samplerArray.Token = 0;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfo.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;

        kernelInfoWithSampler.heapInfo.pKernelHeap = kernelIsa;
        kernelInfoWithSampler.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfoWithSampler.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfoWithSampler.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfoWithSampler.patchInfo.threadPayload = &threadPayload;
        kernelInfoWithSampler.patchInfo.samplerStateArray = &samplerArray;
        kernelInfoWithSampler.heapInfo.pDsh = static_cast<const void *>(dsh);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    std::unique_ptr<MockProgram> program;

    SKernelBinaryHeaderCommon kernelHeader = {};
    SPatchDataParameterStream dataParameterStream = {};
    SPatchExecutionEnvironment executionEnvironment = {};
    SPatchThreadPayload threadPayload = {};
    SPatchSamplerStateArray samplerArray = {};

    KernelInfo kernelInfo;
    KernelInfo kernelInfoWithSampler;

    uint32_t kernelIsa[32];
    uint32_t crossThreadData[32];
    uint32_t dsh[32];
};

HWTEST_F(DispatchWalkerTest, computeDimensions) {
    const size_t workItems1D[] = {100, 1, 1};
    EXPECT_EQ(1u, computeDimensions(workItems1D));

    const size_t workItems2D[] = {100, 100, 1};
    EXPECT_EQ(2u, computeDimensions(workItems2D));

    const size_t workItems3D[] = {100, 100, 100};
    EXPECT_EQ(3u, computeDimensions(workItems3D));
}

HWTEST_F(DispatchWalkerTest, shouldntChangeCommandStreamMemory) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   KernelCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

    //cs has a minimum required size
    auto sizeThatNeedsToBeSubstracted = sizeDispatchWalkerNeeds + CSRequirements::minCommandQueueCommandStreamSize;

    commandStream.getSpace(commandStream.getMaxAvailableSpace() - sizeThatNeedsToBeSubstracted);
    ASSERT_EQ(commandStream.getAvailableSpace(), sizeThatNeedsToBeSubstracted);

    auto commandStreamStart = commandStream.getUsed();
    auto commandStreamBuffer = commandStream.getCpuBase();
    ASSERT_NE(0u, commandStreamStart);

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    cl_uint dimensions = 1;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    EXPECT_EQ(commandStreamBuffer, commandStream.getCpuBase());
    EXPECT_LT(commandStreamStart, commandStream.getUsed());
    EXPECT_EQ(sizeDispatchWalkerNeeds, commandStream.getUsed() - commandStreamStart);
}

HWTEST_F(DispatchWalkerTest, noLocalIdsShouldntCrash) {
    threadPayload.LocalIDXPresent = 0;
    threadPayload.LocalIDYPresent = 0;
    threadPayload.LocalIDZPresent = 0;
    threadPayload.UnusedPerThreadConstantPresent = 1;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   KernelCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

    //cs has a minimum required size
    auto sizeThatNeedsToBeSubstracted = sizeDispatchWalkerNeeds + CSRequirements::minCommandQueueCommandStreamSize;

    commandStream.getSpace(commandStream.getMaxAvailableSpace() - sizeThatNeedsToBeSubstracted);
    ASSERT_EQ(commandStream.getAvailableSpace(), sizeThatNeedsToBeSubstracted);

    auto commandStreamStart = commandStream.getUsed();
    auto commandStreamBuffer = commandStream.getCpuBase();
    ASSERT_NE(0u, commandStreamStart);

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    cl_uint dimensions = 1;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    EXPECT_EQ(commandStreamBuffer, commandStream.getCpuBase());
    EXPECT_LT(commandStreamStart, commandStream.getUsed());
    EXPECT_EQ(sizeDispatchWalkerNeeds, commandStream.getUsed() - commandStreamStart);
}

HWTEST_F(DispatchWalkerTest, dataParameterWorkDimensionswithDefaultLwsAlgorithm) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;

        DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, dataParameterWorkDimensionswithSquaredLwsAlgorithm) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, dataParameterWorkDimensionswithNDLwsAlgorithm) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, dataParameterWorkDimensionswithOldLwsAlgorithm) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, dataParameterNumWorkGroups) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.numWorkGroupsOffset[0] = 0;
    kernelInfo.workloadInfo.numWorkGroupsOffset[1] = 4;
    kernelInfo.workloadInfo.numWorkGroupsOffset[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 1, 1};
    cl_uint dimensions = 3;

    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    EXPECT_EQ(2u, *kernel.numWorkGroupsX);
    EXPECT_EQ(5u, *kernel.numWorkGroupsY);
    EXPECT_EQ(10u, *kernel.numWorkGroupsZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterNoLocalWorkSizeWithOutComputeND) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterNoLocalWorkSizeWithComputeND) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(10u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterNoLocalWorkSizeWithComputeSquared) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterNoLocalWorkSizeWithOutComputeSquaredAndND) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterLocalWorkSize) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 2, 3};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(1u, *kernel.localWorkSizeX);
    EXPECT_EQ(2u, *kernel.localWorkSizeY);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, dataParameterLocalWorkSizes) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[0] = 12;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[1] = 16;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[2] = 20;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 2, 3};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);
    EXPECT_EQ(1u, *kernel.localWorkSizeX);
    EXPECT_EQ(2u, *kernel.localWorkSizeY);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ);
    EXPECT_EQ(1u, *kernel.localWorkSizeX2);
    EXPECT_EQ(2u, *kernel.localWorkSizeY2);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ2);
}

HWTEST_F(DispatchWalkerTest, dataParameterLocalWorkSizeForSplitKernel) {
    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());

    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pDevice);
    kernelInfoWithSampler.workloadInfo.localWorkSizeOffsets[0] = 12;
    kernelInfoWithSampler.workloadInfo.localWorkSizeOffsets[1] = 16;
    kernelInfoWithSampler.workloadInfo.localWorkSizeOffsets[2] = 20;
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    DispatchInfo di1(&kernel1, 3, {10, 10, 10}, {1, 2, 3}, {0, 0, 0});
    DispatchInfo di2(&kernel2, 3, {10, 10, 10}, {4, 5, 6}, {0, 0, 0});

    MockMultiDispatchInfo multiDispatchInfo(std::vector<DispatchInfo *>({&di1, &di2}));

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    auto dispatchId = 0;
    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();
        if (dispatchId == 0) {
            EXPECT_EQ(1u, *kernel.localWorkSizeX);
            EXPECT_EQ(2u, *kernel.localWorkSizeY);
            EXPECT_EQ(3u, *kernel.localWorkSizeZ);
        }
        if (dispatchId == 1) {
            EXPECT_EQ(4u, *kernel.localWorkSizeX);
            EXPECT_EQ(5u, *kernel.localWorkSizeY);
            EXPECT_EQ(6u, *kernel.localWorkSizeZ);
        }
        dispatchId++;
    }
}

HWTEST_F(DispatchWalkerTest, dataParameterLocalWorkSizesForSplitWalker) {
    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    MockKernel mainKernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[0] = 12;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[1] = 16;
    kernelInfo.workloadInfo.localWorkSizeOffsets2[2] = 20;
    kernelInfo.workloadInfo.numWorkGroupsOffset[0] = 24;
    kernelInfo.workloadInfo.numWorkGroupsOffset[1] = 28;
    kernelInfo.workloadInfo.numWorkGroupsOffset[2] = 32;
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    ASSERT_EQ(CL_SUCCESS, mainKernel.initialize());

    DispatchInfo di1(&kernel1, 3, {10, 10, 10}, {1, 2, 3}, {0, 0, 0});
    DispatchInfo di2(&mainKernel, 3, {10, 10, 10}, {4, 5, 6}, {0, 0, 0});

    MultiDispatchInfo multiDispatchInfo(&mainKernel);
    multiDispatchInfo.push(di1);
    multiDispatchInfo.push(di2);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();
        if (&kernel == &mainKernel) {
            EXPECT_EQ(4u, *kernel.localWorkSizeX);
            EXPECT_EQ(5u, *kernel.localWorkSizeY);
            EXPECT_EQ(6u, *kernel.localWorkSizeZ);
            EXPECT_EQ(4u, *kernel.localWorkSizeX2);
            EXPECT_EQ(5u, *kernel.localWorkSizeY2);
            EXPECT_EQ(6u, *kernel.localWorkSizeZ2);
            EXPECT_EQ(3u, *kernel.numWorkGroupsX);
            EXPECT_EQ(2u, *kernel.numWorkGroupsY);
            EXPECT_EQ(2u, *kernel.numWorkGroupsZ);
        } else {
            EXPECT_EQ(0u, *kernel.localWorkSizeX);
            EXPECT_EQ(0u, *kernel.localWorkSizeY);
            EXPECT_EQ(0u, *kernel.localWorkSizeZ);
            EXPECT_EQ(1u, *kernel.localWorkSizeX2);
            EXPECT_EQ(2u, *kernel.localWorkSizeY2);
            EXPECT_EQ(3u, *kernel.localWorkSizeZ2);
            EXPECT_EQ(0u, *kernel.numWorkGroupsX);
            EXPECT_EQ(0u, *kernel.numWorkGroupsY);
            EXPECT_EQ(0u, *kernel.numWorkGroupsZ);
        }
    }
}

HWTEST_F(DispatchWalkerTest, dispatchWalkerDoesntConsumeCommandStreamWhenQueueIsBlocked) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    size_t workGroupSize[3] = {2, 5, 10};
    cl_uint dimensions = 1;

    //block the queue
    auto blockQueue = true;

    KernelOperation *blockedCommandsData = nullptr;

    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        &blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        blockQueue);

    auto &commandStream = pCmdQ->getCS(1024);
    EXPECT_EQ(0u, commandStream.getUsed());
    EXPECT_NE(nullptr, blockedCommandsData);
    EXPECT_NE(nullptr, blockedCommandsData->commandStream);
    EXPECT_NE(nullptr, blockedCommandsData->dsh);
    EXPECT_NE(nullptr, blockedCommandsData->ioh);
    EXPECT_NE(nullptr, blockedCommandsData->ssh);

    delete blockedCommandsData;
}

HWTEST_F(DispatchWalkerTest, dispatchWalkerShouldGetRequiredHeapSizesFromKernelWhenQueueIsBlocked) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    size_t workGroupSize[3] = {2, 5, 10};
    cl_uint dimensions = 1;

    //block the queue
    auto blockQueue = true;

    KernelOperation *blockedCommandsData = nullptr;

    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        &blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        blockQueue);

    Vec3<size_t> localWorkgroupSize(workGroupSize);

    auto expectedSizeCS = MemoryConstants::pageSize; //can get estimated more precisely
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getSizeRequiredDSH(kernel);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getSizeRequiredIOH(kernel, Math::computeTotalElementsCount(localWorkgroupSize));
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getSizeRequiredSSH(kernel);

    EXPECT_EQ(expectedSizeCS, blockedCommandsData->commandStream->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeDSH, blockedCommandsData->dsh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeIOH, blockedCommandsData->ioh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeSSH, blockedCommandsData->ssh->getMaxAvailableSpace());

    delete blockedCommandsData;
}

HWTEST_F(DispatchWalkerTest, dispatchWalkerShouldGetRequiredHeapSizesFromMdiWhenQueueIsBlocked) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    MockMultiDispatchInfo multiDispatchInfo(&kernel);

    //block the queue
    auto blockQueue = true;

    KernelOperation *blockedCommandsData = nullptr;

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        &blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        blockQueue);

    auto expectedSizeCS = MemoryConstants::pageSize; //can get estimated more precisely
    auto expectedSizeDSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_EQ(expectedSizeCS, blockedCommandsData->commandStream->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeDSH, blockedCommandsData->dsh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeIOH, blockedCommandsData->ioh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeSSH, blockedCommandsData->ssh->getMaxAvailableSpace());

    delete blockedCommandsData;
}

HWTEST_F(DispatchWalkerTest, dispatchWalkerWithMultipleDispatchInfo) {
    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1, &kernel2}));

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();
        EXPECT_EQ(*kernel.workDim, dispatchInfo.getDim());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, dispatchWalkerWithMultipleDispatchInfoCorrectlyProgramsInterfaceDesriptors) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    auto memoryManager = this->pDevice->getMemoryManager();
    auto kernelIsaAllocation = memoryManager->allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    auto kernelIsaWithSamplerAllocation = memoryManager->allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    kernelInfo.kernelAllocation = kernelIsaAllocation;
    kernelInfoWithSampler.kernelAllocation = kernelIsaWithSamplerAllocation;
    auto gpuAddress1 = kernelIsaAllocation->getGpuAddressToPatch();
    auto gpuAddress2 = kernelIsaWithSamplerAllocation->getGpuAddressToPatch();

    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1, &kernel2}));

    // create Indirect DSH heap
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);

    indirectHeap.align(KernelCommandsHelper<FamilyType>::alignInterfaceDescriptorData);
    auto dshBeforeMultiDisptach = indirectHeap.getUsed();

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    auto dshAfterMultiDisptach = indirectHeap.getUsed();

    auto numberOfDispatches = multiDispatchInfo.size();
    auto interfaceDesriptorTableSize = numberOfDispatches * sizeof(INTERFACE_DESCRIPTOR_DATA);

    EXPECT_LE(dshBeforeMultiDisptach + interfaceDesriptorTableSize, dshAfterMultiDisptach);

    INTERFACE_DESCRIPTOR_DATA *pID = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(indirectHeap.getCpuBase(), dshBeforeMultiDisptach));

    for (uint32_t index = 0; index < multiDispatchInfo.size(); index++) {
        uint32_t addressLow = pID[index].getKernelStartPointer();
        uint32_t addressHigh = pID[index].getKernelStartPointerHigh();
        uint64_t fullAddress = ((uint64_t)addressHigh << 32) | addressLow;

        if (index > 0) {
            uint32_t addressLowOfPrevious = pID[index - 1].getKernelStartPointer();
            uint32_t addressHighOfPrevious = pID[index - 1].getKernelStartPointerHigh();

            uint64_t addressPrevious = ((uint64_t)addressHighOfPrevious << 32) | addressLowOfPrevious;
            uint64_t address = ((uint64_t)addressHigh << 32) | addressLow;

            EXPECT_NE(addressPrevious, address);
        }

        if (index == 0) {
            auto samplerPointer = pID[index].getSamplerStatePointer();
            auto samplerCount = pID[index].getSamplerCount();
            EXPECT_EQ(0u, samplerPointer);
            EXPECT_EQ(0u, samplerCount);
            EXPECT_EQ(fullAddress, gpuAddress1);
        }

        if (index == 1) {
            auto samplerPointer = pID[index].getSamplerStatePointer();
            auto samplerCount = pID[index].getSamplerCount();
            EXPECT_NE(0u, samplerPointer);
            EXPECT_EQ(1u, samplerCount);
            EXPECT_EQ(fullAddress, gpuAddress2);
        }
    }

    HardwareParse hwParser;
    auto &cmdStream = pCmdQ->getCS(0);

    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    hwParser.findHardwareCommands<FamilyType>();
    auto cmd = hwParser.getCommand<typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD>();

    EXPECT_NE(nullptr, cmd);

    auto IDStartAddress = cmd->getInterfaceDescriptorDataStartAddress();
    auto IDSize = cmd->getInterfaceDescriptorTotalLength();
    EXPECT_EQ(dshBeforeMultiDisptach, IDStartAddress);
    EXPECT_EQ(interfaceDesriptorTableSize, IDSize);

    memoryManager->freeGraphicsMemory(kernelIsaAllocation);
    memoryManager->freeGraphicsMemory(kernelIsaWithSamplerAllocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, dispatchWalkerWithMultipleDispatchInfoCorrectlyProgramsGpgpuWalkerIDOffset) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1, &kernel2}));

    // create commandStream
    auto &cmdStream = pCmdQ->getCS(0);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto walkerItor = hwParser.itorWalker;

    ASSERT_NE(hwParser.cmdList.end(), walkerItor);

    for (uint32_t index = 0; index < multiDispatchInfo.size(); index++) {
        ASSERT_NE(hwParser.cmdList.end(), walkerItor);

        auto *gpgpuWalker = (GPGPU_WALKER *)*walkerItor;
        auto IDIndex = gpgpuWalker->getInterfaceDescriptorOffset();
        EXPECT_EQ(index, IDIndex);

        // move walker iterator
        walkerItor++;
        walkerItor = find<GPGPU_WALKER *>(walkerItor, hwParser.cmdList.end());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, dispatchWalkerWithMultipleDispatchInfoAndDifferentKernelsCorrectlyProgramsGpgpuWalkerThreadGroupIdStartingCoordinates) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel1(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1, &kernel2}));

    // create commandStream
    auto &cmdStream = pCmdQ->getCS(0);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto walkerItor = hwParser.itorWalker;

    ASSERT_NE(hwParser.cmdList.end(), walkerItor);

    for (uint32_t index = 0; index < multiDispatchInfo.size(); index++) {
        ASSERT_NE(hwParser.cmdList.end(), walkerItor);

        auto *gpgpuWalker = (GPGPU_WALKER *)*walkerItor;
        auto coordinateX = gpgpuWalker->getThreadGroupIdStartingX();
        EXPECT_EQ(coordinateX, 0u);
        auto coordinateY = gpgpuWalker->getThreadGroupIdStartingY();
        EXPECT_EQ(coordinateY, 0u);
        auto coordinateZ = gpgpuWalker->getThreadGroupIdStartingResumeZ();
        EXPECT_EQ(coordinateZ, 0u);

        // move walker iterator
        walkerItor++;
        walkerItor = find<GPGPU_WALKER *>(walkerItor, hwParser.cmdList.end());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, dispatchWalkerWithMultipleDispatchInfoButSameKernelCorrectlyProgramsGpgpuWalkerThreadGroupIdStartingCoordinates) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    DispatchInfo di1(&kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0}, {100, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 1, 1}, {0, 0, 0});
    DispatchInfo di2(&kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0}, {100, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 0, 0});

    MockMultiDispatchInfo multiDispatchInfo(std::vector<DispatchInfo *>({&di1, &di2}));

    // create commandStream
    auto &cmdStream = pCmdQ->getCS(0);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    auto walkerItor = hwParser.itorWalker;

    ASSERT_NE(hwParser.cmdList.end(), walkerItor);

    for (uint32_t index = 0; index < multiDispatchInfo.size(); index++) {
        ASSERT_NE(hwParser.cmdList.end(), walkerItor);

        auto *gpgpuWalker = (GPGPU_WALKER *)*walkerItor;
        auto coordinateX = gpgpuWalker->getThreadGroupIdStartingX();
        EXPECT_EQ(coordinateX, index * 10u);
        auto coordinateY = gpgpuWalker->getThreadGroupIdStartingY();
        EXPECT_EQ(coordinateY, 0u);
        auto coordinateZ = gpgpuWalker->getThreadGroupIdStartingResumeZ();
        EXPECT_EQ(coordinateZ, 0u);

        // move walker iterator
        walkerItor++;
        walkerItor = find<GPGPU_WALKER *>(walkerItor, hwParser.cmdList.end());
    }
}

HWTEST_F(DispatchWalkerTest, givenMultiDispatchWhenWhitelistedRegisterForCoherencySwitchThenDontProgramLriInTaskStream) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    WhitelistedRegisters registers = {0};
    registers.chicken0hdc_0xE5F0 = true;
    pDevice->setForceWhitelistedRegs(true, &registers);
    auto &cmdStream = pCmdQ->getCS(0);
    HardwareParse hwParser;

    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    DispatchInfo di1(&kernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));
    DispatchInfo di2(&kernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));
    MockMultiDispatchInfo multiDispatchInfo(std::vector<DispatchInfo *>({&di1, &di2}));

    HardwareInterface<FamilyType>::dispatchWalker(*pCmdQ, multiDispatchInfo, CsrDependencies(), nullptr, nullptr, nullptr, nullptr, nullptr, pDevice->getPreemptionMode(), false);

    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    for (auto it = hwParser.lriList.begin(); it != hwParser.lriList.end(); it++) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        EXPECT_NE(0xE5F0u, cmd->getRegisterOffset());
    }
}

TEST(DispatchWalker, calculateDispatchDim) {
    Vec3<size_t> dim0{0, 0, 0};
    Vec3<size_t> dim1{2, 1, 1};
    Vec3<size_t> dim2{2, 2, 1};
    Vec3<size_t> dim3{2, 2, 2};
    Vec3<size_t> dispatches[] = {dim0, dim1, dim2, dim3};

    uint32_t testDims[] = {0, 1, 2, 3};
    for (const auto &lhs : testDims) {
        for (const auto &rhs : testDims) {
            uint32_t dimTest = calculateDispatchDim(dispatches[lhs], dispatches[rhs]);
            uint32_t dimRef = std::max(1U, std::max(lhs, rhs));
            EXPECT_EQ(dimRef, dimTest);
        }
    }
}

HWTEST_F(DispatchWalkerTest, WhenCallingDefaultWaMethodsThenExpectNothing) {
    auto &cmdStream = pCmdQ->getCS(0);
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    EXPECT_EQ(CL_SUCCESS, kernel.initialize());

    GpgpuWalkerHelper<GENX>::applyWADisableLSQCROPERFforOCL(&cmdStream, kernel, false);

    size_t expectedSize = 0;
    size_t actualSize = GpgpuWalkerHelper<GENX>::getSizeForWADisableLSQCROPERFforOCL(&kernel);
    EXPECT_EQ(expectedSize, actualSize);
}

HWTEST_F(DispatchWalkerTest, givenKernelWhenAuxTranslationRequiredThenPipeControlWithStallAndDCFlushAdded) {
    MockKernel kernel(program.get(), kernelInfo, *pDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &cmdStream = pCmdQ->getCS(0);
    void *buffer = cmdStream.getCpuBase();
    kernel.auxTranslationRequired = true;

    MockMultiDispatchInfo multiDispatchInfo(&kernel);
    DispatchInfo di1(&kernel, 1, Vec3<size_t>(1, 1, 1), Vec3<size_t>(1, 1, 1), Vec3<size_t>(0, 0, 0));
    di1.setPipeControlRequired(true);
    multiDispatchInfo.push(di1);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        pDevice->getPreemptionMode(),
        false);

    auto sizeUsed = cmdStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, buffer, sizeUsed));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, pipeControls.size());

    auto beginPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[0]));
    EXPECT_TRUE(beginPipeControl->getDcFlushEnable());
    EXPECT_TRUE(beginPipeControl->getCommandStreamerStallEnable());

    auto endPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[1]));
    EXPECT_FALSE(endPipeControl->getDcFlushEnable());
    EXPECT_TRUE(endPipeControl->getCommandStreamerStallEnable());
}

struct ProfilingCommandsTest : public DispatchWalkerTest, ::testing::WithParamInterface<bool> {
    void SetUp() override {
        DispatchWalkerTest::SetUp();
    }
    void TearDown() override {
        DispatchWalkerTest::TearDown();
    }
};

HWTEST_P(ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsTakenThenTimeStampAddressIsProgrammedCorrectly) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool checkForStart = GetParam();

    auto &cmdStream = pCmdQ->getCS(0);
    TagAllocator<HwTimeStamps> timeStampAllocator(this->pDevice->getMemoryManager(), 10, MemoryConstants::cacheLineSize);

    auto hwTimeStamp1 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp1);
    if (checkForStart) {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp1, &cmdStream);
    } else {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp1, &cmdStream);
    }

    auto hwTimeStamp2 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp2);
    if (checkForStart) {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp2, &cmdStream);
    } else {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp2, &cmdStream);
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    auto itorStoreReg = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorStoreReg);
    auto storeReg = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorStoreReg);
    ASSERT_NE(nullptr, storeReg);

    uint64_t gpuAddress = storeReg->getMemoryAddress();
    auto timestampFieldAddress = checkForStart ? &hwTimeStamp1->tagForCpuAccess->ContextStartTS : &hwTimeStamp1->tagForCpuAccess->ContextEndTS;
    uint64_t expectedAddress = hwTimeStamp1->getBaseGraphicsAllocation()->getGpuAddress() + ptrDiff(timestampFieldAddress, hwTimeStamp1->getBaseGraphicsAllocation()->getUnderlyingBuffer());
    EXPECT_EQ(expectedAddress, gpuAddress);

    itorStoreReg++;
    itorStoreReg = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(itorStoreReg, cmdList.end());
    ASSERT_NE(cmdList.end(), itorStoreReg);
    storeReg = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorStoreReg);
    ASSERT_NE(nullptr, storeReg);

    gpuAddress = storeReg->getMemoryAddress();
    timestampFieldAddress = checkForStart ? &hwTimeStamp2->tagForCpuAccess->ContextStartTS : &hwTimeStamp2->tagForCpuAccess->ContextEndTS;
    expectedAddress = hwTimeStamp2->getBaseGraphicsAllocation()->getGpuAddress() + ptrDiff(timestampFieldAddress, hwTimeStamp2->getBaseGraphicsAllocation()->getUnderlyingBuffer());
    EXPECT_EQ(expectedAddress, gpuAddress);

    if (checkForStart) {
        auto itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorPipeCtrl);
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
        ASSERT_NE(nullptr, pipeControl);

        gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);
        timestampFieldAddress = checkForStart ? &hwTimeStamp1->tagForCpuAccess->GlobalStartTS : &hwTimeStamp1->tagForCpuAccess->GlobalEndTS;
        expectedAddress = hwTimeStamp1->getBaseGraphicsAllocation()->getGpuAddress() + ptrDiff(timestampFieldAddress, hwTimeStamp1->getBaseGraphicsAllocation()->getUnderlyingBuffer());
        EXPECT_EQ(expectedAddress, gpuAddress);

        itorPipeCtrl++;
        itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(itorPipeCtrl, cmdList.end());
        ASSERT_NE(cmdList.end(), itorPipeCtrl);
        pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
        ASSERT_NE(nullptr, pipeControl);

        gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32;
        timestampFieldAddress = checkForStart ? &hwTimeStamp2->tagForCpuAccess->GlobalStartTS : &hwTimeStamp2->tagForCpuAccess->GlobalEndTS;
        expectedAddress = hwTimeStamp2->getBaseGraphicsAllocation()->getGpuAddress() + ptrDiff(timestampFieldAddress, hwTimeStamp2->getBaseGraphicsAllocation()->getUnderlyingBuffer());
        EXPECT_EQ(expectedAddress, gpuAddress);
    }
}

INSTANTIATE_TEST_CASE_P(StartEndFlag,
                        ProfilingCommandsTest, ::testing::Values(true, false));
