/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"
#include "per_product_test_definitions.h"

using namespace NEO;
using NvlpBlitTests = Test<DeviceFixture>;

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenDstAllocationCompressedWhenAppendBlitCommandsMemCopyThenCompressionFormatIs0x2) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0x2u, bltCmd.getCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenSrcAllocationCompressedWhenAppendBlitCommandsMemCopyThenCompressionFormatIs0x2) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = nullptr;
    properties.srcAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0x2u, bltCmd.getCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenNoAllocationWhenAppendBlitCommandsMemCopyThenCompressionFormatIsZero) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0u, bltCmd.getCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenDstAllocationCompressedAndDebugFlagSetWhenAppendBlitCommandsMemCopyThenDebugFlagOverrides) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t debugCompressionFormat = 15u;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(debugCompressionFormat);

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.dstAllocation = &mockAllocation;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(debugCompressionFormat, bltCmd.getCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenSrcAllocationCompressedWhenAppendBlitCommandsBlockCopyThenSrcCompressionFormatIs0x2) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd.setDestinationX2CoordinateRight(1);
    bltCmd.setDestinationY2CoordinateBottom(1);
    BlitProperties properties = {};

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.srcAllocation = &mockAllocation;
    properties.dstAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0x2u, bltCmd.getSourceCompressionFormat());
    EXPECT_EQ(0u, bltCmd.getDestinationCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenDstAllocationCompressedWhenAppendBlitCommandsBlockCopyThenDstCompressionFormatIs0x2) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd.setDestinationX2CoordinateRight(1);
    bltCmd.setDestinationY2CoordinateBottom(1);
    BlitProperties properties = {};

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    properties.srcAllocation = nullptr;
    properties.dstAllocation = &mockAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0u, bltCmd.getSourceCompressionFormat());
    EXPECT_EQ(0x2u, bltCmd.getDestinationCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenBothAllocationsCompressedWhenAppendBlitCommandsBlockCopyThenBothCompressionFormatsAre0x2) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd.setDestinationX2CoordinateRight(1);
    bltCmd.setDestinationY2CoordinateBottom(1);
    BlitProperties properties = {};

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x2345),
                                             0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockSrcAllocation.setGmm(gmm.get(), 0);
    mockDstAllocation.setGmm(gmm.get(), 0);

    properties.srcAllocation = &mockSrcAllocation;
    properties.dstAllocation = &mockDstAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0x2u, bltCmd.getSourceCompressionFormat());
    EXPECT_EQ(0x2u, bltCmd.getDestinationCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenBothAllocationsCompressedAndForceBufferCompressionFormatSetWhenAppendBlitCommandsBlockCopyThenDebugFlagOverrides) {
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd.setDestinationX2CoordinateRight(1);
    bltCmd.setDestinationY2CoordinateBottom(1);
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t debugCompressionFormat = 5u;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(debugCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockSrcAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                             0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockDstAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x2345),
                                             0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockSrcAllocation.setGmm(gmm.get(), 0);
    mockDstAllocation.setGmm(gmm.get(), 0);

    properties.srcAllocation = &mockSrcAllocation;
    properties.dstAllocation = &mockDstAllocation;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(debugCompressionFormat, bltCmd.getSourceCompressionFormat());
    EXPECT_EQ(debugCompressionFormat, bltCmd.getDestinationCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenDstAllocationCompressedWhenDispatchBlitMemoryFillThenCompressionFormatIs0x2) {
    using MEM_SET = typename FamilyType::MEM_SET;

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto blitCmd = genCmdCast<MEM_SET *>(*itor);
    EXPECT_EQ(0x2u, blitCmd->getCompressionFormat());
}

NVLPTEST_F(NvlpBlitTests, givenNvlpWhenDstAllocationCompressedAndDebugFlagSetWhenDispatchBlitMemoryFillThenDebugFlagOverrides) {
    using MEM_SET = typename FamilyType::MEM_SET;
    DebugManagerStateRestore dbgRestore;

    uint32_t debugCompressionFormat = 15u;
    debugManager.flags.BcsCompressionFormatForXe2Plus.set(debugCompressionFormat);

    uint32_t pattern = 1;
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    auto blitProperties = BlitProperties::constructPropertiesForMemoryFill(&mockAllocation, 0, mockAllocation.getUnderlyingBufferSize(), &pattern, sizeof(uint8_t), 0);

    NEO::BlitCommandsHelper<FamilyType>::dispatchBlitMemoryFill(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<MEM_SET *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto blitCmd = genCmdCast<MEM_SET *>(*itor);
    EXPECT_EQ(debugCompressionFormat, blitCmd->getCompressionFormat());
}
