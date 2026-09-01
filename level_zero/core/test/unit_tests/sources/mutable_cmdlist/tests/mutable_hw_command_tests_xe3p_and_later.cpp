/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/flush_caches_bitmask.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {
namespace ult {

using MutableHwCommandTestXe3p = Test<MutableHwCommandFixture>;

HWTEST2_F(MutableHwCommandTestXe3p, givenMutableComputeWalkerWhenSettingIndirectDataAddressThenCorrectValueSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint64_t indirectAddress = 0xFFAABB0000;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto *walkerCmdIndirectAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(mutableWalker->getInlineDataPointer(), this->indirectOffset));
    auto *cpuBufferIndirectAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(mutableWalker->getHostMemoryInlineDataPointer(), this->indirectOffset));

    mutableWalker->setIndirectDataStartAddress(indirectAddress);

    EXPECT_EQ(indirectAddress, *cpuBufferIndirectAddressPatch);
    EXPECT_EQ(0u, *walkerCmdIndirectAddressPatch);

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBufferIndirectAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(mutableWalker->getHostMemoryInlineDataPointer(), this->indirectOffset));

    mutableWalker->setIndirectDataStartAddress(indirectAddress);
    EXPECT_EQ(indirectAddress, *cpuBufferIndirectAddressPatch);
    EXPECT_EQ(indirectAddress, *walkerCmdIndirectAddressPatch);
}

HWTEST2_F(MutableHwCommandTestXe3p, givenMutableComputeWalkerWhenSettingIndirectDataSizeThenNoActionIsTaken, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    size_t indirectSize = 0x100;

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    mutableWalker->setIndirectDataSize(indirectSize);

    EXPECT_EQ(0, memcmp(this->cmdBufferCpuPtr, &walkerTemplate, this->walkerCmdSize));
    EXPECT_EQ(0, memcmp(this->cmdBufferGpuPtr, &walkerTemplate, this->walkerCmdSize));
}

HWTEST2_F(MutableHwCommandTestXe3p, givenMutableComputeWalkerWhenSettingBindingTablePointerThenNoActionIsTaken, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint64_t bindingTableAddress = 0xFFAABB0000;

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    mutableWalker->setBindingTablePointer(bindingTableAddress);

    EXPECT_EQ(0, memcmp(this->cmdBufferCpuPtr, &walkerTemplate, this->walkerCmdSize));
    EXPECT_EQ(0, memcmp(this->cmdBufferGpuPtr, &walkerTemplate, this->walkerCmdSize));
}

HWTEST2_F(MutableHwCommandTestXe3p, givenTwoMutableComputeWalkersWhenCopyingDataToHostPointerThenDataIsCopied, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b1010u;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    alignas(8) uint8_t srcWalkerGpuBuffer[sizeof(WalkerType)];
    void *srcWalkerCpuBuffer = L0::MCL::MutableComputeWalkerHw<FamilyType>::createCommandBuffer();
    constexpr uint8_t srcWalkerIndirectOffset = 0x0;
    constexpr uint8_t srcWalkerScratchOffset = 0x8;

    L0::MCL::MutableComputeWalkerHw<FamilyType> srcMutableWalker(srcWalkerGpuBuffer, srcWalkerIndirectOffset, srcWalkerScratchOffset, srcWalkerCpuBuffer, false);
    memcpy(srcWalkerCpuBuffer, &walkerTemplate, sizeof(WalkerType));
    auto srcWalkerCpuCmd = reinterpret_cast<WalkerType *>(srcWalkerCpuBuffer);

    uint64_t postSyncAddress = 0xCCDD0000;
    uint64_t indirectAddress = 0xAABB0000;

    srcWalkerCpuCmd->getPostSync().setDestinationAddress(postSyncAddress);
    srcWalkerCpuCmd->getPostSyncOpn1().setDestinationAddress(postSyncAddress + 0x100);
    srcWalkerCpuCmd->getPostSyncOpn2().setDestinationAddress(postSyncAddress + 0x200);
    srcWalkerCpuCmd->getPostSyncOpn3().setDestinationAddress(postSyncAddress + 0x300);
    srcMutableWalker.setPostSyncFlushMask(postSyncFlushMask);
    memcpy(ptrOffset(srcMutableWalker.getHostMemoryInlineDataPointer(), srcWalkerIndirectOffset), &indirectAddress, sizeof(indirectAddress));

    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto walkerCpuCmd = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    auto *cpuBufferIndirectAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(mutableWalker->getHostMemoryInlineDataPointer(), this->indirectOffset));

    mutableWalker->copyWalkerDataToHostBuffer(&srcMutableWalker);

    EXPECT_EQ(indirectAddress, *cpuBufferIndirectAddressPatch);
    EXPECT_EQ(postSyncAddress, walkerCpuCmd->getPostSync().getDestinationAddress());
    EXPECT_EQ(postSyncAddress + 0x100, walkerCpuCmd->getPostSyncOpn1().getDestinationAddress());
    EXPECT_EQ(postSyncAddress + 0x200, walkerCpuCmd->getPostSyncOpn2().getDestinationAddress());
    EXPECT_EQ(postSyncAddress + 0x300, walkerCpuCmd->getPostSyncOpn3().getDestinationAddress());
    EXPECT_EQ(postSyncFlushMask, mutableWalker->getPostSyncFlushMask());
}

