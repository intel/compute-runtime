/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/test/common/helpers/blit_commands_helper_tests.inl"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"

#include "gtest/gtest.h"

using BlitTests = Test<DeviceFixture>;

HWTEST2_F(BlitTests, givenOneBytePatternWhenFillPatternWithBlitThenCommandIsProgrammed, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenDeviceWithoutDefaultGmmWhenAppendBlitCommandsForVillBufferThenDstCompressionDisabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    }
}

HWTEST2_F(BlitTests, givenGmmWithDisabledCompresionWhenAppendBlitCommandsForVillBufferThenDstCompressionDisabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->isCompressionEnabled = false;

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0u);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    }
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForVillBufferThenDstCompressionEnabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->isCompressionEnabled = true;

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0u);
    const auto &rootDeviceEnvironment = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()];
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *rootDeviceEnvironment);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);

        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment->getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        EXPECT_EQ(compressionFormat, blitCmd->getCompressionFormat40());
    }
}

HWTEST2_F(BlitTests, givenOverridedMocksValueWhenAppendBlitCommandsForVillBufferThenDebugMocksValueIsSet, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    uint32_t mockValue = 5;
    DebugManager.flags.OverrideBlitterMocs.set(mockValue);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationMOCS(), mockValue);
    }
}

HWTEST2_F(BlitTests, givenEnableStatelessCompressionWithUnifiedMemoryAndSystemMemWhenAppendBlitCommandsForVillBufferThenCompresionDisabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    }
}

HWTEST2_F(BlitTests, givenEnableStatelessCompressionWithUnifiedMemoryAndLocalMemWhenAppendBlitCommandsForVillBufferThenCompresionEnabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        EXPECT_EQ(static_cast<uint32_t>(DebugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get()), blitCmd->getCompressionFormat40());
    }
}

HWTEST2_F(BlitTests, givenMemorySizeBiggerThanMaxWidthButLessThanTwiceMaxWidthWhenFillPatternWithBlitThenHeightIsOne, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth) - 1,
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(cmd->getFillHeight(), 1u);
    }
}

HWTEST2_F(BlitTests, givenMemorySizeTwiceBiggerThanMaxWidthWhenFillPatternWithBlitThenHeightIsTwo, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint8_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(cmd->getFillHeight(), 2u);
    }
}

struct BlitTestsTestXeHpc : BlitColorTests {};

template <typename FamilyType>
class GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedPVC : public GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> {
  public:
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedPVC(Device *device) : GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType>(device) {}
};

template <typename FamilyType>
typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH getColorDepth(size_t patternSize) {
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

HWTEST2_P(BlitTestsTestXeHpc, givenCommandStreamWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, IsXeHpcCore) {
    auto patternSize = GetParam();
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedPVC<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}

INSTANTIATE_TEST_CASE_P(size_t,
                        BlitTestsTestXeHpc,
                        testing::Values(2,
                                        4,
                                        8,
                                        16));

HWTEST2_F(BlitTests, givenMemoryAndImageWhenDispatchCopyImageCallThenCommandAddedToStream, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    MockGraphicsAllocation srcAlloc;
    MockGraphicsAllocation dstAlloc;
    MockGraphicsAllocation clearColorAlloc;

    Vec3<size_t> dstOffsets = {0, 0, 0};
    Vec3<size_t> srcOffsets = {0, 0, 0};

    Vec3<size_t> copySize = {0x100, 0x40, 0x1};
    Vec3<size_t> srcSize = {0x100, 0x40, 0x1};
    Vec3<size_t> dstSize = {0x100, 0x40, 0x1};

    size_t srcRowPitch = srcSize.x;
    size_t srcSlicePitch = srcSize.y;
    size_t dstRowPitch = dstSize.x;
    size_t dstSlicePitch = dstSize.y;

    auto blitProperties = BlitProperties::constructPropertiesForCopy(&dstAlloc, &srcAlloc,
                                                                     dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                     dstRowPitch, dstSlicePitch, &clearColorAlloc);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    blitProperties.bytesPerPixel = 4;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenBlockCopyCommandWhenAppendBlitCommandsForImagesThenNothingChanged, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}
HWTEST2_F(BlitTests, givenBlockCopyCommandWhenAppendColorDepthThenNothingChanged, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    NEO::BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}
HWTEST2_F(BlitTests, givenBlockCopyCommandWhenAppendSliceOffsetThenNothingChanged, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    auto srcSlicePitch = 0u;
    auto dstSlicePitch = 0u;

    NEO::BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, bltCmd, 0, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}
HWTEST2_F(BlitTests, givenBlockCopyCommandWhenAppendSurfaceTypeThenNothingChanged, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    NEO::BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}
HWTEST2_F(BlitTests, givenBlockCopyCommandWhenAppendTilingTypeThenNothingChanged, IsPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    NEO::BlitCommandsHelper<FamilyType>::appendTilingType(GMM_NOT_TILED, GMM_NOT_TILED, bltCmd);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}
