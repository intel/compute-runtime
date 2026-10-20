/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_launch_params.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

CmdListNonUniformLaunchArgs createNonUniformLaunchArgs(uint32_t workDim, std::array<uint32_t, 3> globalWorkSize, std::array<uint32_t, 3> enqueuedGroupSize) {
    CmdListNonUniformLaunchArgs launchArgs{};
    launchArgs.workDim = workDim;
    for (uint32_t dim = 0u; dim < 3u; dim++) {
        launchArgs.globalWorkSize[dim] = globalWorkSize[dim];
        launchArgs.enqueuedGroupSize[dim] = enqueuedGroupSize[dim];
    }
    return launchArgs;
}

void expectRegion(const CmdListNonUniformLaunchRegion &region, std::array<uint32_t, 3> groupSize, std::array<uint32_t, 3> groupStart, std::array<uint32_t, 3> groupCount) {
    EXPECT_EQ(groupSize[0], region.groupSize[0]);
    EXPECT_EQ(groupSize[1], region.groupSize[1]);
    EXPECT_EQ(groupSize[2], region.groupSize[2]);
    EXPECT_EQ(groupStart[0], region.groupStart[0]);
    EXPECT_EQ(groupStart[1], region.groupStart[1]);
    EXPECT_EQ(groupStart[2], region.groupStart[2]);
    EXPECT_EQ(groupCount[0], region.groupCount.groupCountX);
    EXPECT_EQ(groupCount[1], region.groupCount.groupCountY);
    EXPECT_EQ(groupCount[2], region.groupCount.groupCountZ);
}

TEST(CmdListNonUniformLaunchTests, givenOneDimensionalRangeWithRemainderWhenComputingRegionsThenMainAndRemainderRegionsAreCreated) {
    const CmdListNonUniformLaunch nonUniformLaunch(createNonUniformLaunchArgs(1u, {10u, 1u, 1u}, {4u, 1u, 1u}));

    ASSERT_EQ(2u, nonUniformLaunch.regionCount);
    expectRegion(nonUniformLaunch.regions[0], {4u, 1u, 1u}, {0u, 0u, 0u}, {2u, 1u, 1u});
    expectRegion(nonUniformLaunch.regions[1], {2u, 1u, 1u}, {2u, 0u, 0u}, {1u, 1u, 1u});
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[0]);
    EXPECT_EQ(1u, nonUniformLaunch.totalGroupCount[1]);
    EXPECT_EQ(1u, nonUniformLaunch.totalGroupCount[2]);
}

TEST(CmdListNonUniformLaunchTests, givenTwoDimensionalRangeWithRemainderInBothDimensionsWhenComputingRegionsThenFourRegionsAreCreated) {
    const CmdListNonUniformLaunch nonUniformLaunch(createNonUniformLaunchArgs(2u, {10u, 7u, 1u}, {4u, 3u, 1u}));

    ASSERT_EQ(4u, nonUniformLaunch.regionCount);
    expectRegion(nonUniformLaunch.regions[0], {4u, 3u, 1u}, {0u, 0u, 0u}, {2u, 2u, 1u});
    expectRegion(nonUniformLaunch.regions[1], {2u, 3u, 1u}, {2u, 0u, 0u}, {1u, 2u, 1u});
    expectRegion(nonUniformLaunch.regions[2], {4u, 1u, 1u}, {0u, 2u, 0u}, {2u, 1u, 1u});
    expectRegion(nonUniformLaunch.regions[3], {2u, 1u, 1u}, {2u, 2u, 0u}, {1u, 1u, 1u});
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[0]);
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[1]);
    EXPECT_EQ(1u, nonUniformLaunch.totalGroupCount[2]);
}

TEST(CmdListNonUniformLaunchTests, givenThreeDimensionalRangeWithRemainderInEveryDimensionWhenComputingRegionsThenEightRegionsCoverWholeRange) {
    const CmdListNonUniformLaunch nonUniformLaunch(createNonUniformLaunchArgs(3u, {10u, 7u, 5u}, {4u, 3u, 2u}));

    ASSERT_EQ(CmdListNonUniformLaunch::maxRegionCount, nonUniformLaunch.regionCount);
    expectRegion(nonUniformLaunch.regions[0], {4u, 3u, 2u}, {0u, 0u, 0u}, {2u, 2u, 2u});
    expectRegion(nonUniformLaunch.regions[7], {2u, 1u, 1u}, {2u, 2u, 2u}, {1u, 1u, 1u});

    uint32_t coveredWorkItems = 0u;
    for (uint32_t regionIndex = 0u; regionIndex < nonUniformLaunch.regionCount; regionIndex++) {
        const auto &region = nonUniformLaunch.regions[regionIndex];
        coveredWorkItems += region.groupSize[0] * region.groupSize[1] * region.groupSize[2] *
                            region.groupCount.groupCountX * region.groupCount.groupCountY * region.groupCount.groupCountZ;
    }
    EXPECT_EQ(10u * 7u * 5u, coveredWorkItems);
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[0]);
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[1]);
    EXPECT_EQ(3u, nonUniformLaunch.totalGroupCount[2]);
}

