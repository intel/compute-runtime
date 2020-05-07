/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(BlitCommandsHelperTest, GivenBufferParamsWhenConstructingPropertiesForBufferRegionsThenPropertiesCreatedCorrectly) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 2, 2};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopyBuffer(dstAlloc.get(), srcAlloc.get(),
                                                                                dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                                dstRowPitch, dstSlicePitch);

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::BufferToBuffer);
    EXPECT_EQ(blitProperties.dstAllocation, dstAlloc.get());
    EXPECT_EQ(blitProperties.srcAllocation, srcAlloc.get());
    EXPECT_EQ(blitProperties.dstGpuAddress, dstGpuAddr);
    EXPECT_EQ(blitProperties.srcGpuAddress, srcGpuAddr);
    EXPECT_EQ(blitProperties.copySize, copySize);
    EXPECT_EQ(blitProperties.dstOffset, dstOffsets);
    EXPECT_EQ(blitProperties.srcOffset, srcOffsets);
    EXPECT_EQ(blitProperties.dstRowPitch, dstRowPitch);
    EXPECT_EQ(blitProperties.dstSlicePitch, dstSlicePitch);
    EXPECT_EQ(blitProperties.srcRowPitch, srcRowPitch);
    EXPECT_EQ(blitProperties.srcSlicePitch, srcSlicePitch);
}

TEST(BlitCommandsHelperTest, GivenCopySizeYAndZEqual0WhenConstructingPropertiesForBufferRegionsThenCopyZAndZEqual1) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 0, 0};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopyBuffer(dstAlloc.get(), srcAlloc.get(),
                                                                                dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                                dstRowPitch, dstSlicePitch);
    Vec3<size_t> expectedSize{copySize.x, 1, 1};
    EXPECT_EQ(blitProperties.copySize, expectedSize);
}

using BlitTests = Test<DeviceFixture>;

HWTEST_F(BlitTests, givenDebugVariablesWhenGettingMaxBlitSizeThenHonorUseProvidedValues) {
    DebugManagerStateRestore restore{};

    ASSERT_EQ(BlitterConstants::maxBlitWidth, BlitCommandsHelper<FamilyType>::getMaxBlitWidth());
    ASSERT_EQ(BlitterConstants::maxBlitHeight, BlitCommandsHelper<FamilyType>::getMaxBlitHeight());

    DebugManager.flags.LimitBlitterMaxWidth.set(50);
    EXPECT_EQ(50u, BlitCommandsHelper<FamilyType>::getMaxBlitWidth());

    DebugManager.flags.LimitBlitterMaxHeight.set(60);
    EXPECT_EQ(60u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight());
}

HWTEST_F(BlitTests, givenDebugVariableWhenEstimatingPostBlitsCommandSizeThenReturnCorrectResult) {
    DebugManagerStateRestore restore{};

    ASSERT_EQ(sizeof(typename FamilyType::MI_ARB_CHECK), BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());

    DebugManager.flags.FlushAfterEachBlit.set(1);
    EXPECT_EQ(sizeof(typename FamilyType::MI_FLUSH_DW), BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());
}

HWTEST_F(BlitTests, givenDebugVariableWhenDispatchingPostBlitsCommandThenUseCorrectCommands) {
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    DebugManagerStateRestore restore{};
    uint32_t streamBuffer[100] = {};
    LinearStream linearStream{streamBuffer, sizeof(streamBuffer)};
    GenCmdList commands{};

    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream);
    CmdParse<FamilyType>::parseCommandBuffer(commands, linearStream.getCpuBase(), linearStream.getUsed());
    auto arbCheck = find<MI_ARB_CHECK *>(commands.begin(), commands.end());
    EXPECT_NE(commands.end(), arbCheck);

    memset(streamBuffer, 0, sizeof(streamBuffer));
    linearStream.replaceBuffer(streamBuffer, sizeof(streamBuffer));
    commands.clear();

    DebugManager.flags.FlushAfterEachBlit.set(1);
    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream);
    CmdParse<FamilyType>::parseCommandBuffer(commands, linearStream.getCpuBase(), linearStream.getUsed());
    auto miFlush = find<MI_FLUSH_DW *>(commands.begin(), commands.end());
    EXPECT_NE(commands.end(), miFlush);
}

HWTEST_F(BlitTests, givenMemoryWhenFillPatternWithBlitThenCommandIsProgrammed) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(BlitTests, givenMemorySizeBiggerThanMaxWidthButLessThanTwiceMaxWidthWhenFillPatternWithBlitThenHeightIsOne) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitWidth) - 1,
                                          MemoryPool::System4KBPages);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getTransferHeight(), 1u);
    }
}

HWTEST_F(BlitTests, givenMemorySizeTwiceBiggerThanMaxWidthWhenFillPatternWithBlitThenHeightIsTwo) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitWidth),
                                          MemoryPool::System4KBPages);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getTransferHeight(), 2u);
    }
}

using BlitPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(BlitTests, givenMemoryWhenFillPatternSizeIs4BytesThen32BitMaskISSetCorrectly, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, &pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(XY_COLOR_BLT::_32BPP_BYTE_MASK::_32BPP_BYTE_MASK_WRITE_RGBA_CHANNEL, cmd->get32BppByteMask());
    }
}

template <typename FamilyType>
typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH getColorDepth(size_t patternSize) {
    using COLOR_DEPTH = typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH;
    COLOR_DEPTH depth = {};
    switch (patternSize) {
    case 1:
        depth = COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR;
        break;
    case 2:
        depth = COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR1555;
        break;
    case 4:
        depth = COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR;
        break;
    }
    return depth;
}

HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualOneWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitPlatforms) {
    size_t patternSize = 1;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualTwoWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitPlatforms) {
    size_t patternSize = 2;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualFourWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitPlatforms) {
    size_t patternSize = 4;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}
