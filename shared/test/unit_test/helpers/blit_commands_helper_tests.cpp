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

using BlitTests = Test<ClDeviceFixture>;

HWTEST_F(BlitTests, givenDebugVariablesWhenGettingMaxBlitSizeThenHonorUseProvidedValues) {
    DebugManagerStateRestore restore{};

    ASSERT_EQ(BlitterConstants::maxBlitWidth, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pClDevice->getRootDeviceEnvironment()));
    ASSERT_EQ(BlitterConstants::maxBlitHeight, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pClDevice->getRootDeviceEnvironment()));

    DebugManager.flags.LimitBlitterMaxWidth.set(50);
    EXPECT_EQ(50u, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pClDevice->getRootDeviceEnvironment()));

    DebugManager.flags.LimitBlitterMaxHeight.set(60);
    EXPECT_EQ(60u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pClDevice->getRootDeviceEnvironment()));
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
                                          MemoryPool::System4KBPages, mockMaxOsContextCount);
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
                                          MemoryPool::System4KBPages, mockMaxOsContextCount);
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
                                          MemoryPool::System4KBPages, mockMaxOsContextCount);
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

using BlitColor = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE_LP>;

HWTEST2_F(BlitTests, givenMemoryWhenFillPatternSizeIs4BytesThen32BitMaskISSetCorrectly, BlitColor) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, mockMaxOsContextCount);
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

HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualOneWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitColor) {
    size_t patternSize = 1;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualTwoWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitColor) {
    size_t patternSize = 2;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualFourWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitColor) {
    size_t patternSize = 4;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}

using BlitPlatforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
template <typename FamilyType>
typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH getFastColorDepth(size_t patternSize) {
    using COLOR_DEPTH = typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH;
    COLOR_DEPTH depth = {};
    switch (patternSize) {
    case 1:
        depth = COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR;
        break;
    case 2:
        depth = COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR;
        break;
    case 4:
        depth = COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR;
        break;
    case 8:
        depth = COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR;
        break;
    case 16:
        depth = COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR;
        break;
    }
    return depth;
}

using BlitFastColorTest = BlitColorTests;

HWTEST2_P(BlitFastColorTest, givenCommandStreamWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, IsGen12LP) {
    auto patternSize = GetParam();
    auto expecttedDepth = getFastColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}

INSTANTIATE_TEST_CASE_P(size_t,
                        BlitFastColorTest,
                        testing::Values(1,
                                        2,
                                        4,
                                        8,
                                        16));
HWTEST2_F(BlitTests, givenMemoryAndImageWhenDispatchCopyImageCallThenCommandAddedToStream, BlitPlatforms) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    MockGraphicsAllocation srcAlloc;
    MockGraphicsAllocation dstAlloc;

    Vec3<size_t> dstOffsets = {0, 0, 0};
    Vec3<size_t> srcOffsets = {0, 0, 0};

    Vec3<size_t> copySize = {0x100, 0x40, 0x1};
    Vec3<uint32_t> srcSize = {0x100, 0x40, 0x1};
    Vec3<uint32_t> dstSize = {0x100, 0x40, 0x1};

    uint32_t srcRowPitch = srcSize.x;
    uint32_t srcSlicePitch = srcSize.y;
    uint32_t dstRowPitch = dstSize.x;
    uint32_t dstSlicePitch = dstSize.y;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopyBuffer(&dstAlloc, &srcAlloc,
                                                                                dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                                dstRowPitch, dstSlicePitch);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    blitProperties.bytesPerPixel = 4;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsRegion(blitProperties, stream, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenGen9AndGetBlitAllocationPropertiesThenCorrectValuesAreReturned, IsGen9) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    MockGraphicsAllocation alloc;
    uint32_t pitch = 0x10;
    uint32_t qPitch = 0x20;
    GMM_TILE_TYPE tileType = GMM_NOT_TILED;
    uint32_t mipTailLod = 0;

    auto expectedPitch = pitch;
    auto expectedQPitch = qPitch;
    auto expectedtileType = tileType;
    auto expectedMipTailLod = mipTailLod;

    NEO::BlitCommandsHelper<FamilyType>::getBlitAllocationProperties(alloc, pitch, qPitch, tileType, mipTailLod);

    EXPECT_EQ(expectedPitch, pitch);
    EXPECT_EQ(expectedQPitch, qPitch);
    EXPECT_EQ(expectedtileType, tileType);
    EXPECT_EQ(expectedMipTailLod, mipTailLod);
}

