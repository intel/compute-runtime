/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(BlitCommandsHelperTest, GivenBufferParamsWhenConstructingPropertiesForReadWriteThenPropertiesCreatedCorrectly) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint64_t srcGpuAddr = 0x12345;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));

    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 2, 2};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    UltDeviceFactory deviceFactory{1, 0};

    auto csr = deviceFactory.rootDevices[0]->getDefaultEngine().commandStreamReceiver;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                               *csr, srcAlloc.get(), nullptr,
                                                                               dst,
                                                                               srcGpuAddr,
                                                                               0, dstOffsets, srcOffsets, copySize, dstRowPitch, dstSlicePitch, srcRowPitch, srcSlicePitch);

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::bufferToHostPtr);
    EXPECT_NE(blitProperties.dstAllocation, nullptr);
    EXPECT_EQ(blitProperties.dstAllocation->getUnderlyingBufferSize(), copySize.x * copySize.y * copySize.z);
    EXPECT_EQ(blitProperties.srcAllocation, srcAlloc.get());
    EXPECT_EQ(blitProperties.clearColorAllocation, csr->getClearColorAllocation());
    EXPECT_EQ(blitProperties.dstGpuAddress, blitProperties.dstAllocation->getGpuAddress());
    EXPECT_EQ(blitProperties.srcGpuAddress, srcGpuAddr);
    EXPECT_EQ(blitProperties.copySize, copySize);
    EXPECT_EQ(blitProperties.dstOffset, dstOffsets);
    EXPECT_EQ(blitProperties.srcOffset, srcOffsets);
    EXPECT_EQ(blitProperties.dstRowPitch, dstRowPitch);
    EXPECT_EQ(blitProperties.dstSlicePitch, dstSlicePitch);
    EXPECT_EQ(blitProperties.srcRowPitch, srcRowPitch);
    EXPECT_EQ(blitProperties.srcSlicePitch, srcSlicePitch);

    EXPECT_EQ(1u, blitProperties.dstAllocation->hostPtrTaskCountAssignment.load());
    blitProperties.dstAllocation->hostPtrTaskCountAssignment--;
}

TEST(BlitCommandsHelperTest, GivenSourceGraphicAllocationAndDestinationIsSystemAllocatedConstructPropertiesForCopyCreatedCorrectly) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    GraphicsAllocation *dstAlloc = nullptr;
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));

    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{0, 0, 0};
    Vec3<size_t> copySize{2, 0, 0};

    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc, dstGpuAddr, srcAlloc.get(), srcGpuAddr,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::bufferToBuffer);
    EXPECT_EQ(blitProperties.dstAllocation, nullptr);
    EXPECT_EQ(blitProperties.srcAllocation, srcAlloc.get());
    EXPECT_EQ(blitProperties.clearColorAllocation, clearColorAllocation.get());
    EXPECT_EQ(blitProperties.dstGpuAddress, dstGpuAddr);
    EXPECT_EQ(blitProperties.srcGpuAddress, srcGpuAddr);
    EXPECT_EQ(blitProperties.dstOffset, dstOffsets);
    EXPECT_EQ(blitProperties.srcOffset, srcOffsets);
    EXPECT_EQ(blitProperties.dstRowPitch, dstRowPitch);
    EXPECT_EQ(blitProperties.dstSlicePitch, dstSlicePitch);
    EXPECT_EQ(blitProperties.srcRowPitch, srcRowPitch);
    EXPECT_EQ(blitProperties.srcSlicePitch, srcSlicePitch);
    EXPECT_TRUE(blitProperties.isSystemMemoryPoolUsed);
}

TEST(BlitCommandsHelperTest, GivenDestinationGraphicAllocationAndSrcIsSystemAllocatedConstructPropertiesForCopyCreatedCorrectly) {
    uint32_t dst[] = {1, 2, 3, 4};
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    GraphicsAllocation *srcAlloc = nullptr;
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));

    Vec3<size_t> srcOffsets{0, 0, 0};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 2, 2};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc.get(), dstGpuAddr, srcAlloc, srcGpuAddr,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::bufferToBuffer);
    EXPECT_EQ(blitProperties.dstAllocation, dstAlloc.get());
    EXPECT_EQ(blitProperties.srcAllocation, nullptr);
    EXPECT_EQ(blitProperties.clearColorAllocation, clearColorAllocation.get());
    EXPECT_EQ(blitProperties.dstGpuAddress, dstGpuAddr);
    EXPECT_EQ(blitProperties.srcGpuAddress, srcGpuAddr);
    EXPECT_EQ(blitProperties.copySize, copySize);
    EXPECT_EQ(blitProperties.dstOffset, dstOffsets);
    EXPECT_EQ(blitProperties.srcOffset, srcOffsets);
    EXPECT_EQ(blitProperties.dstRowPitch, dstRowPitch);
    EXPECT_EQ(blitProperties.dstSlicePitch, dstSlicePitch);
    EXPECT_EQ(blitProperties.srcRowPitch, srcRowPitch);
    EXPECT_EQ(blitProperties.srcSlicePitch, srcSlicePitch);
    EXPECT_TRUE(blitProperties.isSystemMemoryPoolUsed);
}