TEST(CmdListNonUniformLaunchTests, givenRemainderOnlyInSecondDimensionWhenComputingRegionsThenFirstDimensionIsNotSplit) {
    const CmdListNonUniformLaunch nonUniformLaunch(createNonUniformLaunchArgs(2u, {8u, 10u, 1u}, {4u, 4u, 1u}));

    ASSERT_EQ(2u, nonUniformLaunch.regionCount);
    expectRegion(nonUniformLaunch.regions[0], {4u, 4u, 1u}, {0u, 0u, 0u}, {2u, 2u, 1u});
    expectRegion(nonUniformLaunch.regions[1], {4u, 2u, 1u}, {0u, 2u, 0u}, {2u, 1u, 1u});
}

TEST(CmdListNonUniformLaunchTests, givenEnqueuedGroupSizeLargerThanGlobalSizeWhenComputingRegionsThenSingleRegionWithGlobalSizeIsCreated) {
    const CmdListNonUniformLaunch nonUniformLaunch(createNonUniformLaunchArgs(1u, {3u, 1u, 1u}, {8u, 1u, 1u}));

    ASSERT_EQ(1u, nonUniformLaunch.regionCount);
    expectRegion(nonUniformLaunch.regions[0], {3u, 1u, 1u}, {0u, 0u, 0u}, {1u, 1u, 1u});
    EXPECT_EQ(1u, nonUniformLaunch.totalGroupCount[0]);
}

using CommandListAppendLaunchKernelNonUniform = Test<ModuleMutableCommandListFixture>;

HWTEST2_F(CommandListAppendLaunchKernelNonUniform, givenNonUniformLaunchWhenAppendingThenWalkerIsDispatchedPerRegionWithStartingGroupIdAndGroupSizeIsRestored, IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    const auto launchArgs = createNonUniformLaunchArgs(2u, {10u, 7u, 1u}, {4u, 3u, 1u});
    const CmdListNonUniformLaunch nonUniformLaunch(launchArgs);
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setGroupSize(4u, 3u, 1u));

    auto commandStream = commandList->getCmdContainer().getCommandStream();
    auto usedSpaceBefore = commandStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernelNonUniform(kernel->toHandle(), launchArgs, nullptr, 0u, nullptr, false));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(commandStream->getCpuBase(), usedSpaceBefore), commandStream->getUsed() - usedSpaceBefore));
    auto walkers = findAll<WalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(nonUniformLaunch.regionCount, walkers.size());

    for (uint32_t regionIndex = 0u; regionIndex < nonUniformLaunch.regionCount; regionIndex++) {
        const auto &region = nonUniformLaunch.regions[regionIndex];
        auto walker = genCmdCast<WalkerType *>(*walkers[regionIndex]);
        EXPECT_EQ(region.groupStart[0], walker->getThreadGroupIdStartingX());
        EXPECT_EQ(region.groupStart[1], walker->getThreadGroupIdStartingY());
        EXPECT_EQ(region.groupStart[2], walker->getThreadGroupIdStartingZ());
        EXPECT_EQ(region.groupStart[0] + region.groupCount.groupCountX, walker->getThreadGroupIdXDimension());
        EXPECT_EQ(region.groupStart[1] + region.groupCount.groupCountY, walker->getThreadGroupIdYDimension());
        EXPECT_EQ(region.groupStart[2] + region.groupCount.groupCountZ, walker->getThreadGroupIdZDimension());
    }

    EXPECT_EQ(4u, kernel->getGroupSize()[0]);
    EXPECT_EQ(3u, kernel->getGroupSize()[1]);
    EXPECT_EQ(1u, kernel->getGroupSize()[2]);
}

HWTEST_F(CommandListAppendLaunchKernelNonUniform, givenImmediateCommandListWhenAppendingNonUniformLaunchThenSuccessIsReturnedAndGroupSizeIsRestored) {
    const auto launchArgs = createNonUniformLaunchArgs(1u, {10u, 1u, 1u}, {4u, 1u, 1u});
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setGroupSize(4u, 1u, 1u));

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate->appendLaunchKernelNonUniform(kernel->toHandle(), launchArgs, nullptr, 0u, nullptr, false));

    EXPECT_EQ(4u, kernel->getGroupSize()[0]);
    EXPECT_EQ(1u, kernel->getGroupSize()[1]);
    EXPECT_EQ(1u, kernel->getGroupSize()[2]);
}

