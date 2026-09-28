/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

#include "gtest/gtest.h"

using namespace NEO;
using BlitTests = Test<DeviceFixture>;

HWTEST2_F(BlitTests, givenXe3pCoreWhenAppendBlitCommandsMemCopyIsCalledThenNothingChanged, IsXe3pCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstAllocation = nullptr;
    properties.srcAllocation = nullptr;
    NEO::BlitCommandsHelper<FamilyType>::appendBlitCommandsMemCopy(properties, bltCmd, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(bltCmd.getCompressionFormat(), 0);
}

HWTEST2_F(BlitTests, givenXe3pCoreWhenDstGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
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

HWTEST2_F(BlitTests, givenXe3pCoreWhenDstGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
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

HWTEST2_F(BlitTests, givenXe3pCoreWhenDstGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.set(static_cast<int32_t>(newCompressionFormat));

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

HWTEST2_F(BlitTests, givenXe3pCoreWhenSrcGraphicAlloctionWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
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

HWTEST2_F(BlitTests, givenXe3pCoreWhenSrcGraphicAlloctionAndStatelessFlagSetWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
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

HWTEST2_F(BlitTests, givenXe3pCoreWhenSrcGraphicAlloctionAndStatelessFlagSetAndSystemMemoryPoolWhenAppendBlitCommandsMemCopyIsCalledThenCompressionChanged, IsXe3pCore) {
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    DebugManagerStateRestore dbgRestore;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.set(static_cast<int32_t>(newCompressionFormat));

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

HWTEST2_F(BlitTests, givenBcsCommandsHelperWhenIsFlushBetweenBlitsRequiredThenReturnFalse, IsXe3pCore) {
    EXPECT_FALSE(this->getHelper<ProductHelper>().isFlushBetweenBlitsRequired());
}

HWTEST2_F(BlitTests, givenCriWhenCheckingWriteSplitThenNotRequiredByDefault, IsCRI) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    EXPECT_FALSE(rootDeviceEnvironment.getProductHelper().isWriteSplitRequired(false));
    EXPECT_TRUE(rootDeviceEnvironment.getProductHelper().isWriteSplitRequired(true));
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isFlushBetweenBlitsRequired(rootDeviceEnvironment, true));
    EXPECT_NE(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false, false, BlitterConstants::maxBlitWidth),
              BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false, true, BlitterConstants::maxBlitWidth));
}

HWTEST2_F(BlitTests, givenCriAndWriteSplitEnabledByDebugFlagWhenCheckingWriteSplitThenRequiredOnlyForDstInSystemOrRemoteMemory, IsCRI) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideBcsWriteSplit.set(1);

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isFlushBetweenBlitsRequired(rootDeviceEnvironment, false));
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isFlushBetweenBlitsRequired(rootDeviceEnvironment, true));
    EXPECT_EQ(128u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false, true, BlitterConstants::maxBlitWidth));
    EXPECT_EQ(128u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, true, true, BlitterConstants::maxBlitWidth));
    EXPECT_EQ(512u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false, true, 4096u));
    EXPECT_NE(128u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment, false, false, BlitterConstants::maxBlitWidth));
}

HWTEST2_F(BlitTests, givenCriAndCopyToHostMemoryWhenDispatchBlitCommandsThenCopyIsSplitInto2MBChunksEachFollowedByFlush, IsCRI) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideBcsWriteSplit.set(1);

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();

    MockGraphicsAllocation srcAlloc(nullptr, 0x100000, 0);
    srcAlloc.overrideMemoryPool(MemoryPool::localMemory);
    MockGraphicsAllocation dstAlloc(nullptr, 0x40000000, 0);
    dstAlloc.overrideMemoryPool(MemoryPool::system4KBPages);

    const size_t numChunks = 4;
    auto blitProperties = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0, &srcAlloc, 0,
                                                                     0, 0, {numChunks * BlitterConstants::writeSplitChunkSize, 1, 1}, 0, 0, 0, 0, nullptr, false);

    uint32_t streamBuffer[400] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    BlitCommandsHelper<FamilyType>::dispatchBlitCommands(blitProperties, stream, rootDeviceEnvironment);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, stream.getCpuBase(), stream.getUsed()));
    auto blits = findAll<XY_COPY_BLT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(numChunks, blits.size());
    for (size_t i = 0; i < blits.size(); i++) {
        auto blitCmd = genCmdCast<XY_COPY_BLT *>(*blits[i]);
        EXPECT_EQ(BlitterConstants::writeSplitChunkSize, static_cast<uint64_t>(blitCmd->getDestinationX2CoordinateRight()) * blitCmd->getDestinationY2CoordinateBottom());
        auto nextBlit = (i + 1 < blits.size()) ? blits[i + 1] : cmdList.end();
        EXPECT_NE(nextBlit, find<MI_FLUSH_DW *>(blits[i], nextBlit));
    }

    BlitPropertiesContainer container;
    container.push_back(blitProperties);
    EXPECT_GE(BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(container, false, false, false, false, rootDeviceEnvironment), stream.getUsed());
}

HWTEST2_F(BlitTests, givenCriAndCopyToLocalMemoryWhenDispatchBlitCommandsThenCopyIsNotSplitInto2MBChunks, IsCRI) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideBcsWriteSplit.set(1);

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();

    MockGraphicsAllocation srcAlloc(nullptr, 0x100000, 0);
    srcAlloc.overrideMemoryPool(MemoryPool::system4KBPages);
    MockGraphicsAllocation dstAlloc(nullptr, 0x40000000, 0);
    dstAlloc.overrideMemoryPool(MemoryPool::localMemory);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(&dstAlloc, 0, &srcAlloc, 0,
                                                                     0, 0, {4 * BlitterConstants::writeSplitChunkSize, 1, 1}, 0, 0, 0, 0, nullptr, false);

    uint32_t streamBuffer[400] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    BlitCommandsHelper<FamilyType>::dispatchBlitCommands(blitProperties, stream, rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    EXPECT_EQ(1u, hwParser.getCommandCount<XY_COPY_BLT>());
    EXPECT_EQ(1u, hwParser.getCommandCount<MI_FLUSH_DW>());
}
