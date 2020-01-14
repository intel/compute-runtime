/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/built_ins/aux_translation_builtin.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/event/perf_counter.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/task_information.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/utilities/tag_allocator.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_program.h"

using namespace NEO;

struct DispatchWalkerTest : public CommandQueueFixture, public DeviceFixture, public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(0);
        DeviceFixture::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
        CommandQueueFixture::SetUp(context.get(), pClDevice, 0);

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
        context.reset();
        DeviceFixture::TearDown();
    }

    std::unique_ptr<KernelOperation> createBlockedCommandsData(CommandQueue &commandQueue) {
        auto commandStream = new LinearStream();

        auto &gpgpuCsr = commandQueue.getGpgpuCommandStreamReceiver();
        gpgpuCsr.ensureCommandBufferAllocation(*commandStream, 1, 1);

        return std::make_unique<KernelOperation>(commandStream, *gpgpuCsr.getInternalAllocationStorage());
    }

    std::unique_ptr<MockContext> context;
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

    DebugManagerStateRestore dbgRestore;
};

HWTEST_F(DispatchWalkerTest, WhenGettingComputeDimensionsThenCorrectNumberOfDimensionsIsReturned) {
    const size_t workItems1D[] = {100, 1, 1};
    EXPECT_EQ(1u, computeDimensions(workItems1D));

    const size_t workItems2D[] = {100, 100, 1};
    EXPECT_EQ(2u, computeDimensions(workItems2D));

    const size_t workItems3D[] = {100, 100, 100};
    EXPECT_EQ(3u, computeDimensions(workItems3D));
}

HWTEST_F(DispatchWalkerTest, givenSimd1WhenSetGpgpuWalkerThreadDataThenSimdInWalkerIsSetTo32Value) {
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation((void *)pCmdBuffer, sizeof(pCmdBuffer));
    LinearStream linearStream(&gfxAllocation);

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    WALKER_TYPE *computeWalker = static_cast<WALKER_TYPE *>(linearStream.getSpace(sizeof(WALKER_TYPE)));
    *computeWalker = FamilyType::cmdInitGpgpuWalker;

    size_t globalOffsets[] = {0, 0, 0};
    size_t startWorkGroups[] = {0, 0, 0};
    size_t numWorkGroups[] = {1, 1, 1};
    size_t localWorkSizesIn[] = {32, 1, 1};
    uint32_t simd = 1;
    iOpenCL::SPatchThreadPayload threadPayload;

    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(
        computeWalker, globalOffsets, startWorkGroups, numWorkGroups, localWorkSizesIn, simd, 3, true, false, threadPayload, 5u);
    EXPECT_EQ(computeWalker->getSimdSize(), 32 >> 4);
}

HWTEST_F(DispatchWalkerTest, WhenDispatchingWalkerThenCommandStreamMemoryIsntChanged) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

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
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_EQ(commandStreamBuffer, commandStream.getCpuBase());
    EXPECT_LT(commandStreamStart, commandStream.getUsed());
    EXPECT_EQ(sizeDispatchWalkerNeeds, commandStream.getUsed() - commandStreamStart);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalIdsWhenDispatchingWalkerThenWalkerIsDispatched) {
    threadPayload.LocalIDXPresent = 0;
    threadPayload.LocalIDYPresent = 0;
    threadPayload.LocalIDZPresent = 0;
    threadPayload.UnusedPerThreadConstantPresent = 1;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(&kernel);

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
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_EQ(commandStreamBuffer, commandStream.getCpuBase());
    EXPECT_LT(commandStreamStart, commandStream.getUsed());
    EXPECT_EQ(sizeDispatchWalkerNeeds, commandStream.getUsed() - commandStreamStart);
}

HWTEST_F(DispatchWalkerTest, GivenDefaultLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
            CL_COMMAND_NDRANGE_KERNEL);

        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, GivenSquaredLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
            CL_COMMAND_NDRANGE_KERNEL);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, GivenNdLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
            CL_COMMAND_NDRANGE_KERNEL);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, GivenOldLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
            CL_COMMAND_NDRANGE_KERNEL);
        EXPECT_EQ(dimension, *kernel.workDim);
    }
}

