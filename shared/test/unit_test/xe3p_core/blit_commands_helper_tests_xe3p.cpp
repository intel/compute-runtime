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