TEST(BlitCommandsHelperTest, GivenBufferParamsWhenConstructingPropertiesForBufferRegionsThenPropertiesCreatedCorrectly) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));

    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 2, 2};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc.get(), 0, srcAlloc.get(), 0,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::bufferToBuffer);
    EXPECT_EQ(blitProperties.dstAllocation, dstAlloc.get());
    EXPECT_EQ(blitProperties.srcAllocation, srcAlloc.get());
    EXPECT_EQ(blitProperties.clearColorAllocation, clearColorAllocation.get());
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
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));
    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 0, 0};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc.get(), 0, srcAlloc.get(), 0,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());
    Vec3<size_t> expectedSize{copySize.x, 1, 1};
    EXPECT_EQ(blitProperties.copySize, expectedSize);
}

using BlitTests = Test<DeviceFixture>;

HWTEST_F(BlitTests, givenDebugVariablesWhenGettingMaxBlitSizeThenHonorUseProvidedValues) {
    DebugManagerStateRestore restore{};

    ASSERT_EQ(BlitterConstants::maxBlitWidth, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironment()));
    ASSERT_EQ(BlitterConstants::maxBlitHeight, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), false));

    debugManager.flags.LimitBlitterMaxWidth.set(50);
    EXPECT_EQ(50u, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironment()));

    debugManager.flags.LimitBlitterMaxHeight.set(60);
    EXPECT_EQ(60u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironment(), false));
}

HWTEST_F(BlitTests, givenDebugVariablesWhenGettingMaxBlitSetSizeThenHonorUseProvidedValues) {
    DebugManagerStateRestore restore{};

    ASSERT_EQ(BlitterConstants::maxBlitSetWidth, BlitCommandsHelper<FamilyType>::getMaxBlitSetWidth(pDevice->getRootDeviceEnvironment()));
    ASSERT_EQ(BlitterConstants::maxBlitSetHeight, BlitCommandsHelper<FamilyType>::getMaxBlitSetHeight(pDevice->getRootDeviceEnvironment()));

    debugManager.flags.LimitBlitterMaxSetWidth.set(50);
    EXPECT_EQ(50u, BlitCommandsHelper<FamilyType>::getMaxBlitSetWidth(pDevice->getRootDeviceEnvironment()));

    debugManager.flags.LimitBlitterMaxSetHeight.set(60);
    EXPECT_EQ(60u, BlitCommandsHelper<FamilyType>::getMaxBlitSetHeight(pDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BlitTests, givenDebugVariableWhenEstimatingPostBlitsCommandSizeThenReturnCorrectResult) {
    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};
    DebugManagerStateRestore restore{};

    size_t arbCheckSize = EncodeMiArbCheck<FamilyType>::getCommandSize();
    size_t expectedDefaultSize = arbCheckSize;

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        expectedDefaultSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    EXPECT_EQ(expectedDefaultSize, BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());

    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::miArbCheck);
    EXPECT_EQ(arbCheckSize, BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());

    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::miFlush);
    EXPECT_EQ(EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs), BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());

    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::none);
    EXPECT_EQ(0u, BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize());
}

HWTEST_F(BlitTests, givenDebugVariableWhenDispatchingPostBlitsCommandThenUseCorrectCommands) {
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    DebugManagerStateRestore restore{};
    uint32_t streamBuffer[100] = {};
    LinearStream linearStream{streamBuffer, sizeof(streamBuffer)};
    GenCmdList commands{};
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();
    EncodeDummyBlitWaArgs waArgs{false, &rootDeviceEnvironment};
    size_t expectedDefaultSize = EncodeMiArbCheck<FamilyType>::getCommandSize() + BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs);

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        expectedDefaultSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    // -1: default
    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

    EXPECT_EQ(expectedDefaultSize, linearStream.getUsed());
    CmdParse<FamilyType>::parseCommandBuffer(commands, linearStream.getCpuBase(), linearStream.getUsed());

    auto iterator = commands.begin();
    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        iterator = find<MI_FLUSH_DW *>(commands.begin(), commands.end());
        EXPECT_NE(commands.end(), iterator);
        if (EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) == 2 * sizeof(MI_FLUSH_DW)) {
            iterator = find<MI_FLUSH_DW *>(++iterator, commands.end());
            EXPECT_NE(commands.end(), iterator);
        }
    }

    auto arbCheck = find<MI_ARB_CHECK *>(iterator, commands.end());
    EXPECT_NE(commands.end(), arbCheck);

    // 0: MI_ARB_CHECK
    memset(streamBuffer, 0, sizeof(streamBuffer));
    linearStream.replaceBuffer(streamBuffer, sizeof(streamBuffer));
    commands.clear();
    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::miArbCheck);
    waArgs.isWaRequired = true;
    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

    CmdParse<FamilyType>::parseCommandBuffer(commands, linearStream.getCpuBase(), linearStream.getUsed());
    arbCheck = find<MI_ARB_CHECK *>(commands.begin(), commands.end());
    EXPECT_NE(commands.end(), arbCheck);

    // 1: MI_FLUSH_DW
    memset(streamBuffer, 0, sizeof(streamBuffer));
    linearStream.replaceBuffer(streamBuffer, sizeof(streamBuffer));
    commands.clear();
    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::miFlush);
    waArgs.isWaRequired = true;
    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

    CmdParse<FamilyType>::parseCommandBuffer(commands, linearStream.getCpuBase(), linearStream.getUsed());
    auto miFlush = find<MI_FLUSH_DW *>(commands.begin(), commands.end());
    EXPECT_NE(commands.end(), miFlush);

    // 2: Nothing
    memset(streamBuffer, 0, sizeof(streamBuffer));
    linearStream.replaceBuffer(streamBuffer, sizeof(streamBuffer));
    commands.clear();
    debugManager.flags.PostBlitCommand.set(BlitterConstants::PostBlitMode::none);
    waArgs.isWaRequired = true;
    BlitCommandsHelper<FamilyType>::dispatchPostBlitCommand(linearStream, rootDeviceEnvironment);

    EXPECT_EQ(0u, linearStream.getUsed());
}

