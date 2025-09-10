/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "gtest/gtest.h"

using BlitTests = Test<DeviceFixture>;

HWTEST2_F(BlitTests, givenOneBytePatternWhenFillPatternWithBlitThenCommandIsProgrammed, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenOneBytePatternWhenFillPatternWithSystemMemoryBlitThenCommandIsProgrammed, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    void *dstPtr = malloc(4);
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(nullptr, reinterpret_cast<uint64_t>(dstPtr), sizeof(uint32_t), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    free(dstPtr);
}

HWTEST2_F(BlitTests, givenDeviceWithoutDefaultGmmWhenAppendBlitCommandsForVillBufferThenDstCompressionDisabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    gmm->setCompressionEnabled(false);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);
    mockAllocation.setGmm(gmm.get(), 0u);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    gmm->setCompressionEnabled(true);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0u);
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, rootDeviceEnvironment);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);

        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        EXPECT_EQ(compressionFormat, blitCmd->getCompressionFormat40());
    }
}

HWTEST2_F(BlitTests, givenOverridedMocksValueWhenAppendBlitCommandsForVillBufferThenDebugMocksValueIsSet, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    uint32_t mockValue = pDevice->getGmmHelper()->getL3EnabledMOCS() + 1;
    debugManager.flags.OverrideBlitterMocs.set(mockValue);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
    }
}

HWTEST2_F(BlitTests, whenGetMaxBlitHeightOverrideThenReturnProperValue, IsPVC) {
    auto ret = BlitCommandsHelper<FamilyType>::getMaxBlitHeightOverride(pDevice->getRootDeviceEnvironment(), true);
    EXPECT_EQ(ret, 512u);

    ret = BlitCommandsHelper<FamilyType>::getMaxBlitHeightOverride(pDevice->getRootDeviceEnvironment(), false);
    EXPECT_EQ(ret, 0u);
}

HWTEST2_F(BlitTests, givenEnableStatelessCompressionWithUnifiedMemoryAndLocalMemWhenAppendBlitCommandsForVillBufferThenCompresionEnabled, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getDestinationCompressible(), MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get()), blitCmd->getCompressionFormat40());
    }
}

HWTEST2_F(BlitTests, givenMemorySizeBiggerThanMaxWidthButLessThanTwiceMaxWidthWhenFillPatternWithBlitThenHeightIsOne, IsPVC) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth) - 1,
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    test.testBodyImpl(patternSize, expecttedDepth);
}

INSTANTIATE_TEST_SUITE_P(size_t,
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

    auto blitProperties = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0, &srcAlloc, 0,
                                                                     dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                     dstRowPitch, dstSlicePitch, &clearColorAlloc);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    blitProperties.bytesPerPixel = 4;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
HWTEST2_F(BlitTests, givenCopyCommandListWhenBytesPerPixelIsCalledForNonBlitCopySupportedPlatformThenExpectOne, IsXeHpcCore) {
    size_t copySize{}, srcSize{}, dstSize{};
    uint32_t srcOrigin{}, dstOrigin{};
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);
    EXPECT_EQ(bytesPerPixel, 1u);
}

HWTEST2_F(BlitTests, GivenDispatchDummyBlitWhenRootDeviceEnviromentResourcesAreReleasedThenDummyAllocationIsNull, IsXeHpcCore) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()].get();

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    EXPECT_EQ(nullptr, rootDeviceEnvironment->getDummyAllocation());

    EncodeDummyBlitWaArgs waArgs{true, rootDeviceEnvironment};
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgs);
    EXPECT_NE(nullptr, rootDeviceEnvironment->getDummyAllocation());

    executionEnvironment->releaseRootDeviceEnvironmentResources(rootDeviceEnvironment);
    EXPECT_EQ(nullptr, rootDeviceEnvironment->getDummyAllocation());
}

HWTEST2_F(BlitTests, GivenDispatchDummyBlitWhenWorkaroundIsNotRequiredThenDummyAllocationIsNull, IsXeHpcCore) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()].get();

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    EXPECT_EQ(nullptr, rootDeviceEnvironment->getDummyAllocation());

    EncodeDummyBlitWaArgs waArgs{false, rootDeviceEnvironment};
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgs);
    EXPECT_EQ(nullptr, rootDeviceEnvironment->getDummyAllocation());
}
