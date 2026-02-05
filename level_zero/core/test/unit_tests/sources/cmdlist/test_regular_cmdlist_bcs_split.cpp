/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/transfer_direction.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

struct RegularCmdListBcsSplitTests : public InOrderCmdListFixture {
    void SetUp() override {
        constexpr int32_t defaultTransferDirection = ~(1 << static_cast<int32_t>(NEO::TransferDirection::localToLocal));

        NEO::debugManager.flags.SplitBcsCopy.set(1);
        NEO::debugManager.flags.SplitBcsRequiredTileCount.set(1);
        NEO::debugManager.flags.SplitBcsRequiredEnginesCount.set(numCopyEngines);
        NEO::debugManager.flags.SplitBcsMask.set(0b11110);
        NEO::debugManager.flags.SplitBcsTransferDirectionMask.set(defaultTransferDirection);
        NEO::debugManager.flags.SplitBcsForRegularCmdList.set(1);

        hwInfoBackup = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        defaultHwInfo->featureTable.ftrBcsInfo = 0b111111111;

        InOrderCmdListFixture::SetUp();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    DestroyableZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<gfxCoreFamily>>> createBcsSplitRegularCmdList() {
        return createRegularCmdList<gfxCoreFamily>(true);
    }

    std::unique_ptr<VariableBackup<HardwareInfo>> hwInfoBackup;
    const uint32_t numCopyEngines = 4;
};

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenEnableBcsSplitCalledThenRecordedModeIsSet, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    EXPECT_EQ(BcsSplitParams::BcsSplitMode::recorded, cmdList->bcsSplitMode);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWithoutInOrderWhenEnableBcsSplitCalledThenDisabledModeIsSet, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    ze_command_queue_desc_t desc = {};

    mockCmdQs.emplace_back(std::make_unique<Mock<CommandQueue>>(device, csr, &desc));

    cmdList->initialize(device, NEO::EngineGroupType::copy, 0);

    cmdList->enableBcsSplit();

    EXPECT_EQ(BcsSplitParams::BcsSplitMode::disabled, cmdList->bcsSplitMode);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenGetRegularCmdListsForSplitCalledThenCorrectCountReturned, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    auto result = cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_EQ(std::min(totalSize / perEngineMaxSize, bcsSplit->cmdLists.size()), result.size());

    EXPECT_GE(cmdList->subCmdListsForRecordedBcsSplit.size(), result.size());

