/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_work_size.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/utilities/perf_counter.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/built_ins/aux_translation_builtin.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/mocks/mock_timestamp_container.h"
#include "test.h"

using namespace NEO;

struct DispatchWalkerTest : public CommandQueueFixture, public ClDeviceFixture, public ::testing::Test {

    using CommandQueueFixture::SetUp;

    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(0);
        ClDeviceFixture::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
        CommandQueueFixture::SetUp(context.get(), pClDevice, 0);

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
        kernelInfo.setCrossThreadDataSize(64);
        kernelInfo.setLocalIds({1, 1, 1});
        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.KernelHeapSize = sizeof(kernelIsa);

        kernelInfoWithSampler.kernelDescriptor.kernelAttributes.simdSize = 32;
        kernelInfoWithSampler.setCrossThreadDataSize(64);
        kernelInfoWithSampler.setLocalIds({1, 1, 1});
        kernelInfoWithSampler.setSamplerTable(0, 1, 4);
        kernelInfoWithSampler.heapInfo.pKernelHeap = kernelIsa;
        kernelInfoWithSampler.heapInfo.KernelHeapSize = sizeof(kernelIsa);
        kernelInfoWithSampler.heapInfo.pDsh = static_cast<const void *>(dsh);
        kernelInfoWithSampler.heapInfo.DynamicStateHeapSize = sizeof(dsh);
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        context.reset();
        ClDeviceFixture::TearDown();
    }

    std::unique_ptr<KernelOperation> createBlockedCommandsData(CommandQueue &commandQueue) {
        auto commandStream = new LinearStream();

        auto &gpgpuCsr = commandQueue.getGpgpuCommandStreamReceiver();
        gpgpuCsr.ensureCommandBufferAllocation(*commandStream, 1, 1);

        return std::make_unique<KernelOperation>(commandStream, *gpgpuCsr.getInternalAllocationStorage());
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockProgram> program;

    MockKernelInfo kernelInfo;
    MockKernelInfo kernelInfoWithSampler;

    uint32_t kernelIsa[32];
    uint32_t dsh[32];

    DebugManagerStateRestore dbgRestore;
};

struct DispatchWalkerTestForAuxTranslation : DispatchWalkerTest, public ::testing::WithParamInterface<KernelObjForAuxTranslation::Type> {
    void SetUp() override {
        DispatchWalkerTest::SetUp();
        kernelObjType = GetParam();
    }
    KernelObjForAuxTranslation::Type kernelObjType;
};

INSTANTIATE_TEST_CASE_P(,
                        DispatchWalkerTestForAuxTranslation,
                        testing::ValuesIn({KernelObjForAuxTranslation::Type::MEM_OBJ, KernelObjForAuxTranslation::Type::GFX_ALLOC}));

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
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream linearStream(&gfxAllocation);

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    WALKER_TYPE *computeWalker = static_cast<WALKER_TYPE *>(linearStream.getSpace(sizeof(WALKER_TYPE)));
    *computeWalker = FamilyType::cmdInitGpgpuWalker;

    size_t globalOffsets[] = {0, 0, 0};
    size_t startWorkGroups[] = {0, 0, 0};
    size_t numWorkGroups[] = {1, 1, 1};
    size_t localWorkSizesIn[] = {32, 1, 1};
    uint32_t simd = 1;

    KernelDescriptor kd;
    GpgpuWalkerHelper<FamilyType>::setGpgpuWalkerThreadData(
        computeWalker, kd, globalOffsets, startWorkGroups, numWorkGroups, localWorkSizesIn, simd, 3, true, false, 5u);
    EXPECT_EQ(computeWalker->getSimdSize(), 32 >> 4);
}

HWTEST_F(DispatchWalkerTest, WhenDispatchingWalkerThenCommandStreamMemoryIsntChanged) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   HardwareCommandsHelper<FamilyType>::getSizeRequiredCS();

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
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
    kernelInfo.setLocalIds({0, 0, 0});
    kernelInfo.kernelDescriptor.kernelAttributes.flags.perThreadDataUnusedGrfIsPresent = true;

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &commandStream = pCmdQ->getCS(4096);

    // Consume all memory except what is needed for this enqueue
    auto sizeDispatchWalkerNeeds = sizeof(typename FamilyType::WALKER_TYPE) +
                                   HardwareCommandsHelper<FamilyType>::getSizeRequiredCS();

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
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;

        DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

        EXPECT_EQ(dimension, *kernel.getWorkDim());
    }
}