HWTEST_F(BlitTests, givenMemoryWhenFillPatternWithBlitThenCommandIsProgrammed) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), pattern, sizeof(uint32_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(BlitTests, givenSystemAllocatedMemoryWhenConstructPropertiesForMemoryFillIsCalledThenVerifyPropertiesAreCreatedSuccessfully) {
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint64_t dstPtr = 0x1234;
    size_t size = 0x1000;

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(nullptr, dstPtr, size, pattern, sizeof(uint32_t), 0);

    EXPECT_EQ(blitProperties.dstAllocation, nullptr);
    EXPECT_EQ(blitProperties.dstGpuAddress, dstPtr);
    EXPECT_EQ(blitProperties.isSystemMemoryPoolUsed, true);
}

HWTEST_F(BlitTests, givenUnalignedPatternSizeWhenDispatchingBlitFillThenSetCorrectColorDepth) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    size_t copySize = 0x800'0000;

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, copySize, pattern, 3, 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, stream.getCpuBase(), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
    EXPECT_EQ(cmd->getColorDepth(), XY_COLOR_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
    EXPECT_EQ(cmd->getDestinationX2CoordinateRight(), 0x4000u);
    EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), 0x200u);
    EXPECT_EQ(cmd->getDestinationPitch(), 0x4000u * 16u);
}

HWTEST_F(BlitTests, givenMemorySizeBiggerThanMaxWidthButLessThanTwiceMaxWidthWhenFillPatternWithBlitThenHeightIsOne) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitWidth) - 1,
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), pattern, sizeof(uint32_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), 1u);
    }
}

HWTEST_F(BlitTests, givenMemoryPointerOffsetVerifyCorrectDestinationBaseAddress) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    uint32_t pattern[4] = {5, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), pattern, sizeof(uint32_t), 0x234);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getDestinationBaseAddress(), static_cast<uint64_t>(0x1234));
    }
}

HWTEST_F(BlitTests, givenMemorySizeTwiceBiggerThanMaxWidthWhenFillPatternWithBlitThenHeightIsTwo) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;

    HardwareInfo *hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.blitterOperationsSupported = true;

    REQUIRE_BLITTER_OR_SKIP(pDevice->getRootDeviceEnvironment());

    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitWidth * sizeof(uint32_t)),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), pattern, sizeof(uint32_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), 2u);
        EXPECT_EQ(cmd->getDestinationPitch(), BlitterConstants::maxBlitWidth * sizeof(uint32_t));
    }
}

HWTEST_F(BlitTests, givenMemorySizeIsLessThanTwicenMaxWidthWhenFillPatternWithBlitThenHeightIsOne) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;

    HardwareInfo *hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.blitterOperationsSupported = true;

    REQUIRE_BLITTER_OR_SKIP(pDevice->getRootDeviceEnvironment());

    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, ((BlitterConstants::maxBlitWidth + 1) * sizeof(uint32_t)),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), pattern, sizeof(uint32_t), 0);

    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getDestinationY2CoordinateBottom(), 1u);
    }
}

HWTEST_F(BlitTests, givenXyCopyBltCommandWhenAppendBlitCommandsMemCopyIsCalledThenNothingChanged) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_COPY_BLT)), 0);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenAppendBlitCommandsMemCopyIsCalledThenNothingChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), 0);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenAppendBlitCommandsMemCopyIsCalledWithDebugFlagSetThenCompFormatChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    DebugManagerStateRestore restore{};
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(1);
    BlitProperties properties = {};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), 1u);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenDstGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 2;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenDstGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    uint32_t statelessCompressionFormat = 15u;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(statelessCompressionFormat);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), statelessCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenDstGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 2;
    debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.set(static_cast<int32_t>(newCompressionFormat));
    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(1);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenSrcGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 2;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenSrcGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t statelessCompressionFormat = 15u;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(statelessCompressionFormat);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), statelessCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe2HpgCoreWhenSrcGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe2HpgCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 2;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenAppendBlitCommandsMemCopyIsCalledThenNothingChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), 0);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenDstGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    uint32_t newCompressionFormat = 2;

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());

    if (pDevice->getProductHelper().isCompressionFormatFromGmmRequired()) {
        auto resourceFormat = mockAllocation.getDefaultGmm()->gmmResourceInfo->getResourceFormat();
        newCompressionFormat = pDevice->getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    }
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenDstGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    uint32_t statelessCompressionFormat = 15u;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(statelessCompressionFormat);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), statelessCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenDstGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenSrcGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenSrcGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t statelessCompressionFormat = 15u;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(statelessCompressionFormat);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), statelessCompressionFormat);
}

HWTEST2_F(BlitTests, givenXe3CoreWhenSrcGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3Core) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), newCompressionFormat);
}