    for (auto subCmdList : cmdList->subCmdListsForRecordedBcsSplit) {
        EXPECT_FALSE(subCmdList->isCopyOffloadEnabled());
        EXPECT_FALSE(subCmdList->isBcsSplitEnabled());
    }
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenEnsureSubCmdListsCalledMultipleTimesThenNoExtraAllocations, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    auto result1 = cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());
    size_t initialCount = cmdList->subCmdListsForRecordedBcsSplit.size();

    auto result2 = cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_EQ(initialCount, cmdList->subCmdListsForRecordedBcsSplit.size());
    EXPECT_EQ(result1.size(), result2.size());
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenSplitBcsForRegularCmdListDisabledWhenEnableBcsSplitCalledThenDisabledModeIsSet, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.SplitBcsForRegularCmdList.set(-1);

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);

    cmdList->enableBcsSplit();

    EXPECT_EQ(BcsSplitParams::BcsSplitMode::disabled, cmdList->bcsSplitMode);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenGetRegularCmdListsForSplitWithSmallSizeThenSingleEngineReturned, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 1 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    auto result = cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_EQ(1u, result.size());
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenPlatformWithoutAdditionalBlitPropertiesWhenEnableBcsSplitCalledThenDisabledModeIsSet, IsAtLeastXeHpcCore) {
    if (device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(true);

    cmdList->enableBcsSplit();

    EXPECT_EQ(BcsSplitParams::BcsSplitMode::disabled, cmdList->bcsSplitMode);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWithSubCmdListsWhenCloseCalledThenSubCmdListsAreClosed, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_FALSE(cmdList->subCmdListsForRecordedBcsSplit.empty());

    for (auto &subCmdList : cmdList->subCmdListsForRecordedBcsSplit) {
        EXPECT_FALSE(WhiteBox<L0::CommandList>::whiteboxCast(subCmdList)->closedCmdList);
    }

    cmdList->close();

    for (auto &subCmdList : cmdList->subCmdListsForRecordedBcsSplit) {
        EXPECT_TRUE(WhiteBox<L0::CommandList>::whiteboxCast(subCmdList)->closedCmdList);
    }
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenExecuteCommandListsCalledThenDispatchRecordedBcsSplitIsCalled, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    cmdList->close();

    ze_command_queue_desc_t desc = {};
    desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;

    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    auto mockCmdQ = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    mockCmdQ->initialize(false, false, false);

    auto handle = cmdList->toHandle();

    for (auto i = 0u; i < cmdList->subCmdListsForRecordedBcsSplit.size(); i++) {
        EXPECT_EQ(0u, static_cast<WhiteBox<CommandList> *>(bcsSplit->cmdLists[i])->cmdQImmediate->getTaskCount());
    }

    mockCmdQ->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);

    for (auto i = 0u; i < cmdList->subCmdListsForRecordedBcsSplit.size(); i++) {
        EXPECT_NE(0u, static_cast<WhiteBox<CommandList> *>(bcsSplit->cmdLists[i])->cmdQImmediate->getTaskCount());
    }
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenResetCalledThenDestroyRecordedBcsSplitResourcesIsCalled, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_FALSE(cmdList->subCmdListsForRecordedBcsSplit.empty());

    cmdList->reset();

    EXPECT_TRUE(cmdList->subCmdListsForRecordedBcsSplit.empty());
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenStoreEventsForBcsSplitCalledThenEventsAreStoredCorrectly, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    auto context = Context::fromHandle(cmdList->getCmdListContext());
    auto markerEventIndex = bcsSplit->events.obtainForRecordedSplit(context);

    auto markerEvent = &bcsSplit->events.getEventResources().marker[markerEventIndex];

    EXPECT_TRUE(markerEvent->reservedForRecordedCmdList);

    cmdList->storeEventsForBcsSplit(markerEvent);

    EXPECT_EQ(1u, cmdList->eventsForRecordedBcsSplit.size());
    EXPECT_EQ(markerEvent, cmdList->eventsForRecordedBcsSplit[0]);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWithMultipleEventsWhenDestroyCalledThenAllEventsAreReleased, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    auto context = Context::fromHandle(cmdList->getCmdListContext());

    auto markerEventIndex1 = bcsSplit->events.obtainForRecordedSplit(context);
    auto markerEvent1 = &bcsSplit->events.getEventResources().marker[markerEventIndex1];
    cmdList->storeEventsForBcsSplit(markerEvent1);

    auto markerEventIndex2 = bcsSplit->events.obtainForRecordedSplit(context);
    auto markerEvent2 = &bcsSplit->events.getEventResources().marker[markerEventIndex2];
    cmdList->storeEventsForBcsSplit(markerEvent2);

    EXPECT_EQ(2u, cmdList->eventsForRecordedBcsSplit.size());

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    EXPECT_FALSE(cmdList->subCmdListsForRecordedBcsSplit.empty());
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenBcsSplitEventsWhenObtainForRecordedSplitCalledMultipleTimesThenDifferentIndicesReturned, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    auto context = Context::fromHandle(cmdList->getCmdListContext());

    auto markerEventIndex1 = bcsSplit->events.obtainForRecordedSplit(context);
    auto markerEvent1 = &bcsSplit->events.getEventResources().marker[markerEventIndex1];

    auto markerEventIndex2 = bcsSplit->events.obtainForRecordedSplit(context);

    EXPECT_NE(markerEventIndex1, markerEventIndex2);

    cmdList->storeEventsForBcsSplit(markerEvent1);
    EXPECT_EQ(1u, cmdList->eventsForRecordedBcsSplit.size());

    auto markerEvent2 = &bcsSplit->events.getEventResources().marker[markerEventIndex2];

    EXPECT_TRUE(markerEvent1->reservedForRecordedCmdList);
    EXPECT_TRUE(markerEvent2->reservedForRecordedCmdList);

    cmdList->resetBcsSplitEvents(false);

    EXPECT_EQ(1u, cmdList->eventsForRecordedBcsSplit.size());
    EXPECT_TRUE(markerEvent1->reservedForRecordedCmdList);

    cmdList->resetBcsSplitEvents(true);

    EXPECT_TRUE(cmdList->eventsForRecordedBcsSplit.empty());
    EXPECT_FALSE(markerEvent1->reservedForRecordedCmdList);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenAppendMemoryCopyCalledThenSubCmdListsContainCopyOperations, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    constexpr size_t copySize = 32 * MemoryConstants::megaByte;

    void *srcPtr = nullptr;
    void *dstPtr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, copySize, 1, &srcPtr);
    context->allocHostMem(&hostDesc, copySize, 1, &dstPtr);

    CmdListMemoryCopyParams memoryCopyParams = {};
    cmdList->appendMemoryCopy(dstPtr, srcPtr, copySize, nullptr, 0, nullptr, memoryCopyParams);

    EXPECT_FALSE(cmdList->subCmdListsForRecordedBcsSplit.empty());

    for (auto &subCmdList : cmdList->subCmdListsForRecordedBcsSplit) {
        auto subCmdListImp = static_cast<WhiteBox<L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>> *>(subCmdList);
        auto commandStream = subCmdListImp->commandContainer.getCommandStream();

        GenCmdList cmdListParsed;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdListParsed, commandStream->getCpuBase(), commandStream->getUsed()));

        auto copyCommands = findAll<XY_COPY_BLT *>(cmdListParsed.begin(), cmdListParsed.end());
        EXPECT_FALSE(copyCommands.empty());
    }

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenDispatchRecordedBcsSplitCalledThenEventsAreResetButReservedFlagIsNotCleared, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    auto ctx = Context::fromHandle(cmdList->getCmdListContext());
    auto markerEventIndex = bcsSplit->events.obtainForRecordedSplit(ctx);
    auto markerEvent = &bcsSplit->events.getEventResources().marker[markerEventIndex];

    cmdList->storeEventsForBcsSplit(markerEvent);

    EXPECT_TRUE(markerEvent->reservedForRecordedCmdList);

    constexpr size_t copySize = 32 * MemoryConstants::megaByte;

    void *srcPtr = nullptr;
    void *dstPtr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, copySize, 1, &srcPtr);
    context->allocHostMem(&hostDesc, copySize, 1, &dstPtr);

    CmdListMemoryCopyParams memoryCopyParams = {};
    cmdList->appendMemoryCopy(dstPtr, srcPtr, copySize, nullptr, 0, nullptr, memoryCopyParams);
    cmdList->close();

    cmdList->dispatchRecordedBcsSplit();

    EXPECT_TRUE(markerEvent->reservedForRecordedCmdList);
    EXPECT_FALSE(cmdList->eventsForRecordedBcsSplit.empty());

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWithStoredEventsWhenDestroyCalledThenReservedFlagIsCleared, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    auto bcsSplit = static_cast<Device *>(device)->bcsSplit.get();

    auto ctx = Context::fromHandle(cmdList->getCmdListContext());
    auto markerEventIndex = bcsSplit->events.obtainForRecordedSplit(ctx);
    auto markerEvent = &bcsSplit->events.getEventResources().marker[markerEventIndex];

    cmdList->storeEventsForBcsSplit(markerEvent);

    EXPECT_TRUE(markerEvent->reservedForRecordedCmdList);

    constexpr size_t totalSize = 32 * MemoryConstants::megaByte;
    constexpr size_t perEngineMaxSize = 8 * MemoryConstants::megaByte;

    cmdList->getRegularCmdListsForSplit(totalSize, perEngineMaxSize, bcsSplit->cmdLists.size());

    cmdList.reset();

    EXPECT_FALSE(markerEvent->reservedForRecordedCmdList);
}