HWTEST_F(DispatchWalkerTest, GivenSquaredLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
        EXPECT_EQ(dimension, *kernel.getWorkDim());
    }
}

HWTEST_F(DispatchWalkerTest, GivenNdLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
        EXPECT_EQ(dimension, *kernel.getWorkDim());
    }
}

HWTEST_F(DispatchWalkerTest, GivenOldLwsAlgorithmWhenDispatchingWalkerThenDimensionsAreCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    for (uint32_t dimension = 1; dimension <= 3; ++dimension) {
        workItems[dimension - 1] = 256;
        DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimension, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
        EXPECT_EQ(dimension, *kernel.getWorkDim());
    }
}

HWTEST_F(DispatchWalkerTest, GivenNumWorkGroupsWhenDispatchingWalkerThenNumWorkGroupsIsCorrectlySet) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 1, 1};
    cl_uint dimensions = 3;

    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups(workItems);
    dispatchInfo.setTotalNumberOfWorkgroups(workItems);
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

    auto numWorkGroups = kernel.getNumWorkGroupsValues();
    EXPECT_EQ(2u, *numWorkGroups[0]);
    EXPECT_EQ(5u, *numWorkGroups[1]);
    EXPECT_EQ(10u, *numWorkGroups[2]);
}

HWTEST_F(DispatchWalkerTest, GivenGlobalWorkOffsetWhenDispatchingWalkerThenGlobalWorkOffsetIsCorrectlySet) {
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[0] = 0u;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[1] = 4u;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[2] = 8u;
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {1, 2, 3};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 1, 1};
    cl_uint dimensions = 3;

    DispatchInfo dispatchInfo(pClDevice, &kernel, dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto gwo = kernel.getGlobalWorkOffsetValues();
    EXPECT_EQ(1u, *gwo[0]);
    EXPECT_EQ(2u, *gwo[1]);
    EXPECT_EQ(3u, *gwo[2]);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndDefaultAlgorithmWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(2u, *localWorkSize[0]);
    EXPECT_EQ(5u, *localWorkSize[1]);
    EXPECT_EQ(1u, *localWorkSize[2]);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndNdOnWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 3, 5};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(2u, *localWorkSize[0]);
    EXPECT_EQ(3u, *localWorkSize[1]);
    EXPECT_EQ(5u, *localWorkSize[2]);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndSquaredAlgorithmWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(2u, *localWorkSize[0]);
    EXPECT_EQ(5u, *localWorkSize[1]);
    EXPECT_EQ(1u, *localWorkSize[2]);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeAndSquaredAlgorithmOffAndNdOffWhenDispatchingWalkerThenLwsIsCorrect) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(2u, *localWorkSize[0]);
    EXPECT_EQ(5u, *localWorkSize[1]);
    EXPECT_EQ(1u, *localWorkSize[2]);
}

HWTEST_F(DispatchWalkerTest, GivenNoLocalWorkSizeWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 2, 3};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(1u, *localWorkSize[0]);
    EXPECT_EQ(2u, *localWorkSize[1]);
    EXPECT_EQ(3u, *localWorkSize[2]);
}

HWTEST_F(DispatchWalkerTest, GivenTwoSetsOfLwsOffsetsWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[0] = 12;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[1] = 16;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2[2] = 20;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {2, 5, 10};
    size_t workGroupSize[3] = {1, 2, 3};
    cl_uint dimensions = 3;
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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

    auto localWorkSize = kernel.getLocalWorkSizeValues();
    EXPECT_EQ(1u, *localWorkSize[0]);
    EXPECT_EQ(2u, *localWorkSize[1]);
    EXPECT_EQ(3u, *localWorkSize[2]);
    auto localWorkSize2 = kernel.getLocalWorkSize2Values();
    EXPECT_EQ(1u, *localWorkSize2[0]);
    EXPECT_EQ(2u, *localWorkSize2[1]);
    EXPECT_EQ(3u, *localWorkSize2[2]);
}

