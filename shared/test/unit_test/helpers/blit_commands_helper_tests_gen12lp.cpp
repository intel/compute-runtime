/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "gtest/gtest.h"

#include <cstring>

using namespace NEO;

using BlitTests = Test<DeviceFixture>;

HWTEST2_F(BlitTests, givenOneBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 1;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenTwoBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 2;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenFourBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 4;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenEightBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 8;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenSixteenBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsGen12LP) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 16;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenIncorrectBytePerPixelWhenAppendColorDepthThenAbortIsThrown, IsGen12LP) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 48;
    EXPECT_THROW(BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd), std::exception);
}

HWTEST2_F(BlitTests, givenSrcAndDestinationImagesWhenAppendSliceOffsetsThenAddressAreCorectOffset, IsGen12LP) {
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcGpuAddress = mockAllocationSrc.getGpuAddress();
    properties.dstGpuAddress = mockAllocationDst.getGpuAddress();

    properties.srcSize.y = 0x10;
    properties.srcRowPitch = 0x10;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSize.y * properties.srcRowPitch);

    properties.dstSize.y = 0x20;
    properties.dstRowPitch = 0x20;
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSize.y * properties.dstRowPitch);

    properties.srcOffset = {0x10, 0x10, 0x10};
    properties.dstOffset = {0x20, 0x20, 0x20};
    uint32_t index = 7;

    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, bltCmd, index, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    auto expectesSrcOffset = (index + properties.srcOffset.z) * srcSlicePitch;
    auto expectesDstOffset = (index + properties.dstOffset.z) * dstSlicePitch;

    EXPECT_EQ(bltCmd.getSourceBaseAddress(), ptrOffset(mockAllocationSrc.getGpuAddress(), expectesSrcOffset));
    EXPECT_EQ(bltCmd.getDestinationBaseAddress(), ptrOffset(mockAllocationDst.getGpuAddress(), expectesDstOffset));
}

HWTEST2_F(BlitTests, givenInputAndDefaultSlicePitchWhenAppendBlitCommandsForImagesThenSlicePitchesAreCorrect, IsGen12LP) {

    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcSize = {10, 10, 1};
    properties.dstSize = {8, 12, 1};
    properties.srcRowPitch = 0x10;
    properties.dstRowPitch = 0x40;

    {
        uint32_t inputSlicePitch = 0;
        auto srcSlicePitch = inputSlicePitch;
        auto dstSlicePitch = inputSlicePitch;
        BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

        EXPECT_EQ(srcSlicePitch, static_cast<uint32_t>(properties.srcRowPitch * properties.srcSize.y));
        EXPECT_EQ(dstSlicePitch, static_cast<uint32_t>(properties.dstRowPitch * properties.dstSize.y));
    }

    {
        uint32_t inputSlicePitch = 0x40000;
        auto srcSlicePitch = inputSlicePitch;
        auto dstSlicePitch = inputSlicePitch;
        BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

        EXPECT_EQ(srcSlicePitch, inputSlicePitch);
        EXPECT_EQ(dstSlicePitch, inputSlicePitch);
    }
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
    uint32_t getMipTailStartLODSurfaceState() override {
        return 0;
    }
    size_t pitch = 0;
    GMM_RESOURCE_FLAG flags = {};
};

HWTEST2_F(BlitTests, givenTiledSrcAndDestinationImagesWhenAppendImageCommandsThenPitchIsValueFromGmm, IsGen12LP) {
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0x100;
    myResourecInfo->flags.Info.TiledY = 1;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcRowPitch = 0x1000;
    properties.dstRowPitch = 0x4000;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationPitch(), gmm->gmmResourceInfo->getRenderPitch());
    EXPECT_EQ(bltCmd.getSourcePitch(), gmm->gmmResourceInfo->getRenderPitch());
    EXPECT_NE(bltCmd.getDestinationPitch(), static_cast<uint32_t>(properties.dstRowPitch));
    EXPECT_NE(bltCmd.getSourcePitch(), static_cast<uint32_t>(properties.srcRowPitch));
}

HWTEST2_F(BlitTests, givenLinearSrcAndDestinationImagesWhenAppendImageCommandsThenPitchIsValueFromProperties, IsGen12LP) {
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0x100;
    myResourecInfo->flags.Info.Linear = 1;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcRowPitch = 0x1000;
    properties.dstRowPitch = 0x4000;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_NE(bltCmd.getDestinationPitch(), gmm->gmmResourceInfo->getRenderPitch());
    EXPECT_NE(bltCmd.getSourcePitch(), gmm->gmmResourceInfo->getRenderPitch());
    EXPECT_EQ(bltCmd.getDestinationPitch(), static_cast<uint32_t>(properties.dstRowPitch));
    EXPECT_EQ(bltCmd.getSourcePitch(), static_cast<uint32_t>(properties.srcRowPitch));
}

HWTEST2_F(BlitTests, givenBlitCommandWhenAppendClearColorCalledThenNothingHappens, IsGen12LP) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    auto expectedBlitCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendClearColor(properties, bltCmd);
    EXPECT_EQ(0, std::memcmp(&expectedBlitCmd, &bltCmd, sizeof(bltCmd)));
}

HWTEST2_F(BlitTests, givenGen12LpPlatformWhenPreBlitCommandWARequiredThenReturnsTrue, IsGen12LP) {
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::preBlitCommandWARequired());
}

HWTEST2_F(BlitTests, givenGen12LpPlatformWhenEstimatePreBlitCommandSizeThenSizeOfFlushIsReturned, IsGen12LP) {
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    EXPECT_EQ(EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs), BlitCommandsHelper<FamilyType>::estimatePreBlitCommandSize());
}

HWTEST2_F(BlitTests, givenGen12LpPlatformWhenDispatchPreBlitCommandThenMiFlushDwIsProgramed, IsGen12LP) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto miFlushBuffer = std::make_unique<MI_FLUSH_DW>();
    EncodeDummyBlitWaArgs waArgs{true, &(pDevice->getRootDeviceEnvironmentRef())};
    LinearStream linearStream(miFlushBuffer.get(), EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs));

    BlitCommandsHelper<FamilyType>::dispatchPreBlitCommand(linearStream, *waArgs.rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(linearStream);
    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
}