HWTEST_F(DispatchWalkerTest, GivenNumWorkGroupsWhenDispatchingWalkerThenNumWorkGroupsIsCorrectlySet) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_EQ(2u, *kernel.numWorkGroupsX);
    EXPECT_EQ(5u, *kernel.numWorkGroupsY);
    EXPECT_EQ(10u, *kernel.numWorkGroupsZ);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndDefaultAlgorithmWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndNdOnWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(10u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndSquaredAlgorithmWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndSquaredAlgorithmOffAndNdOffWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(2u, *kernel.localWorkSizeX);
    EXPECT_EQ(5u, *kernel.localWorkSizeY);
    EXPECT_EQ(1u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(1u, *kernel.localWorkSizeX);
    EXPECT_EQ(2u, *kernel.localWorkSizeY);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ);
}

HWTEST_F(DispatchWalkerTest, GivenTwoSetsOfLwsOffsetsWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);
    EXPECT_EQ(1u, *kernel.localWorkSizeX);
    EXPECT_EQ(2u, *kernel.localWorkSizeY);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ);
    EXPECT_EQ(1u, *kernel.localWorkSizeX2);
    EXPECT_EQ(2u, *kernel.localWorkSizeY2);
    EXPECT_EQ(3u, *kernel.localWorkSizeZ2);
}

HWTEST_F(DispatchWalkerTest, GivenSplitKernelWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    kernelInfo.workloadInfo.localWorkSizeOffsets[0] = 0;
    kernelInfo.workloadInfo.localWorkSizeOffsets[1] = 4;
    kernelInfo.workloadInfo.localWorkSizeOffsets[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());

    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWTEST_F(DispatchWalkerTest, GivenSplitWalkerWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    MockKernel mainKernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWTEST_F(DispatchWalkerTest, GivenBlockedQueueWhenDispatchingWalkerThenCommandSteamIsNotConsumed) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    size_t workGroupSize[3] = {2, 5, 10};
    cl_uint dimensions = 1;

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);

    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto &commandStream = pCmdQ->getCS(1024);
    EXPECT_EQ(0u, commandStream.getUsed());
    EXPECT_NE(nullptr, blockedCommandsData);
    EXPECT_NE(nullptr, blockedCommandsData->commandStream);
    EXPECT_NE(nullptr, blockedCommandsData->dsh);
    EXPECT_NE(nullptr, blockedCommandsData->ioh);
    EXPECT_NE(nullptr, blockedCommandsData->ssh);
}

HWTEST_F(DispatchWalkerTest, GivenBlockedQueueWhenDispatchingWalkerThenRequiredHeaSizesAreTakenFromKernel) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    size_t workGroupSize[3] = {2, 5, 10};
    cl_uint dimensions = 1;

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
    DispatchInfo dispatchInfo(const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    MultiDispatchInfo multiDispatchInfo(&kernel);
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    Vec3<size_t> localWorkgroupSize(workGroupSize);

    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(kernel);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(kernel, Math::computeTotalElementsCount(localWorkgroupSize));
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(kernel);

    EXPECT_LE(expectedSizeDSH, blockedCommandsData->dsh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeIOH, blockedCommandsData->ioh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeSSH, blockedCommandsData->ssh->getMaxAvailableSpace());
}