HWTEST_F(DispatchWalkerTest, GivenSplitKernelWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 0;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 4;
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 8;
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());

    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
    kernelInfoWithSampler.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0] = 12;
    kernelInfoWithSampler.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1] = 16;
    kernelInfoWithSampler.kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2] = 20;
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    DispatchInfo di1(pClDevice, &kernel1, 3, {10, 10, 10}, {1, 2, 3}, {0, 0, 0});
    di1.setNumberOfWorkgroups({1, 1, 1});
    di1.setTotalNumberOfWorkgroups({2, 2, 2});
    DispatchInfo di2(pClDevice, &kernel2, 3, {10, 10, 10}, {4, 5, 6}, {0, 0, 0});
    di2.setNumberOfWorkgroups({1, 1, 1});
    di2.setTotalNumberOfWorkgroups({2, 2, 2});

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
        auto &kernel = static_cast<MockKernel &>(*dispatchInfo.getKernel());
        auto localWorkSize = kernel.getLocalWorkSizeValues();
        if (dispatchId == 0) {
            EXPECT_EQ(1u, *localWorkSize[0]);
            EXPECT_EQ(2u, *localWorkSize[1]);
            EXPECT_EQ(3u, *localWorkSize[2]);
        }
        if (dispatchId == 1) {
            EXPECT_EQ(4u, *localWorkSize[0]);
            EXPECT_EQ(5u, *localWorkSize[1]);
            EXPECT_EQ(6u, *localWorkSize[2]);
        }
        dispatchId++;
    }
}

