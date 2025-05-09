/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "gtest/gtest.h"

using BlitTests = Test<DeviceFixture>;

HWTEST2_F(BlitTests, givenOneBytePatternWhenFillPatternWithBlitThenCommandIsProgrammed, IsXe2HpgCore) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenOverridedMocksValueWhenAppendBlitCommandsForVillBufferThenDebugMocksValueIsSet, IsXe2HpgCore) {
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

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

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

HWTEST2_F(BlitTests, givenEnableStatelessCompressionWithUnifiedMemoryAndSystemMemWhenAppendBlitCommandsForVillBufferThenCompresionDisabled, IsXe2HpgCore) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.set(2);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getCompressionFormat(), 0u);
    }
}

HWTEST2_F(BlitTests, givenEnableStatelessCompressionWithUnifiedMemoryAndLocalMemWhenAppendBlitCommandsForVillBufferThenCompresionEnabled, IsXe2HpgCore) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);
    debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.set(2);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);
        EXPECT_EQ(blitCmd->getCompressionFormat(), 2u);
    }
}

HWTEST2_F(BlitTests, givenMemorySizeBiggerThanMaxWidthButLessThanTwiceMaxWidthWhenFillPatternWithBlitThenHeightIsOne, IsXe2HpgCore) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth) - 1,
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

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

HWTEST2_F(BlitTests, givenMemorySizeTwiceBiggerThanMaxWidthWhenFillPatternWithBlitThenHeightIsTwo, IsXe2HpgCore) {
    using MEM_SET = typename FamilyType::MEM_SET;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitSetWidth),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

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

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForFillBufferThenDstCompressionEnabled, IsXe2HpgCore) {
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
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, rootDeviceEnvironment);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto blitCmd = genCmdCast<MEM_SET *>(*itor);

        auto resourceFormat = gmm->gmmResourceInfo->getResourceFormat();
        auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
        EXPECT_EQ(compressionFormat, blitCmd->getCompressionFormat());
    }
}

HWTEST2_F(BlitTests, givenLinearResourcesWhenAppendSliceOffsetsIsCalledThenAddressesAreOffsetted, IsXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto blitCmd = FamilyType::cmdInitXyBlockCopyBlt;
    blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR);
    blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_LINEAR);
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    BlitProperties properties{};
    properties.copySize = {0x10, 0x10, 0x1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcSlicePitch = 0x4000;
    properties.dstSlicePitch = 0x8000;
    properties.srcGpuAddress = mockAllocationSrc.getGpuAddress();
    properties.dstGpuAddress = mockAllocationDst.getGpuAddress();
    properties.clearColorAllocation = &mockClearColor;
    properties.bytesPerPixel = 1;
    uint32_t sliceIndex = 1;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);

    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, blitCmd, sliceIndex, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(blitCmd.getSourceBaseAddress(), ptrOffset(mockAllocationSrc.getGpuAddress(), srcSlicePitch));
    EXPECT_EQ(blitCmd.getDestinationBaseAddress(), ptrOffset(mockAllocationDst.getGpuAddress(), dstSlicePitch));
}

HWTEST2_F(BlitTests, givenTiledResourcesWhenAppendSliceOffsetsIsCalledThenIndexesAreSet, IsXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto blitCmd = FamilyType::cmdInitXyBlockCopyBlt;
    blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitProperties properties{};
    uint32_t sliceIndex = 1;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);

    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, blitCmd, sliceIndex, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(blitCmd.getDestinationArrayIndex(), sliceIndex + 1);
    EXPECT_EQ(blitCmd.getSourceArrayIndex(), sliceIndex + 1);
}

HWTEST2_F(BlitTests, givenCompressionInfoWhenAppendImageCommandsThenCorrectPropertiesAreSet, IsAtLeastXe2HpgCore) {
    auto verifyCompressionFormat = [](bool mediaCompressed, bool renderCompressed, uint32_t compressionFormat) {
        if (mediaCompressed || renderCompressed) {
            EXPECT_GT(compressionFormat, 0u);
        } else {
            EXPECT_EQ(compressionFormat, 0u);
        }
    };

    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x100;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);

    std::tuple<bool, bool> params[]{
        {false, false},
        {false, true},
        {true, false}};

    for (auto &[mediaCompressedSrc, renderCompressedSrc] : params) {
        auto gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
        auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
        resourceInfoSrc->getResourceFlags()->Info.MediaCompressed = mediaCompressedSrc;
        resourceInfoSrc->getResourceFlags()->Info.RenderCompressed = renderCompressedSrc;
        MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                 reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                 MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationSrc.setGmm(gmmSrc.get(), 0);
        properties.srcAllocation = &mockAllocationSrc;

        for (auto &[mediaCompressedDst, renderCompressedDst] : params) {
            auto gmmDst = std::make_unique<MockGmm>(pDevice->getGmmHelper());
            auto resourceInfoDst = static_cast<MockGmmResourceInfo *>(gmmDst->gmmResourceInfo.get());
            resourceInfoDst->getResourceFlags()->Info.MediaCompressed = mediaCompressedDst;
            resourceInfoDst->getResourceFlags()->Info.RenderCompressed = renderCompressedDst;
            MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                     reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                     MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
            mockAllocationDst.setGmm(gmmDst.get(), 0);
            properties.dstAllocation = &mockAllocationDst;
            auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
            BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(),
                                                                        srcSlicePitch, dstSlicePitch);

            verifyCompressionFormat(mediaCompressedSrc, renderCompressedSrc, bltCmd.getSourceCompressionFormat());
            verifyCompressionFormat(mediaCompressedDst, renderCompressedDst, bltCmd.getDestinationCompressionFormat());
        }
    }
}