HWTEST2_F(CommandListAppendLaunchKernelNonUniform, givenKernelWithRequiredWorkGroupSizeWhenAppendingNonUniformLaunchThenRemainderRegionIsDispatchedWithSmallerGroupSize, IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    const auto launchArgs = createNonUniformLaunchArgs(1u, {10u, 1u, 1u}, {4u, 1u, 1u});
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setGroupSize(4u, 1u, 1u));
    mockKernelImmData->kernelDescriptor->kernelAttributes.requiredWorkgroupSize[0] = 4u;
    mockKernelImmData->kernelDescriptor->kernelAttributes.requiredWorkgroupSize[1] = 1u;
    mockKernelImmData->kernelDescriptor->kernelAttributes.requiredWorkgroupSize[2] = 1u;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel->setGroupSize(2u, 1u, 1u));

    auto commandStream = commandList->getCmdContainer().getCommandStream();
    auto usedSpaceBefore = commandStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernelNonUniform(kernel->toHandle(), launchArgs, nullptr, 0u, nullptr, false));

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(commandStream->getCpuBase(), usedSpaceBefore), commandStream->getUsed() - usedSpaceBefore));
    EXPECT_EQ(2u, findAll<WalkerType *>(cmdList.begin(), cmdList.end()).size());
    EXPECT_EQ(4u, kernel->getGroupSize()[0]);
}

struct RemainderRejectingKernel : public ModuleImmutableDataFixture::MockKernel {
    using ModuleImmutableDataFixture::MockKernel::MockKernel;

    ze_result_t setNonUniformRegionGroupSize(uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) override {
        if (groupSizeX != acceptedGroupSizeX) {
            return ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION;
        }
        return ModuleImmutableDataFixture::MockKernel::setNonUniformRegionGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    }

    uint32_t acceptedGroupSizeX = 4u;
};

HWTEST_F(CommandListAppendLaunchKernelNonUniform, givenRemainderRegionGroupSizeRejectedByKernelWhenAppendingNonUniformLaunchThenErrorIsReturnedWithoutEncodingAnyCommandAndGroupSizeIsRestored) {
    auto rejectingKernel = std::make_unique<RemainderRejectingKernel>(module.get());
    createKernel(rejectingKernel.get());
    ASSERT_EQ(ZE_RESULT_SUCCESS, rejectingKernel->setGroupSize(4u, 1u, 1u));
    const auto launchArgs = createNonUniformLaunchArgs(1u, {10u, 1u, 1u}, {4u, 1u, 1u});

    auto commandStream = commandList->getCmdContainer().getCommandStream();
    const auto usedSpaceBefore = commandStream->getUsed();
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, commandList->appendLaunchKernelNonUniform(rejectingKernel->toHandle(), launchArgs, nullptr, 0u, nullptr, false));

    EXPECT_EQ(usedSpaceBefore, commandStream->getUsed());
    EXPECT_EQ(4u, rejectingKernel->getGroupSize()[0]);
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct FailingSplitLaunchCommandList : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
    using BaseClass = WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>;

    ze_result_t appendLaunchKernelSplit(::L0::Kernel *kernel, const ze_group_count_t &threadGroupDimensions, ::L0::Event *event, CmdListKernelLaunchParams &launchParams) override {
        if (appendLaunchKernelSplitCalled++ == failingCallIndex) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return BaseClass::appendLaunchKernelSplit(kernel, threadGroupDimensions, event, launchParams);
    }

    uint32_t appendLaunchKernelSplitCalled = 0u;
    uint32_t failingCallIndex = 1u;
};

HWTEST_F(CommandListAppendLaunchKernelNonUniform, givenRegionLaunchFailingWhenAppendingNonUniformLaunchThenErrorIsReturnedAndGroupSizeIsRestored) {
    FailingSplitLaunchCommandList<FamilyType::gfxCoreFamily> failingCommandList;
    ASSERT_EQ(ZE_RESULT_SUCCESS, failingCommandList.initialize(device, NEO::EngineGroupType::compute, 0u));
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setGroupSize(4u, 1u, 1u));
    const auto launchArgs = createNonUniformLaunchArgs(1u, {10u, 1u, 1u}, {4u, 1u, 1u});

    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, failingCommandList.appendLaunchKernelNonUniform(kernel->toHandle(), launchArgs, nullptr, 0u, nullptr, false));

    EXPECT_EQ(2u, failingCommandList.appendLaunchKernelSplitCalled);
    EXPECT_EQ(4u, kernel->getGroupSize()[0]);
}

using InOrderCmdListNonUniformLaunchTests = InOrderCmdListFixture;

HWTEST2_F(InOrderCmdListNonUniformLaunchTests, givenInOrderCmdListWhenAppendingNonUniformLaunchThenInOrderCounterIsIncrementedOnceAndEventIsSignaledWithIt, IsAtLeastXeCore) {
    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto eventPool = createEvents<FamilyType>(1, false);

    const auto launchArgs = createNonUniformLaunchArgs(2u, {10u, 7u, 1u}, {4u, 3u, 1u});
    ASSERT_EQ(ZE_RESULT_SUCCESS, kernel->setGroupSize(4u, 3u, 1u));

    EXPECT_EQ(ZE_RESULT_SUCCESS, immCmdList->appendLaunchKernelNonUniform(kernel->toHandle(), launchArgs, events[0]->toHandle(), 0u, nullptr, false));

    EXPECT_EQ(1u, immCmdList->inOrderExecInfo->getCounterValue());
    EXPECT_EQ(1u, events[0]->getInOrderExecBaseSignalValue());
}

} // namespace ult
} // namespace L0