using BlitTestsParams = BlitColorTests;

HWTEST2_P(BlitTestsParams, givenCopySizeAlignedWithin1and16ByteWhenGettingBytesPerPixelThenCorectPixelSizeReturned, BlitPlatforms) {
    size_t copySize = 33;
    auto aligment = GetParam();
    copySize = alignUp(copySize, aligment);
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcOrigin = dstOrigin = 0;
    srcSize = dstSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, aligment);
}

HWTEST2_P(BlitTestsParams, givenSrcSizeAlignedWithin1and16ByteWhenGettingBytesPerPixelThenCorectPixelSizeReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto aligment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcSize = 33;
    srcSize = alignUp(srcSize, aligment);
    srcOrigin = dstOrigin = dstSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, aligment);
}

HWTEST2_P(BlitTestsParams, givenDstSizeAlignedWithin1and16ByteWhenGettingBytesPerPixelThenCorectPixelSizeReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto aligment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstSize = 33;
    dstSize = alignUp(dstSize, aligment);
    srcOrigin = dstOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, aligment);
}

HWTEST2_P(BlitTestsParams, givenSrcOriginAlignedWithin1and16ByteWhenGettingBytesPerPixelThenCorectPixelSizeReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto aligment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcOrigin = 33;
    srcOrigin = alignUp(srcOrigin, aligment);
    dstSize = dstOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, aligment);
}

HWTEST2_P(BlitTestsParams, givenDrcOriginAlignedWithin1and16ByteWhenGettingBytesPerPixelThenCorectPixelSizeReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto aligment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstOrigin = 33;
    dstOrigin = alignUp(dstOrigin, aligment);
    dstSize = srcOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, aligment);
}

INSTANTIATE_TEST_CASE_P(size_t,
                        BlitTestsParams,
                        testing::Values(1,
                                        2,
                                        4,
                                        8,
                                        16));

HWTEST2_F(BlitTests, givenAllocWidthGreaterThanMaxBlitWidthWhenCheckingIfOneCommandCanBeUsedThenFalseReturned, BlitPlatforms) {
    uint32_t bytesPerPixel = 1;
    Vec3<size_t> copySize = {BlitterConstants::maxBlitWidth + 1, 1, 1};
    bool useOneCommand = NEO::BlitCommandsHelper<FamilyType>::useOneBlitCopyCommand(copySize, bytesPerPixel);
    EXPECT_FALSE(useOneCommand);
}

HWTEST2_F(BlitTests, givenAllocHeightGreaterThanMaxBlitHeightWhenCheckingIfOneCommandCanBeUsedThenFalseReturned, BlitPlatforms) {
    uint32_t bytesPerPixel = 1;
    Vec3<size_t> copySize = {0x10, BlitterConstants::maxBlitHeight + 1, 1};
    bool useOneCommand = NEO::BlitCommandsHelper<FamilyType>::useOneBlitCopyCommand(copySize, bytesPerPixel);
    EXPECT_FALSE(useOneCommand);
}
HWTEST2_F(BlitTests, givenAllocDimsLowerThanMaxSizesWhenCheckingIfOneCommandCanBeUsedThenTrueReturned, BlitPlatforms) {
    uint32_t bytesPerPixel = 1;
    Vec3<size_t> copySize = {BlitterConstants::maxBlitWidth - 1, BlitterConstants::maxBlitHeight - 1, 1};
    bool useOneCommand = NEO::BlitCommandsHelper<FamilyType>::useOneBlitCopyCommand(copySize, bytesPerPixel);
    EXPECT_TRUE(useOneCommand);
}