HWTEST_F(BlitTests, givenXyBlockCopyBltCommandAndSliceIndex0WhenAppendBaseAddressOffsetIsCalledThenNothingChanged) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties{};

    NEO::BlitCommandsHelper<FamilyType>::appendBaseAddressOffset(properties, bltCmd, false);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);

    NEO::BlitCommandsHelper<FamilyType>::appendBaseAddressOffset(properties, bltCmd, true);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_BLOCK_COPY_BLT)), 0);
}

using BlitColor = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE_LP>;

HWTEST2_F(BlitTests, givenMemoryWhenFillPatternSizeIs4BytesThen32BitMaskISSetCorrectly, BlitColor) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, &pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), pDevice->getRootDeviceEnvironmentRef());
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
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
    test.testBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualTwoWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitColor) {
    size_t patternSize = 2;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.testBodyImpl(patternSize, expecttedDepth);
}
HWTEST2_F(BlitColorTests, givenCommandStreamAndPaternSizeEqualFourWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, BlitColor) {
    size_t patternSize = 4;
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> test(pDevice);
    test.testBodyImpl(patternSize, expecttedDepth);
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
    test.testBodyImpl(patternSize, expecttedDepth);
}

INSTANTIATE_TEST_SUITE_P(size_t,
                         BlitFastColorTest,
                         testing::Values(1,
                                         2,
                                         4,
                                         8,
                                         16));

HWTEST2_F(BlitTests, givenMemoryAndImageWhenDispatchCopyImageCallThenCommandAddedToStream, BlitPlatforms) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    MockGraphicsAllocation srcAlloc;
    MockGraphicsAllocation dstAlloc;
    MockGraphicsAllocation clearColorAllocation;

    Vec3<size_t> dstOffsets = {0, 0, 0};
    Vec3<size_t> srcOffsets = {0, 0, 0};

    Vec3<size_t> copySize = {0x100, 0x40, 0x1};
    Vec3<size_t> srcSize = {0x100, 0x40, 0x1};
    Vec3<size_t> dstSize = {0x100, 0x40, 0x1};

    size_t srcRowPitch = srcSize.x;
    size_t srcSlicePitch = srcSize.y;
    size_t dstRowPitch = dstSize.x;
    size_t dstSlicePitch = dstSize.y;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(&dstAlloc, 0, &srcAlloc, 0,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, &clearColorAllocation);

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

HWTEST2_F(BlitTests, whenPrintImageBlitBlockCopyCommandIsCalledThenCmdDetailsAreNotPrintedToStdOutput, BlitPlatforms) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;

    StreamCapture capture;
    capture.captureStdout();
    NEO::BlitCommandsHelper<FamilyType>::printImageBlitBlockCopyCommand(bltCmd, 0);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput("");
    EXPECT_EQ(expectedOutput, output);
}

using BlitTestsParams = BlitColorTests;

HWTEST2_P(BlitTestsParams, givenCopySizeAlignedWithin1and16BytesWhenGettingBytesPerPixelThenCorrectPixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = 33;
    auto alignment = GetParam();
    copySize = alignUp(copySize, alignment);
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcOrigin = dstOrigin = 0;
    srcSize = dstSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, alignment);
}

HWTEST2_P(BlitTestsParams, givenSrcSizeAlignedWithin1and16BytesWhenGettingBytesPerPixelThenCorrectPixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto alignment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcSize = 33;
    srcSize = alignUp(srcSize, alignment);
    srcOrigin = dstOrigin = dstSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, alignment);
}

HWTEST2_P(BlitTestsParams, givenSrcSizeNotAlignedWithin1and16BytesWhenGettingBytesPerPixelThen1BytePixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcSize = 33;
    srcOrigin = dstOrigin = dstSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, 1u);
}

HWTEST2_P(BlitTestsParams, givenDstSizeAlignedWithin1and16BytesWhenGettingBytesPerPixelThenCorrectPixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto alignment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstSize = 33;
    dstSize = alignUp(dstSize, alignment);
    srcOrigin = dstOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, alignment);
}

HWTEST2_P(BlitTestsParams, givenDstSizeNotAlignedWithin1and16BytesWhenGettingBytesPerPixelThen1BytePixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstSize = 33;
    srcOrigin = dstOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, 1u);
}

HWTEST2_P(BlitTestsParams, givenSrcOriginAlignedWithin1and16BytesWhenGettingBytesPerPixelThenCorrectPixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto alignment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    srcOrigin = 33;
    srcOrigin = alignUp(srcOrigin, alignment);
    dstSize = dstOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, alignment);
}

HWTEST2_P(BlitTestsParams, givenDstOriginAlignedWithin1and16BytesWhenGettingBytesPerPixelThenCorrectPixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    auto alignment = GetParam();
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstOrigin = 33;
    dstOrigin = alignUp(dstOrigin, alignment);
    dstSize = srcOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, alignment);
}

HWTEST2_P(BlitTestsParams, givenDstOriginNotAlignedWithin1and16BytesWhenGettingBytesPerPixelThen1BytePixelSizeIsReturned, BlitPlatforms) {
    size_t copySize = BlitterConstants::maxBytesPerPixel;
    uint32_t srcOrigin, dstOrigin, srcSize, dstSize;
    dstOrigin = 33;
    dstSize = srcOrigin = srcSize = static_cast<uint32_t>(BlitterConstants::maxBytesPerPixel);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<FamilyType>::getAvailableBytesPerPixel(copySize, srcOrigin, dstOrigin, srcSize, dstSize);

    EXPECT_EQ(bytesPerPixel, 1u);
}

