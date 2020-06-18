/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

using BlitTests = Test<ClDeviceFixture>;

HWTEST2_F(BlitTests, givenOneBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 1;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenTwoBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 2;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenFourBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 4;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenEightBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 8;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenSixteenBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 16;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenIncorrectBytePerPixelWhenAppendColorDepthThenAbortIsThrown, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 48;
    EXPECT_THROW(BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd), std::exception);
}

HWTEST2_F(BlitTests, givenNotTiledSrcAndDestinationWhenAppendTilingTypeThenCorrectTilingIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendTilingType(GMM_NOT_TILED, GMM_NOT_TILED, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::SOURCE_TILING::SOURCE_TILING_LINEAR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::DESTINATION_TILING::DESTINATION_TILING_LINEAR);
}
HWTEST2_F(BlitTests, givenTiledSrcAndDestinationAppendTilingTypeThenCorrectTilingIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendTilingType(GMM_TILED_Y, GMM_TILED_Y, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::SOURCE_TILING::SOURCE_TILING_YMAJOR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::DESTINATION_TILING::DESTINATION_TILING_YMAJOR);
}

HWTEST2_F(BlitTests, givenTiledSrcAndDestinationWhenAppendImageCommandsThenCorrectTiledIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = true;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::SOURCE_TILING::SOURCE_TILING_YMAJOR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::DESTINATION_TILING::DESTINATION_TILING_YMAJOR);
}

HWTEST2_F(BlitTests, givenNotTiledSrcAndDestinationWhenAppendImageCommandsThenCorrectTiledIsSet, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = false;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::SOURCE_TILING::SOURCE_TILING_LINEAR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::DESTINATION_TILING::DESTINATION_TILING_LINEAR);
}

HWTEST2_F(BlitTests, givenSrcAndDestinationImagesWhenAppendSliceOffsetsThenAdressAreCorectOffseted, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = false;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    properties.srcSize.y = 0x10;
    properties.srcRowPitch = 0x10;
    auto srcSlicePitch = properties.srcSize.y * properties.srcRowPitch;

    properties.dstSize.y = 0x20;
    properties.dstRowPitch = 0x20;
    auto dstSlicePitch = properties.dstSize.y * properties.dstRowPitch;

    properties.srcOffset = {0x10, 0x10, 0x10};
    properties.dstOffset = {0x20, 0x20, 0x20};
    uint32_t index = 7;
    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, bltCmd, index);
    auto expectesSrcOffset = (index + properties.srcOffset.z) * srcSlicePitch;
    auto expectesDstOffset = (index + properties.dstOffset.z) * dstSlicePitch;

    EXPECT_EQ(bltCmd.getSourceBaseAddress(), ptrOffset(mockAllocationSrc.getGpuAddress(), expectesSrcOffset));
    EXPECT_EQ(bltCmd.getDestinationBaseAddress(), ptrOffset(mockAllocationDst.getGpuAddress(), expectesDstOffset));
}

HWTEST2_F(BlitTests, givenNotTiledSrcAndDestinationWhenAppendImageCommandsThenPitchIsValueFromProperties, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = false;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcRowPitch = 0x1000;
    properties.dstRowPitch = 0x4000;
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getDestinationPitch(), properties.dstRowPitch);
    EXPECT_EQ(bltCmd.getSourcePitch(), properties.srcRowPitch);
}
struct MyMockResourecInfo : public GmmResourceInfo {
    using GmmResourceInfo::resourceInfo;

    MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(clientContext, inputParams){};
    MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(clientContext, inputGmmResourceInfo){};
    size_t getRenderPitch() override {
        return pitch;
    }
    uint32_t getQPitch() override {
        return 0;
    }
    GMM_RESOURCE_FLAG *getResourceFlags() override {
        return &flags;
    }
    uint32_t getMipTailStartLodSurfaceState() override {
        return 0;
    }
    size_t pitch = 0;
    GMM_RESOURCE_FLAG flags = {};
};

HWTEST2_F(BlitTests, givenTiledSrcAndDestinationWhenAppendImageCommandsThenPitchIsValueFromGmm, IsGen12LP) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0x100;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = true;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getDestinationPitch(), gmm->gmmResourceInfo->getRenderPitch() / sizeof(uint32_t));
    EXPECT_EQ(bltCmd.getSourcePitch(), gmm->gmmResourceInfo->getRenderPitch() / sizeof(uint32_t));
}

HWTEST2_F(BlitTests, givenTiledSrcAndDestinationWhenGmmReturnsNotAlignedPitchThenValuesInCommandAreAligned, IsGen12LP) {
    constexpr uint32_t TILED_Y_PITCH_ALIGNMENT = 128;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>();
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0xFC;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = true;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    auto expectedPitch = alignUp<uint32_t>(static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch()), TILED_Y_PITCH_ALIGNMENT);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getDestinationPitch(), expectedPitch / sizeof(uint32_t));
    EXPECT_EQ(bltCmd.getSourcePitch(), expectedPitch / sizeof(uint32_t));
}

HWTEST2_F(BlitTests, givenDstTiledImageAndNotTiledSourceWhenAppendBlitCommandsForImagesThenPitchIsValueInDwords, IsGen12LP) {
    constexpr uint32_t TILED_Y_PITCH_ALIGNMENT = 128;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmmSrc = std::make_unique<MockGmm>();
    auto gmmDst = std::make_unique<MockGmm>();
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfoSrc = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    auto myResourecInfoDst = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfoSrc->pitch = 0x100;
    myResourecInfoDst->pitch = 0x200;
    gmmSrc->gmmResourceInfo.reset(myResourecInfoSrc.release());
    gmmDst->gmmResourceInfo.reset(myResourecInfoDst.release());
    auto flags = gmmDst->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = true;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmDst.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    auto expectedPitch = alignUp<uint32_t>(static_cast<uint32_t>(gmmDst->gmmResourceInfo->getRenderPitch()), TILED_Y_PITCH_ALIGNMENT);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getDestinationPitch(), expectedPitch / sizeof(uint32_t));
    EXPECT_EQ(bltCmd.getSourcePitch(), properties.srcRowPitch);
}

HWTEST2_F(BlitTests, givenSrcTiledImageAndNotTiledDstWhenAppendBlitCommandsForImagesThenPitchIsValueInDwords, IsGen12LP) {
    constexpr uint32_t TILED_Y_PITCH_ALIGNMENT = 128;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmmSrc = std::make_unique<MockGmm>();
    auto gmmDst = std::make_unique<MockGmm>();
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfoSrc = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    auto myResourecInfoDst = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfoSrc->pitch = 0x100;
    myResourecInfoDst->pitch = 0x200;
    gmmSrc->gmmResourceInfo.reset(myResourecInfoSrc.release());
    gmmDst->gmmResourceInfo.reset(myResourecInfoDst.release());
    auto flags = gmmSrc->gmmResourceInfo->getResourceFlags();
    flags->Info.TiledY = true;
    MockGraphicsAllocation mockAllocationSrc(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    MockGraphicsAllocation mockAllocationDst(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages);
    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmDst.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    auto expectedPitch = alignUp<uint32_t>(static_cast<uint32_t>(gmmSrc->gmmResourceInfo->getRenderPitch()), TILED_Y_PITCH_ALIGNMENT);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourcePitch(), expectedPitch / sizeof(uint32_t));
    EXPECT_EQ(bltCmd.getDestinationPitch(), properties.dstRowPitch);
}