HWTEST2_F(RegularCmdListBcsSplitTests, givenRegularCmdListWhenAppendMemoryCopyCalledThenMainCmdListDoesNotContainCopyCommands, IsAtLeastXeHpcCore) {
    if (!device->getProductHelper().useAdditionalBlitProperties()) {
        GTEST_SKIP();
    }

    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    auto cmdList = createBcsSplitRegularCmdList<FamilyType::gfxCoreFamily>();

    constexpr size_t copySize = 32 * MemoryConstants::megaByte;

    void *srcPtr = nullptr;
    void *dstPtr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, copySize, 1, &srcPtr);
    context->allocHostMem(&hostDesc, copySize, 1, &dstPtr);

    CmdListMemoryCopyParams memoryCopyParams = {};
    cmdList->appendMemoryCopy(dstPtr, srcPtr, copySize, nullptr, 0, nullptr, memoryCopyParams);

    EXPECT_FALSE(cmdList->subCmdListsForRecordedBcsSplit.empty());

    auto mainCommandStream = cmdList->commandContainer.getCommandStream();

    GenCmdList cmdListParsed;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdListParsed, mainCommandStream->getCpuBase(), mainCommandStream->getUsed()));

    auto copyCommands = findAll<XY_COPY_BLT *>(cmdListParsed.begin(), cmdListParsed.end());
    EXPECT_TRUE(copyCommands.empty());

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);
}

} // namespace ult
} // namespace L0