INSTANTIATE_TEST_SUITE_P(size_t,
                         BlitTestsParams,
                         testing::Values(1,
                                         2,
                                         4,
                                         8,
                                         16));

using WithoutGen12Lp = IsNotGfxCore<IGFX_GEN12LP_CORE>;

HWTEST2_F(BlitTests, givenPlatformWhenCallingPreBlitCommandWARequiredThenReturnsFalse, WithoutGen12Lp) {
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::preBlitCommandWARequired());
}

HWTEST2_F(BlitTests, givenPlatformWhenCallingEstimatePreBlitCommandSizeThenZeroIsReturned, WithoutGen12Lp) {
    EXPECT_EQ(0u, BlitCommandsHelper<FamilyType>::estimatePreBlitCommandSize());
}

HWTEST2_F(BlitTests, givenPlatformWhenCallingDispatchPreBlitCommandThenNoneMiFlushDwIsProgramed, WithoutGen12Lp) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto miFlushBuffer = std::make_unique<MI_FLUSH_DW>();
    EncodeDummyBlitWaArgs waArgs{true, &pDevice->getRootDeviceEnvironmentRef()};
    LinearStream linearStream(miFlushBuffer.get(), EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs));

    BlitCommandsHelper<FamilyType>::dispatchPreBlitCommand(linearStream, *waArgs.rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(linearStream);
    auto cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(hwParser.cmdList.end(), cmdIterator);
}

HWTEST_F(BlitTests, givenBlitPropertiesContainerWithNullSrcOrDstAllocationWhenEstimatingCommandsSizeThenCalculateForAllAttachedProperties) {
    const auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    const uint32_t numberOfBlts = 3;
    const size_t bltSize = (3 * max2DBlitSize);
    const uint32_t numberOfBlitOperations = 4;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    EncodeDummyBlitWaArgs waArgs{false, &(pDevice->getRootDeviceEnvironmentRef())};

    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + EncodeMiArbCheck<FamilyType>::getCommandSize();
    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }
    auto expectedBlitInstructionsSize = cmdsSizePerBlit * numberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedBlitInstructionsSize += EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs);
    }

    waArgs.isWaRequired = true;
    auto baseSize = EncodeMiFlushDW<FamilyType>::getCommandSizeWithWa(waArgs) + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    auto syncSize = 2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment());

    // Test both scenarios: dstAllocation=nullptr, srcAllocation=nullptr
    for (auto isSourceAllocationNullptr : ::testing::Bool()) {
        size_t expectedAlignedSize = baseSize + syncSize;

        MockGraphicsAllocation bufferMockAllocation(0, 1u, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
        MockGraphicsAllocation hostMockAllocation(0, 1u, AllocationType::externalHostPtr, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

        BlitPropertiesContainer blitPropertiesContainer;
        for (uint32_t i = 0; i < numberOfBlitOperations; i++) {
            BlitProperties blitProperties;
            blitProperties.blitDirection = BlitterConstants::BlitDirection::bufferToHostPtr;
            blitProperties.auxTranslationDirection = AuxTranslationDirection::none;
            blitProperties.copySize = {bltSize, 1, 1};

            if (isSourceAllocationNullptr) {
                blitProperties.dstAllocation = &hostMockAllocation;
                blitProperties.srcAllocation = nullptr;
            } else {
                blitProperties.dstAllocation = nullptr;
                blitProperties.srcAllocation = &bufferMockAllocation;
            }

            blitPropertiesContainer.push_back(blitProperties);

            expectedAlignedSize += expectedBlitInstructionsSize;

            bool deviceToHostPostSyncFenceRequired = rootDeviceEnvironment.getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                     ((blitProperties.srcAllocation && blitProperties.srcAllocation->isAllocatedInLocalMemoryPool()) &&
                                                      (blitProperties.dstAllocation && !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool()));
            if (deviceToHostPostSyncFenceRequired) {
                expectedAlignedSize += MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
            }
        }

        expectedAlignedSize = alignUp(expectedAlignedSize, MemoryConstants::cacheLineSize);

        auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
            blitPropertiesContainer, false, false, false, false, pDevice->getRootDeviceEnvironment());

        EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    }
}