HWTEST2_F(BlitTests, givenFastCopyBltCommandWhenSettingForbidenColorDepthThenExpectThrow, IsXe2HpgCore) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto forbidenColorDepth = XY_COLOR_BLT::COLOR_DEPTH::COLOR_DEPTH_96_BIT_COLOR_ONLY_SUPPORTED_FOR_LINEAR_CASE;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    EXPECT_THROW(blitCmd.setColorDepth(forbidenColorDepth), std::exception);
}

struct BlitTestsTestXe2HpgCore : BlitColorTests {};

template <typename FamilyType>
class GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXe2HpgCore : public GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> {
  public:
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXe2HpgCore(Device *device) : GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType>(device) {}
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

HWTEST2_P(BlitTestsTestXe2HpgCore, givenCommandStreamWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, IsXe2HpgCore) {
    auto patternSize = GetParam();
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXe2HpgCore<FamilyType> test(pDevice);
    test.testBodyImpl(patternSize, expecttedDepth);
}

HWTEST2_F(BlitTests, givenCopyCommandListWhenBytesPerPixelIsCalledForNonBlitCopySupportedPlatformThenExpectOne, IsAtLeastXe2HpgCore) {
    size_t copySize{}, srcSize{}, dstSize{};
    uint32_t srcOrigin{}, dstOrigin{};
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);
    EXPECT_EQ(bytesPerPixel, 1u);
}

INSTANTIATE_TEST_SUITE_P(size_t,
                         BlitTestsTestXe2HpgCore,
                         testing::Values(2,
                                         4,
                                         8,
                                         16));

HWTEST2_F(BlitTests, givenMemoryAndImageWhenDispatchCopyImageCallThenCommandAddedToStream, IsXe2HpgCore) {
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
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_BLOCK_COPY_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(BlitTests, givenSurfaceTypeAndSliceIndexWhenAppendBaseAddressOffsetIsCalledThenBaseAddressAndArrayIndexAreCorrectlySet, IsXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    MockGraphicsAllocation mockAllocationSrc(reinterpret_cast<void *>(0x1234), 0x1000, sizeof(uint32_t));
    MockGraphicsAllocation mockAllocationDst(reinterpret_cast<void *>(0x1234), 0x1000, sizeof(uint32_t));
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    mockAllocationSrc.setGmm(gmm.get(), 0u);
    mockAllocationDst.setGmm(gmm.get(), 0u);
    BlitProperties properties{};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcGpuAddress = mockAllocationSrc.getGpuAddress();
    properties.dstGpuAddress = mockAllocationDst.getGpuAddress();
    properties.srcRowPitch = 1;
    properties.dstRowPitch = 1;
    properties.srcSize.z = 129;
    properties.dstSize.z = 129;
    std::array<std::tuple<typename XY_BLOCK_COPY_BLT::SURFACE_TYPE, uint32_t, uint32_t, uint32_t>, 4> testParams =
        {{{XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 0u, 0u, 0u},
          {XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 64u, 64u, 64u},
          {XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 64u, 128u, 128u},
          {XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 4096u, 1u, 1u}}};
    for (const auto &isSource : {false, true}) {
        for (const auto &[surfaceType, qPitch, sliceIndex, anchorSliceIndex] : testParams) {
            uint32_t y1Top = 0;
            uint32_t yOffset = 0;
            uint32_t xOffset = 0;
            auto blitCmd = FamilyType::cmdInitXyBlockCopyBlt;
            blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE4);
            blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE4);
            blitCmd.setSourceSurfaceType(surfaceType);
            blitCmd.setDestinationSurfaceType(surfaceType);
            blitCmd.setSourceBaseAddress(properties.srcGpuAddress);
            blitCmd.setDestinationBaseAddress(properties.dstGpuAddress);
            blitCmd.setSourceY1CoordinateTop(y1Top);
            blitCmd.setDestinationY1CoordinateTop(y1Top);
            blitCmd.setSourceXOffset(xOffset);
            blitCmd.setDestinationXOffset(xOffset);
            blitCmd.setSourceYOffset(yOffset);
            blitCmd.setDestinationYOffset(yOffset);
            blitCmd.setSourceSurfaceQpitch(qPitch);
            blitCmd.setDestinationSurfaceQpitch(qPitch);
            blitCmd.setSourceSurfaceDepth(static_cast<uint32_t>(properties.srcSize.z));
            blitCmd.setDestinationSurfaceDepth(static_cast<uint32_t>(properties.dstSize.z));
            BlitCommandsHelper<FamilyType>::appendBaseAddressOffset(properties, blitCmd, isSource);
            auto gpuAddress = isSource ? properties.srcGpuAddress : properties.dstGpuAddress;
            if (isSource) {
                EXPECT_EQ(blitCmd.getSourceBaseAddress(), gpuAddress);
                EXPECT_EQ(blitCmd.getSourceSurfaceDepth(), 1u);
                EXPECT_EQ(blitCmd.getSourceArrayIndex(), 1u);
                EXPECT_EQ(blitCmd.getSourceXOffset(), xOffset);
                EXPECT_EQ(blitCmd.getSourceYOffset(), yOffset);
            } else {
                EXPECT_EQ(blitCmd.getDestinationBaseAddress(), gpuAddress);
                EXPECT_EQ(blitCmd.getDestinationSurfaceDepth(), 1u);
                EXPECT_EQ(blitCmd.getDestinationArrayIndex(), 1u);
                EXPECT_EQ(blitCmd.getDestinationXOffset(), xOffset);
                EXPECT_EQ(blitCmd.getDestinationYOffset(), yOffset);
            }
        }
    }
}