HWTEST_F(DispatchWalkerTest, givenBlockedEnqueueWhenObtainingCommandStreamThenAllocateEnoughSpaceAndBlockedKernelData) {
    DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);

    std::unique_ptr<KernelOperation> blockedKernelData;
    MockCommandQueueHw<FamilyType> mockCmdQ(nullptr, pClDevice, nullptr);

    auto expectedSizeCSAllocation = MemoryConstants::pageSize64k;
    auto expectedSizeCS = MemoryConstants::pageSize64k - CSRequirements::csOverfetchSize;

    CsrDependencies csrDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    auto cmdStream = mockCmdQ.template obtainCommandStream<CL_COMMAND_NDRANGE_KERNEL>(csrDependencies, false, true,
                                                                                      multiDispatchInfo, eventsRequest, blockedKernelData,
                                                                                      nullptr, 0u);

    EXPECT_EQ(expectedSizeCS, cmdStream->getMaxAvailableSpace());
    EXPECT_EQ(expectedSizeCSAllocation, cmdStream->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, blockedKernelData);
    EXPECT_EQ(cmdStream, blockedKernelData->commandStream.get());
}

HWTEST_F(DispatchWalkerTest, GivenBlockedQueueWhenDispatchingWalkerThenRequiredHeapSizesAreTakenFromMdi) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    MockMultiDispatchInfo multiDispatchInfo(&kernel);

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto expectedSizeDSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredDSH(multiDispatchInfo);
    auto expectedSizeIOH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredIOH(multiDispatchInfo);
    auto expectedSizeSSH = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);

    EXPECT_LE(expectedSizeDSH, blockedCommandsData->dsh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeIOH, blockedCommandsData->ioh->getMaxAvailableSpace());
    EXPECT_LE(expectedSizeSSH, blockedCommandsData->ssh->getMaxAvailableSpace());
}

HWTEST_F(DispatchWalkerTest, givenBlockedQueueWhenDispatchWalkerIsCalledThenCommandStreamHasGpuAddress) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    MockMultiDispatchInfo multiDispatchInfo(&kernel);

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_NE(nullptr, blockedCommandsData->commandStream->getGraphicsAllocation());
    EXPECT_NE(0ull, blockedCommandsData->commandStream->getGraphicsAllocation()->getGpuAddress());
}

HWTEST_F(DispatchWalkerTest, givenThereAreAllocationsForReuseWhenDispatchWalkerIsCalledThenCommandStreamObtainsReusableAllocation) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    MockMultiDispatchInfo multiDispatchInfo(&kernel);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto allocation = csr.getMemoryManager()->allocateGraphicsMemoryWithProperties({csr.getRootDeviceIndex(), MemoryConstants::pageSize64k + CSRequirements::csOverfetchSize,
                                                                                    GraphicsAllocation::AllocationType::COMMAND_BUFFER});
    csr.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>{allocation}, REUSABLE_ALLOCATION);
    ASSERT_FALSE(csr.getInternalAllocationStorage()->getAllocationsForReuse().peekIsEmpty());

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_TRUE(csr.getInternalAllocationStorage()->getAllocationsForReuse().peekIsEmpty());
    EXPECT_EQ(allocation, blockedCommandsData->commandStream->getGraphicsAllocation());
}

HWTEST_F(DispatchWalkerTest, GivenMultipleKernelsWhenDispatchingWalkerThenWorkDimensionsAreCorrect) {
    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

    for (auto &dispatchInfo : multiDispatchInfo) {
        auto &kernel = *dispatchInfo.getKernel();
        EXPECT_EQ(*kernel.workDim, dispatchInfo.getDim());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, GivenMultipleKernelsWhenDispatchingWalkerThenInterfaceDescriptorsAreProgrammedCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    auto memoryManager = this->pDevice->getMemoryManager();
    auto kernelIsaAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    auto kernelIsaWithSamplerAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA});
    kernelInfo.kernelAllocation = kernelIsaAllocation;
    kernelInfoWithSampler.kernelAllocation = kernelIsaWithSamplerAllocation;
    auto gpuAddress1 = kernelIsaAllocation->getGpuAddressToPatch();
    auto gpuAddress2 = kernelIsaWithSamplerAllocation->getGpuAddressToPatch();

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1, &kernel2}));

    // create Indirect DSH heap
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);

    indirectHeap.align(HardwareCommandsHelper<FamilyType>::alignInterfaceDescriptorData);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, GivenMultipleKernelsWhenDispatchingWalkerThenGpgpuWalkerIdOffsetIsProgrammedCorrectly) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, GivenMultipleKernelsWhenDispatchingWalkerThenThreadGroupIdStartingCoordinatesAreProgrammedCorrectly) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, GivenMultipleDispatchInfoAndSameKernelWhenDispatchingWalkerThenGpgpuWalkerThreadGroupIdStartingCoordinatesAreCorrectlyProgrammed) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
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
        CL_COMMAND_NDRANGE_KERNEL);

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