HWTEST2_F(BlitTests, givenPlatformWithBlitSyncPropertiesWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitCommandForBufferPerRowThenTheResultsAreTheSame, MatchAny) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    size_t maxBlitHeight = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironmentRef(), false));
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));

    Vec3<size_t> srcOffsets{1, 0, 0};
    Vec3<size_t> dstOffsets{1, 0, 0};
    Vec3<size_t> copySize{(maxBlitWidth * maxBlitHeight) + 1, 2, 2};

    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc.get(), 0, srcAlloc.get(), 0,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());
    ASSERT_FALSE(blitProperties.isSystemMemoryPoolUsed);
    uint32_t streamBuffer[400] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferPerRow(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[400] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferPerRow(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[400] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferPerRow(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
}

HWTEST2_F(BlitTests, givenPlatformWithBlitSyncPropertiesWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitCommandForBufferRegionThenTheResultsAreTheSame, MatchAny) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint32_t clear[] = {5, 6, 7, 8};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    uint64_t clearGpuAddr = 0x5678;
    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    size_t maxBlitHeight = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironmentRef(), false));
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    std::unique_ptr<GraphicsAllocation> clearColorAllocation(new MockGraphicsAllocation(clear, clearGpuAddr, sizeof(clear)));

    Vec3<size_t> srcOffsets{1, 0, 0};
    Vec3<size_t> dstOffsets{1, 0, 0};
    Vec3<size_t> copySize{(maxBlitWidth + 1), (maxBlitHeight + 1), 3};

    size_t srcRowPitch = 0;
    size_t srcSlicePitch = 0;

    size_t dstRowPitch = 0;
    size_t dstSlicePitch = 0;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc.get(), 0, srcAlloc.get(), 0,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation.get());
    ASSERT_FALSE(blitProperties.isSystemMemoryPoolUsed);

    uint32_t streamBuffer[400] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferRegion(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[400] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferRegion(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[400] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForBufferRegion(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
}

HWTEST2_F(BlitTests, givenPlatformWithBlitSyncPropertiesWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitCommandForImageRegionThenTheResultsAreTheSame, MatchAny) {
    MockGraphicsAllocation srcAlloc;
    MockGraphicsAllocation dstAlloc;
    MockGraphicsAllocation clearColorAllocation;

    Vec3<size_t> dstOffsets = {0, 0, 0};
    Vec3<size_t> srcOffsets = {0, 0, 0};

    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    size_t maxBlitHeight = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironmentRef(), false));
    size_t copySizeX = maxBlitWidth - 1;
    size_t copySizeY = maxBlitHeight - 1;
    Vec3<size_t> copySize = {copySizeX, copySizeY, 0x3};
    Vec3<size_t> srcSize = {copySizeX, copySizeY, 0x3};
    Vec3<size_t> dstSize = {copySizeX, copySizeY, 0x3};

    size_t srcRowPitch = srcSize.x;
    size_t srcSlicePitch = srcSize.y;
    size_t dstRowPitch = dstSize.x;
    size_t dstSlicePitch = dstSize.y;

    auto blitProperties = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0, &srcAlloc, 0,
                                                                     dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                     dstRowPitch, dstSlicePitch, &clearColorAllocation);
    ASSERT_FALSE(blitProperties.isSystemMemoryPoolUsed);
    blitProperties.bytesPerPixel = 4;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[100] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[100] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
}

HWTEST2_F(BlitTests, givenPlatformWithBlitSyncPropertiesAndSingleBytePatternWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitMemoryColorFillThenTheResultsAreTheSame, MatchAny) {
    DebugManagerStateRestore restore{};

    constexpr int32_t setWidth = 50;
    constexpr int32_t setHeight = 60;
    debugManager.flags.LimitBlitterMaxSetWidth.set(setWidth);
    debugManager.flags.LimitBlitterMaxSetHeight.set(setHeight);
    debugManager.flags.LimitBlitterMaxWidth.set(setWidth);
    debugManager.flags.LimitBlitterMaxHeight.set(setHeight);

    size_t dstSize = 3 * setWidth * setHeight + 1;
    MockGraphicsAllocation dstAlloc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                    reinterpret_cast<void *>(0x1234), 0x1000, 0, dstSize,
                                    MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    uint32_t pattern[4] = {};
    pattern[0] = 0x1;
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&dstAlloc, 0, dstSize, pattern, sizeof(uint8_t), 0);
    EXPECT_EQ(1u, blitProperties.fillPatternSize);

    auto nBlits = NEO::BlitCommandsHelper<FamilyType>::getNumberOfBlitsForColorFill(blitProperties.copySize, sizeof(uint8_t), pDevice->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    EXPECT_EQ(4u, nBlits);

    uint32_t streamBuffer[400] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[400] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[400] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
}