HWTEST_F(DispatchWalkerTest, GivenSplitWalkerWhenDispatchingWalkerThenLwsIsCorrect) {
    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    MockKernel mainKernel(program.get(), kernelInfo, *pClDevice);
    auto &dispatchTraits = kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits;
    dispatchTraits.localWorkSize[0] = 0;
    dispatchTraits.localWorkSize[1] = 4;
    dispatchTraits.localWorkSize[2] = 8;
    dispatchTraits.localWorkSize2[0] = 12;
    dispatchTraits.localWorkSize2[1] = 16;
    dispatchTraits.localWorkSize2[2] = 20;
    dispatchTraits.numWorkGroups[0] = 24;
    dispatchTraits.numWorkGroups[1] = 28;
    dispatchTraits.numWorkGroups[2] = 32;
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    ASSERT_EQ(CL_SUCCESS, mainKernel.initialize());

    DispatchInfo di1(pClDevice, &kernel1, 3, {10, 10, 10}, {1, 2, 3}, {0, 0, 0});
    di1.setNumberOfWorkgroups({1, 1, 1});
    di1.setTotalNumberOfWorkgroups({3, 2, 2});
    DispatchInfo di2(pClDevice, &mainKernel, 3, {10, 10, 10}, {4, 5, 6}, {0, 0, 0});
    di2.setNumberOfWorkgroups({1, 1, 1});
    di2.setTotalNumberOfWorkgroups({3, 2, 2});

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
        auto &kernel = static_cast<MockKernel &>(*dispatchInfo.getKernel());
        auto localWorkSize = kernel.getLocalWorkSizeValues();
        auto localWorkSize2 = kernel.getLocalWorkSize2Values();
        auto numWorkGroups = kernel.getNumWorkGroupsValues();
        if (&kernel == &mainKernel) {
            EXPECT_EQ(4u, *localWorkSize[0]);
            EXPECT_EQ(5u, *localWorkSize[1]);
            EXPECT_EQ(6u, *localWorkSize[2]);
            EXPECT_EQ(4u, *localWorkSize2[0]);
            EXPECT_EQ(5u, *localWorkSize2[1]);
            EXPECT_EQ(6u, *localWorkSize2[2]);
            EXPECT_EQ(3u, *numWorkGroups[0]);
            EXPECT_EQ(2u, *numWorkGroups[1]);
            EXPECT_EQ(2u, *numWorkGroups[2]);
        } else {
            EXPECT_EQ(0u, *localWorkSize[0]);
            EXPECT_EQ(0u, *localWorkSize[1]);
            EXPECT_EQ(0u, *localWorkSize[2]);
            EXPECT_EQ(1u, *localWorkSize2[0]);
            EXPECT_EQ(2u, *localWorkSize2[1]);
            EXPECT_EQ(3u, *localWorkSize2[2]);
            EXPECT_EQ(0u, *numWorkGroups[0]);
            EXPECT_EQ(0u, *numWorkGroups[1]);
            EXPECT_EQ(0u, *numWorkGroups[2]);
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

    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
    DispatchInfo dispatchInfo(pClDevice, const_cast<MockKernel *>(&kernel), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
                                                                                      nullptr, 0u, false);

    EXPECT_EQ(expectedSizeCS, cmdStream->getMaxAvailableSpace());
    EXPECT_EQ(expectedSizeCSAllocation, cmdStream->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, blockedKernelData);
    EXPECT_EQ(cmdStream, blockedKernelData->commandStream.get());
}

HWTEST_F(DispatchWalkerTest, GivenBlockedQueueWhenDispatchingWalkerThenRequiredHeapSizesAreTakenFromMdi) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, &kernel);

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
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, &kernel);

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
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, &kernel);

    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    auto allocation = csr.getMemoryManager()->allocateGraphicsMemoryWithProperties({csr.getRootDeviceIndex(), MemoryConstants::pageSize64k + CSRequirements::csOverfetchSize,
                                                                                    GraphicsAllocation::AllocationType::COMMAND_BUFFER, csr.getOsContext().getDeviceBitfield()});
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
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1, &kernel2}));

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
        auto &kernel = static_cast<MockKernel &>(*dispatchInfo.getKernel());
        EXPECT_EQ(dispatchInfo.getDim(), *kernel.getWorkDim());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, DispatchWalkerTest, GivenMultipleKernelsWhenDispatchingWalkerThenInterfaceDescriptorsAreProgrammedCorrectly) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    auto memoryManager = this->pDevice->getMemoryManager();
    auto kernelIsaAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA, pDevice->getDeviceBitfield()});
    auto kernelIsaWithSamplerAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, GraphicsAllocation::AllocationType::KERNEL_ISA, pDevice->getDeviceBitfield()});
    kernelInfo.kernelAllocation = kernelIsaAllocation;
    kernelInfoWithSampler.kernelAllocation = kernelIsaWithSamplerAllocation;
    auto gpuAddress1 = kernelIsaAllocation->getGpuAddressToPatch();
    auto gpuAddress2 = kernelIsaWithSamplerAllocation->getGpuAddressToPatch();

    MockKernel kernel1(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel1.initialize());
    MockKernel kernel2(program.get(), kernelInfoWithSampler, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1, &kernel2}));

    // create Indirect DSH heap
    auto &indirectHeap = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);

    indirectHeap.align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
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
            if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
                EXPECT_EQ(1u, samplerCount);
            } else {
                EXPECT_EQ(0u, samplerCount);
            }
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

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1, &kernel2}));

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

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1, &kernel2}));

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

    DispatchInfo di1(pClDevice, &kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0}, {100, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 1, 1}, {0, 0, 0});
    DispatchInfo di2(pClDevice, &kernel, 1, {100, 1, 1}, {10, 1, 1}, {0, 0, 0}, {100, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 1, 1}, {10, 0, 0});

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

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1}));
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

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, std::vector<Kernel *>({&kernel1, &kernel2}));
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

    MockMultiDispatchInfo multiDispatchInfo1(pClDevice, std::vector<Kernel *>({&kernel1}));
    MockMultiDispatchInfo multiDispatchInfo2(pClDevice, std::vector<Kernel *>({&kernel2}));
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

HWTEST_P(DispatchWalkerTestForAuxTranslation, givenKernelWhenAuxToNonAuxWhenTranslationRequiredThenPipeControlWithStallAndDCFlushAdded) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &cmdStream = pCmdQ->getCS(0);
    void *buffer = cmdStream.getCpuBase();
    kernel.auxTranslationRequired = true;
    MockKernelObjForAuxTranslation mockKernelObj1(kernelObjType);
    MockKernelObjForAuxTranslation mockKernelObj2(kernelObjType);

    MultiDispatchInfo multiDispatchInfo;
    KernelObjsForAuxTranslation kernelObjsForAuxTranslation;
    multiDispatchInfo.setKernelObjsForAuxTranslation(kernelObjsForAuxTranslation);
    kernelObjsForAuxTranslation.insert(mockKernelObj1);
    kernelObjsForAuxTranslation.insert(mockKernelObj2);

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
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), beginPipeControl->getDcFlushEnable());
    EXPECT_TRUE(beginPipeControl->getCommandStreamerStallEnable());

    auto endPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[1]));
    bool dcFlushRequired = (pClDevice->getHardwareInfo().platform.eRenderCoreFamily == IGFX_GEN8_CORE);
    EXPECT_EQ(dcFlushRequired, endPipeControl->getDcFlushEnable());
    EXPECT_TRUE(endPipeControl->getCommandStreamerStallEnable());
}