HWTEST2_F(MutableHwCommandTestXe3p,
          givenMutableComputeWalkerWhenSettingScratchAddressThenCorrectValueSet,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    size_t scratchPatchAddress = 0xF000;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    mutableWalker->updateWalkerScratchPatchAddress(scratchPatchAddress);

    auto *cpuBufferScratchAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(mutableWalker->getHostMemoryInlineDataPointer(), this->scratchOffset));
    EXPECT_EQ(scratchPatchAddress, *cpuBufferScratchAddressPatch);
}

HWTEST2_F(MutableHwCommandTestXe3p,
          givenNonZeroHostAndImportedArgsWhenUpdateL3FlushAfterWalkerThenFlushEnabled,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0001u;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    mutableWalker->setPostSyncFlushMask(postSyncFlushMask);

    mutableWalker->updateL3FlushAfterWalker(1u, 1u);

    EXPECT_TRUE(cpuBuffer->getPostSync().getL2TransientFlush());
    EXPECT_TRUE(cpuBuffer->getPostSync().getL2Flush());
    EXPECT_TRUE(walkerCmd->getPostSync().getL2TransientFlush());
    EXPECT_TRUE(walkerCmd->getPostSync().getL2Flush());
}

HWTEST2_F(MutableHwCommandTestXe3p,
          givenZeroHostAndImportedArgsWhenUpdateL3FlushAfterWalkerThenBothThenFlushDisabled,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0001u;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    walkerTemplate.getPostSync().setL2Flush(true);
    walkerTemplate.getPostSync().setL2TransientFlush(true);

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    mutableWalker->setPostSyncFlushMask(postSyncFlushMask);

    mutableWalker->updateL3FlushAfterWalker(0u, 0u);

    EXPECT_FALSE(cpuBuffer->getPostSync().getL2TransientFlush());
    EXPECT_FALSE(cpuBuffer->getPostSync().getL2Flush());
    EXPECT_FALSE(walkerCmd->getPostSync().getL2TransientFlush());
    EXPECT_FALSE(walkerCmd->getPostSync().getL2Flush());
}

HWTEST2_F(MutableHwCommandTestXe3p,
          givenPostSyncFlushMaskWhenUpdateL3FlushAfterWalkerThenOnlySelectedPostSyncsAreUpdated,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b1010u;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    mutableWalker->setPostSyncFlushMask(postSyncFlushMask);

    mutableWalker->updateL3FlushAfterWalker(1u, 1u);

    for (auto *postSync : {&cpuBuffer->getPostSync(), &walkerCmd->getPostSync(),
                           &cpuBuffer->getPostSyncOpn2(), &walkerCmd->getPostSyncOpn2()}) {
        EXPECT_FALSE(postSync->getL2TransientFlush());
        EXPECT_FALSE(postSync->getL2Flush());
    }
    for (auto *postSync : {&cpuBuffer->getPostSyncOpn1(), &walkerCmd->getPostSyncOpn1(),
                           &cpuBuffer->getPostSyncOpn3(), &walkerCmd->getPostSyncOpn3()}) {
        EXPECT_TRUE(postSync->getL2TransientFlush());
        EXPECT_TRUE(postSync->getL2Flush());
    }
}

HWTEST2_F(MutableHwCommandTestXe3p,
          givenFlushAllCachesDebugFlagWhenUpdateL3FlushAfterWalkerThenFlushesAreCorrect,
          IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    constexpr NEO::PostSyncFlushMask postSyncFlushMask = 0b0001u;
    DebugManagerStateRestore restorer;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    debugManager.flags.FlushAllCaches.set(static_cast<int32_t>(NEO::FlushCachesBitmask::l2Flush));
    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    mutableWalker->setPostSyncFlushMask(postSyncFlushMask);

    mutableWalker->updateL3FlushAfterWalker(0u, 0u);
    EXPECT_FALSE(cpuBuffer->getPostSync().getL2TransientFlush());
    EXPECT_TRUE(cpuBuffer->getPostSync().getL2Flush());
    EXPECT_TRUE(walkerCmd->getPostSync().getL2Flush());

    debugManager.flags.FlushAllCaches.set(static_cast<int32_t>(NEO::FlushCachesBitmask::l2TransientFlush));
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    mutableWalker->setPostSyncFlushMask(postSyncFlushMask);

    mutableWalker->updateL3FlushAfterWalker(0u, 0u);
    EXPECT_TRUE(cpuBuffer->getPostSync().getL2TransientFlush());
    EXPECT_FALSE(cpuBuffer->getPostSync().getL2Flush());
    EXPECT_TRUE(walkerCmd->getPostSync().getL2TransientFlush());
}

} // namespace ult
} // namespace L0