HWTEST2_F(BlitTests, givenPlatformWithBlitSyncPropertiesWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitMemoryFillThenTheResultsAreTheSame, MatchAny) {
    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    size_t maxBlitHeight = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironmentRef(), true));
    size_t dstSize = 2 * sizeof(uint32_t) * (maxBlitWidth * maxBlitHeight) + sizeof(uint32_t);
    MockGraphicsAllocation dstAlloc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                    reinterpret_cast<void *>(0x1234), 0x1000, 0, dstSize,
                                    MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    uint32_t pattern[4] = {};
    pattern[0] = 0x4567;
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&dstAlloc, 0, dstSize, pattern, sizeof(uint32_t), 0);
    ASSERT_TRUE(blitProperties.isSystemMemoryPoolUsed);

    auto nBlitsColorFill = NEO::BlitCommandsHelper<FamilyType>::getNumberOfBlitsForColorFill(blitProperties.copySize, sizeof(uint32_t), pDevice->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    auto nBlitsFill = NEO::BlitCommandsHelper<FamilyType>::getNumberOfBlitsForFill(blitProperties.copySize, sizeof(uint32_t), pDevice->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    EXPECT_EQ(3u, nBlitsColorFill);
    EXPECT_EQ(nBlitsFill, nBlitsColorFill);

    uint32_t streamBuffer[1200] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[1200] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[1300] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
}

HWTEST2_F(BlitTests, givenSystemMemoryPlatformWithBlitSyncPropertiesWithAndWithoutUseAdditionalPropertiesWhenCallingDispatchBlitMemoryFillThenTheResultsAreTheSame, MatchAny) {
    DebugManagerStateRestore restore;
    debugManager.flags.LimitBlitterMaxWidth.set(1024);
    debugManager.flags.LimitBlitterMaxHeight.set(1024);
    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    size_t maxBlitHeight = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(pDevice->getRootDeviceEnvironmentRef(), true));
    size_t dstSize = 2 * sizeof(uint32_t) * (maxBlitWidth * maxBlitHeight) + sizeof(uint32_t);
    void *dstPtr = malloc(dstSize);

    uint32_t pattern[4] = {};
    pattern[0] = 0x4567;
    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(nullptr, reinterpret_cast<uint64_t>(dstPtr), dstSize, pattern, sizeof(uint32_t), 0);
    ASSERT_TRUE(blitProperties.isSystemMemoryPoolUsed);

    auto nBlitsColorFill = NEO::BlitCommandsHelper<FamilyType>::getNumberOfBlitsForColorFill(blitProperties.copySize, sizeof(uint32_t), pDevice->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    auto nBlitsFill = NEO::BlitCommandsHelper<FamilyType>::getNumberOfBlitsForFill(blitProperties.copySize, sizeof(uint32_t), pDevice->getRootDeviceEnvironmentRef(), blitProperties.isSystemMemoryPoolUsed);
    EXPECT_EQ(3u, nBlitsColorFill);
    EXPECT_EQ(nBlitsFill, nBlitsColorFill);

    uint32_t streamBuffer[1200] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer2[1200] = {};
    LinearStream stream2(streamBuffer2, sizeof(streamBuffer2));
    auto blitResult2 = NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream2, pDevice->getRootDeviceEnvironmentRef());
    EXPECT_NE(nullptr, blitResult2.lastBlitCommand);

    // change productHelper to return true
    pDevice->getRootDeviceEnvironmentRef().productHelper.reset(new MockProductHelperHw<productFamily>);
    auto *mockProductHelper = static_cast<MockProductHelperHw<productFamily> *>(pDevice->getRootDeviceEnvironmentRef().productHelper.get());
    mockProductHelper->enableAdditionalBlitProperties = true;

    uint32_t streamBuffer3[1300] = {};
    LinearStream stream3(streamBuffer3, sizeof(streamBuffer3));
    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream3, pDevice->getRootDeviceEnvironmentRef());

    EXPECT_EQ(stream.getUsed(), stream3.getUsed());
    EXPECT_EQ(0, memcmp(ptrOffset(stream.getCpuBase(), 0), ptrOffset(stream3.getCpuBase(), 0), std::min(stream.getUsed(), stream3.getUsed())));
    free(dstPtr);
}

HWTEST_F(BlitTests, givenBlitPropertieswithImageOperationWhenCallingEstimateBlitCommandSizeThenBlockCopySizeIsReturned) {
    size_t maxBlitWidth = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(pDevice->getRootDeviceEnvironmentRef()));
    Vec3<size_t> copySize{maxBlitWidth - 1, 1, 1};
    NEO::CsrDependencies csrDependencies{};

    size_t totalSize = NEO::BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(copySize, csrDependencies, false, false, true, pDevice->getRootDeviceEnvironmentRef(), false, false);

    size_t expectedSize = sizeof(typename FamilyType::XY_BLOCK_COPY_BLT);
    expectedSize += NEO::BlitCommandsHelper<FamilyType>::estimatePostBlitCommandSize();
    expectedSize += NEO::BlitCommandsHelper<FamilyType>::estimatePreBlitCommandSize();
    EXPECT_EQ(expectedSize, totalSize);
}

using IsWithinGen12LPAndXe3Core = IsWithinGfxCore<IGFX_GEN12LP_CORE, IGFX_XE3_CORE>;
HWTEST2_F(BlitTests, givenXyCopyBltCommandWhenApplyBlitPropertiesIsCalledThenNothingChanged, IsWithinGen12LPAndXe3Core) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    auto bltCmdBefore = bltCmd;
    BlitProperties properties = {};
    NEO::BlitCommandsHelper<FamilyType>::applyAdditionalBlitProperties(properties, bltCmd, pDevice->getRootDeviceEnvironment(), false);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_COPY_BLT)), 0);
    NEO::BlitCommandsHelper<FamilyType>::applyAdditionalBlitProperties(properties, bltCmd, pDevice->getRootDeviceEnvironment(), true);
    EXPECT_EQ(memcmp(&bltCmd, &bltCmdBefore, sizeof(XY_COPY_BLT)), 0);
}

HWTEST2_F(BlitTests, givenSrcAndDstAllocationWithCompressionEnabledWhenAppendBlitCommandsBlockCopyThenSetSrcAndDstTargetMemToLocalMemAndCompressionHandled, IsXeHpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties{};
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x2345),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockDstAllocation.setGmm(gmm.get(), 0);
    mockSrcAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockDstAllocation;
    properties.srcAllocation = &mockSrcAllocation;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    EXPECT_EQ(blitCmd.getSourceCompressionEnable(), XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_BLOCK_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    EXPECT_EQ(blitCmd.getSourceAuxiliarysurfacemode(), XY_BLOCK_COPY_BLT::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

HWTEST2_F(BlitTests, givenSrcAndDstAllocationWithoutCompressionEnabledWhenAppendBlitCommandsBlockCopyThenSetSrcAndDstTargetMemToLocalMemAndCompressionHandled, IsXeHpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties{};
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(false);
    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x2345),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockDstAllocation.setGmm(gmm.get(), 0);
    mockSrcAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockDstAllocation;
    properties.srcAllocation = &mockSrcAllocation;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), 0u);
    EXPECT_EQ(blitCmd.getSourceCompressionEnable(), 0u);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), 0u);
    EXPECT_EQ(blitCmd.getSourceAuxiliarysurfacemode(), 0u);
}