HWTEST_P(DispatchWalkerTestForAuxTranslation, givenKernelWhenNonAuxToAuxWhenTranslationRequiredThenPipeControlWithStallAdded) {
    BuiltinDispatchInfoBuilder &baseBuilder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, *pClDevice);
    auto &builder = static_cast<BuiltInOp<EBuiltInOps::AuxTranslation> &>(baseBuilder);

    MockKernel kernel(program.get(), kernelInfo, *pClDevice);
    kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.workDim = 0;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    auto &cmdStream = pCmdQ->getCS(0);
    void *buffer = cmdStream.getCpuBase();
    kernel.auxTranslationRequired = true;
    MockKernelObjForAuxTranslation mockKernelObj1(kernelObjType);
    MockKernelObjForAuxTranslation mockKernelObj2(kernelObjType);

    MultiDispatchInfo multiDispatchInfo;
    KernelObjsForAuxTranslation kernelObjsForAuxTranslation;
    multiDispatchInfo.setKernelObjsForAuxTranslation(kernelObjsForAuxTranslation);
    kernelObjsForAuxTranslation.insert(mockKernelObj1);
    kernelObjsForAuxTranslation.insert(mockKernelObj2);

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

    bool dcFlushRequired = (pClDevice->getHardwareInfo().platform.eRenderCoreFamily == IGFX_GEN8_CORE);

    auto beginPipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*(pipeControls[0]));
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), beginPipeControl->getDcFlushEnable());
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

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsTakenThenTimeStampAddressIsProgrammedCorrectly) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &cmdStream = pCmdQ->getCS(0);
    MockTagAllocator<HwTimeStamps> timeStampAllocator(pDevice->getRootDeviceIndex(), this->pDevice->getMemoryManager(), 10,
                                                      MemoryConstants::cacheLineSize, sizeof(HwTimeStamps), false, pDevice->getDeviceBitfield());

    auto hwTimeStamp1 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp1);

    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp1, &cmdStream, pDevice->getHardwareInfo());

    auto hwTimeStamp2 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp2);

    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsStart(*hwTimeStamp2, &cmdStream, pDevice->getHardwareInfo());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    auto itorStoreReg = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorStoreReg);
    auto storeReg = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorStoreReg);
    ASSERT_NE(nullptr, storeReg);

    uint64_t gpuAddress = storeReg->getMemoryAddress();
    auto contextTimestampFieldOffset = offsetof(HwTimeStamps, ContextStartTS);
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

    auto itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPipeCtrl);
    if (MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        itorPipeCtrl++;
    }
    if (UnitTestHelper<FamilyType>::isAdditionalSynchronizationRequired()) {
        itorPipeCtrl++;
    }
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
    ASSERT_NE(nullptr, pipeControl);

    gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | (static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32);
    expectedAddress = hwTimeStamp1->getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
    EXPECT_EQ(expectedAddress, gpuAddress);

    itorPipeCtrl++;
    itorPipeCtrl = find<typename FamilyType::PIPE_CONTROL *>(itorPipeCtrl, cmdList.end());
    if (MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(pDevice->getHardwareInfo())) {
        itorPipeCtrl++;
    }
    if (UnitTestHelper<FamilyType>::isAdditionalSynchronizationRequired()) {
        itorPipeCtrl++;
    }
    ASSERT_NE(cmdList.end(), itorPipeCtrl);
    pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
    ASSERT_NE(nullptr, pipeControl);

    gpuAddress = static_cast<uint64_t>(pipeControl->getAddress()) | static_cast<uint64_t>(pipeControl->getAddressHigh()) << 32;
    expectedAddress = hwTimeStamp2->getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
    EXPECT_EQ(expectedAddress, gpuAddress);
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProfilingCommandsTest, givenKernelWhenProfilingCommandStartIsNotTakenThenTimeStampAddressIsProgrammedCorrectly) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto &cmdStream = pCmdQ->getCS(0);
    MockTagAllocator<HwTimeStamps> timeStampAllocator(pDevice->getRootDeviceIndex(), this->pDevice->getMemoryManager(), 10,
                                                      MemoryConstants::cacheLineSize, sizeof(HwTimeStamps), false, pDevice->getDeviceBitfield());

    auto hwTimeStamp1 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp1);
    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp1, &cmdStream, pDevice->getHardwareInfo());

    auto hwTimeStamp2 = timeStampAllocator.getTag();
    ASSERT_NE(nullptr, hwTimeStamp2);
    GpgpuWalkerHelper<FamilyType>::dispatchProfilingCommandsEnd(*hwTimeStamp2, &cmdStream, pDevice->getHardwareInfo());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    auto itorStoreReg = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorStoreReg);
    auto storeReg = genCmdCast<MI_STORE_REGISTER_MEM *>(*itorStoreReg);
    ASSERT_NE(nullptr, storeReg);

    uint64_t gpuAddress = storeReg->getMemoryAddress();
    auto contextTimestampFieldOffset = offsetof(HwTimeStamps, ContextEndTS);
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
}