HWTEST_F(DispatchWalkerTest, GivenCacheFlushAfterWalkerDisabledWhenAllocationRequiresCacheFlushThenFlushCommandNotPresentAfterWalker) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    kernel1.kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    kernel1.kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({&kernel1}));
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
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdStream);
    PIPE_CONTROL *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    EXPECT_EQ(nullptr, pipeControl);
}

HWTEST_F(DispatchWalkerTest, GivenCacheFlushAfterWalkerEnabledWhenWalkerWithTwoKernelsThenFlushCommandPresentOnce) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    kernel1.kernelArgRequiresCacheFlush.resize(1);
    kernel2.kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    kernel1.kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;
    kernel2.kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

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
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdStream);
    uint32_t pipeControlCount = hwParse.getCommandCount<PIPE_CONTROL>();
    EXPECT_EQ(pipeControlCount, 1u);
}

HWTEST_F(DispatchWalkerTest, GivenCacheFlushAfterWalkerEnabledWhenTwoWalkersForQueueThenFlushCommandPresentTwice) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    kernel1.kernelArgRequiresCacheFlush.resize(1);
    kernel2.kernelArgRequiresCacheFlush.resize(1);
    MockGraphicsAllocation cacheRequiringAllocation;
    kernel1.kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;
    kernel2.kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    MockMultiDispatchInfo multiDispatchInfo1(std::vector<Kernel *>({&kernel1}));
    MockMultiDispatchInfo multiDispatchInfo2(std::vector<Kernel *>({&kernel2}));
    // create commandStream
    auto &cmdStream = pCmdQ->getCS(0);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo1,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo2,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(cmdStream);
    uint32_t pipeControlCount = hwParse.getCommandCount<PIPE_CONTROL>();
    EXPECT_EQ(pipeControlCount, 2u);
}

TEST(DispatchWalker, WhenCalculatingDispatchDimensionsThenCorrectValuesAreReturned) {
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

HWTEST_F(DispatchWalkerTest, givenKernelWhenAuxToNonAuxWhenTranslationRequiredThenPipeControlWithStallAndDCFlushAdded) {
    auto &context = pCmdQ->getContext();
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto builtIns = executionEnvironment->getBuiltIns();
    BuiltinDispatchInfoBuilder &baseBuilder = builtIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, context, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &cmdStream = pCmdQ->getCS(0);
    void *buffer = cmdStream.getCpuBase();
    kernel.auxTranslationRequired = true;
    MockBuffer mockBuffer[2];

    MultiDispatchInfo multiDispatchInfo;
    MemObjsForAuxTranslation memObjsForAuxTranslation;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    memObjsForAuxTranslation.insert(&mockBuffer[0]);
    memObjsForAuxTranslation.insert(&mockBuffer[1]);

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::AuxToNonAux;

    builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto sizeUsed = cmdStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, buffer, sizeUsed));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, pipeControls.size());

    auto beginPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[0]));
    EXPECT_TRUE(beginPipeControl->getDcFlushEnable());
    EXPECT_TRUE(beginPipeControl->getCommandStreamerStallEnable());

    auto endPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[1]));
    bool dcFlushRequired = (executionEnvironment->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN8_CORE);
    EXPECT_EQ(dcFlushRequired, endPipeControl->getDcFlushEnable());
    EXPECT_TRUE(endPipeControl->getCommandStreamerStallEnable());
}