HWTEST2_F(BlitTests, givenNullAllocationsWhenAppendBlitCommandsBlockCopyThenSetSrcAndDstTargetMemToLocalMemAndCompressionHandled, IsXeHpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties{};

    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), 0u);
    EXPECT_EQ(blitCmd.getSourceCompressionEnable(), 0u);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), 0u);
    EXPECT_EQ(blitCmd.getSourceAuxiliarysurfacemode(), 0u);
}

HWTEST2_F(BlitTests, givenNullAllocationsWhenAdjustControlSurfaceTypeThenSurfaceTypeWillNotChange, IsXeHpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    BlitProperties properties{};
    properties.srcAllocation = nullptr;
    properties.dstAllocation = nullptr;

    constexpr auto initialSrcSurface = 0; // CONTROL_SURFACE_TYPE_3D
    constexpr auto initialDstSurface = 0; // CONTROL_SURFACE_TYPE_3D

    BlitCommandsHelper<FamilyType>::adjustControlSurfaceType(properties, blitCmd);

    EXPECT_EQ(blitCmd.getSourceControlSurfaceType(), initialSrcSurface);
    EXPECT_EQ(blitCmd.getDestinationControlSurfaceType(), initialDstSurface);
}

HWTEST2_F(BlitTests, givenSrcAndDstAllocationWithCompressionEnabledAndForceBufferCompressionFormatSetWhenAppendBlitCommandsBlockCopyIsCalledThenSetSrcAndDstTargetMemToSystemMemAndCompressionHandled, IsAtLeastXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t compressionFormat = 1;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(compressionFormat));
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::buffer, reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::buffer, reinterpret_cast<void *>(0x2345), 0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockDstAllocation.setGmm(gmm.get(), 0);
    mockSrcAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockDstAllocation;
    properties.srcAllocation = &mockSrcAllocation;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationCompressionFormat(), compressionFormat);
    EXPECT_EQ(blitCmd.getSourceCompressionFormat(), compressionFormat);
    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenSrcAllocationWithoutCompressionEnabledAndForceBufferCompressionFormatSetWhenAppendBlitCommandsBlockCopyIsCalledThenSetSrcTargetMemToSystemMemAndCompressionHandled, IsAtLeastXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(false);
    mockSrcAllocation.setGmm(gmm.get(), 0);

    properties.srcAllocation = &mockSrcAllocation;
    properties.dstAllocation = nullptr;

    debugManager.flags.ForceBufferCompressionFormat.set(1);

    auto initialDstCompressionFormat = blitCmd.getDestinationCompressionFormat();
    auto initialSrcCompressionFormat = blitCmd.getSourceCompressionFormat();

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationCompressionFormat(), initialDstCompressionFormat);
    EXPECT_EQ(blitCmd.getSourceCompressionFormat(), initialSrcCompressionFormat);
    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenDstAllocationWithoutCompressionEnabledAndForceBufferCompressionFormatSetWhenAppendBlitCommandsBlockCopyIsCalledThenSetDstTargetMemToSystemMemAndCompressionHandled, IsAtLeastXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x2000, 0, sizeof(uint32_t), MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(false);
    mockDstAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockDstAllocation;
    properties.srcAllocation = nullptr;

    debugManager.flags.ForceBufferCompressionFormat.set(1);

    auto initialDstCompressionFormat = blitCmd.getDestinationCompressionFormat();
    auto initialSrcCompressionFormat = blitCmd.getSourceCompressionFormat();

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationCompressionFormat(), initialDstCompressionFormat);
    EXPECT_EQ(blitCmd.getSourceCompressionFormat(), initialSrcCompressionFormat);
    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenNullAllocationsAndForceBufferCompressionFormatSetWhenAppendBlitCommandsBlockCopyIsCalledThenSetSrcAndDstTargetMemToSystemMemAndCompressionHandled, IsAtLeastXe2HpgCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    blitCmd.setSourceX1CoordinateLeft(0);
    blitCmd.setSourceY1CoordinateTop(0);
    blitCmd.setDestinationX2CoordinateRight(1);
    blitCmd.setDestinationY2CoordinateBottom(1);

    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;

    debugManager.flags.ForceBufferCompressionFormat.set(1);

    auto initialDstCompressionFormat = blitCmd.getDestinationCompressionFormat();
    auto initialSrcCompressionFormat = blitCmd.getSourceCompressionFormat();

    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, blitCmd, pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(blitCmd.getDestinationCompressionFormat(), initialDstCompressionFormat);
    EXPECT_EQ(blitCmd.getSourceCompressionFormat(), initialSrcCompressionFormat);
    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(blitCmd.getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenNullAllocationsWhenAppendBlitCommandsForImagesThenSlicePitchesWillBeUpdated, IsNotPVC) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    XY_BLOCK_COPY_BLT blitCmd = FamilyType::cmdInitXyBlockCopyBlt;

    BlitProperties properties{};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    properties.dstSize = {8, 12, 1};
    properties.srcSize = {10, 10, 1};
    properties.dstRowPitch = 0x40;
    properties.srcRowPitch = 0x10;
    uint32_t dstSlicePitch = 0;
    uint32_t srcSlicePitch = 0;

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, blitCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_NE(dstSlicePitch, 0u);
    EXPECT_NE(srcSlicePitch, 0u);
}