HWTEST_F(DispatchWalkerTest, WhenKernelRequiresImplicitArgsThenIohRequiresMoreSpace) {
    size_t globalOffsets[3] = {0, 0, 0};
    size_t workItems[3] = {1, 1, 1};
    size_t workGroupSize[3] = {2, 5, 10};
    cl_uint dimensions = 1;
    Vec3<size_t> localWorkgroupSize(workGroupSize);
    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);

    kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    MockKernel kernelWithoutImplicitArgs(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernelWithoutImplicitArgs.initialize());

    kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    MockKernel kernelWithImplicitArgs(program.get(), kernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernelWithImplicitArgs.initialize());

    DispatchInfo dispatchInfoWithoutImplicitArgs(pClDevice, const_cast<MockKernel *>(&kernelWithoutImplicitArgs), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfoWithoutImplicitArgs.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfoWithoutImplicitArgs.setTotalNumberOfWorkgroups({1, 1, 1});
    MultiDispatchInfo multiDispatchInfoWithoutImplicitArgs(&kernelWithoutImplicitArgs);
    multiDispatchInfoWithoutImplicitArgs.push(dispatchInfoWithoutImplicitArgs);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfoWithoutImplicitArgs,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto iohSizeWithoutImplicitArgs = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(kernelWithoutImplicitArgs, Math::computeTotalElementsCount(localWorkgroupSize));

    DispatchInfo dispatchInfoWithImplicitArgs(pClDevice, const_cast<MockKernel *>(&kernelWithImplicitArgs), dimensions, workItems, workGroupSize, globalOffsets);
    dispatchInfoWithImplicitArgs.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfoWithImplicitArgs.setTotalNumberOfWorkgroups({1, 1, 1});
    MultiDispatchInfo multiDispatchInfoWithImplicitArgs(&kernelWithoutImplicitArgs);
    multiDispatchInfoWithImplicitArgs.push(dispatchInfoWithImplicitArgs);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfoWithImplicitArgs,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto iohSizeWithImplicitArgs = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(kernelWithImplicitArgs, Math::computeTotalElementsCount(localWorkgroupSize));

    EXPECT_LE(iohSizeWithoutImplicitArgs, iohSizeWithImplicitArgs);

    {
        auto numChannels = kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;
        uint32_t grfSize = sizeof(typename FamilyType::GRF);
        auto size = kernelWithImplicitArgs.getCrossThreadDataSize() +
                    HardwareCommandsHelper<FamilyType>::getPerThreadDataSizeTotal(kernelInfo.getMaxSimdSize(), grfSize, numChannels, Math::computeTotalElementsCount(localWorkgroupSize)) +
                    sizeof(ImplicitArgs) +
                    alignUp(HardwareCommandsHelper<FamilyType>::getPerThreadDataSizeTotal(kernelInfo.getMaxSimdSize(), grfSize, 3u, Math::computeTotalElementsCount(localWorkgroupSize)), MemoryConstants::cacheLineSize);

        size = alignUp(size, MemoryConstants::cacheLineSize);
        EXPECT_EQ(size, iohSizeWithImplicitArgs);
    }
}