HWTEST_F(DispatchWalkerTest, givenKernelWhenNonAuxToAuxWhenTranslationRequiredThenPipeControlWithStallAdded) {
    auto &context = pCmdQ->getContext();
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto builtIns = executionEnvironment->getBuiltIns();
    BuiltinDispatchInfoBuilder &baseBuilder = builtIns->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, context, *pDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.workloadInfo.workDimOffset = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &cmdStream = pCmdQ->getCS(0);
    void *buffer = cmdStream.getCpuBase();
    kernel.auxTranslationRequired = true;
    MockBuffer mockBuffer[2];

    MultiDispatchInfo multiDispatchInfo;
    MemObjsForAuxTranslation memObjsForAuxTranslation;
    multiDispatchInfo.setMemObjsForAuxTranslation(memObjsForAuxTranslation);
    memObjsForAuxTranslation.insert(&mockBuffer[0]);
    memObjsForAuxTranslation.insert(&mockBuffer[1]);

    BuiltinOpParams builtinOpsParams;
    builtinOpsParams.auxTranslationDirection = AuxTranslationDirection::NonAuxToAux;

    builder.buildDispatchInfosForAuxTranslation<FamilyType>(multiDispatchInfo, builtinOpsParams);

    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto sizeUsed = cmdStream.getUsed();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, buffer, sizeUsed));

    auto pipeControls = findAll<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, pipeControls.size());

    bool dcFlushRequired = (executionEnvironment->getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN8_CORE);

    auto beginPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[0]));
    EXPECT_TRUE(beginPipeControl->getDcFlushEnable());
    EXPECT_TRUE(beginPipeControl->getCommandStreamerStallEnable());

    auto endPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[1]));
    EXPECT_EQ(dcFlushRequired, endPipeControl->getDcFlushEnable());
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
    TagAllocator<HwTimeStamps> timeStampAllocator(pDevice->getRootDeviceIndex(), this->pDevice->getMemoryManager(), 10,
                                                  MemoryConstants::cacheLineSize, sizeof(HwTimeStamps), false);

    auto hwTimeStamp1 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp1);
    if (checkForStart) {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp1, &cmdStream, pDevice->getHardwareInfo());
    } else {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp1, &cmdStream);
    }

    auto hwTimeStamp2 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp2);
    if (checkForStart) {
        GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp2, &cmdStream, pDevice->getHardwareInfo());
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
    auto contextTimestampFieldOffset = checkForStart ? offsetof(HwTimeStamps, ContextStartTS) : offsetof(HwTimeStamps, ContextEndTS);
    uint64_t expectedAddress = hwTimeStamp1->getGpuAddress() + contextTimestampFieldOffset;
    EXPECT_EQ(expectedAddress, gpuAddress);

    itorStoreReg++;
    itorStoreReg = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(itorStoreReg, cmdList.end());
    ASSERT_NE(cmdList.end(), itorStoreReg);
    storeReg = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorStoreReg);
    ASSERT_NE(nullptr, storeReg);

    gpuAddress = storeReg->getMemoryAddress();
    expectedAddress = hwTimeStamp2->getGpuAddress() + contextTimestampFieldOffset;
    EXPECT_EQ(expectedAddress, gpuAddress);

    if (checkForStart) {
        auto itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itorPipeCtrl);
        if (HardwareCommandsHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
            itorPipeCtrl++;
        }
        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
        ASSERT_NE(nullptr, pipeControl);

        gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);
        expectedAddress = hwTimeStamp1->getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
        EXPECT_EQ(expectedAddress, gpuAddress);

        itorPipeCtrl++;
        itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(itorPipeCtrl, cmdList.end());
        if (HardwareCommandsHelper<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
            itorPipeCtrl++;
        }
        ASSERT_NE(cmdList.end(), itorPipeCtrl);
        pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
        ASSERT_NE(nullptr, pipeControl);

        gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32;
        expectedAddress = hwTimeStamp2->getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
        EXPECT_EQ(expectedAddress, gpuAddress);
    }
}

INSTANTIATE_TEST_CASE_P(StartEndFlag,
                        ProfilingCommandsTest, ::testing::Bool());
