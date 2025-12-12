/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/mem_lifetime.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

struct InOrderCmdListTestsXe3pCoreAndLater : public InOrderCmdListFixture {
    void SetUp() override {
        debugManager.flags.Enable64BitAddressing.set(1);
        InOrderCmdListFixture::SetUp();
    }
};

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenExternalSyncStorageWhenCallingAppendThenSetCorrectGpuVa, IsAtLeastXe3pCore) {
    using TagSizeT = typename FamilyType::TimestampPacketType;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    constexpr uint64_t counterValue = 4;
    constexpr uint64_t incValue = 2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t)));
    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    eventObj->isTimestampEvent = true;
    eventObj->setSinglePacketSize(NEO::TimestampPackets<TagSizeT, 1>::getSinglePacketSize());

    auto handle = eventObj->toHandle();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(1u, eventObj->inOrderTimestampNode.size());
    auto expectedAddress0 = eventObj->inOrderTimestampNode[0]->getGpuAddress();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto walkerItor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);
        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*walkerItor);

        auto &postSync2 = walkerCmd->getPostSyncOpn2();
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync2.getOperation());
        EXPECT_EQ(expectedAddress0, postSync2.getDestinationAddress());
    }

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);
    ASSERT_EQ(2u, eventObj->inOrderTimestampNode.size());
    auto expectedAddress1 = eventObj->inOrderTimestampNode[1]->getGpuAddress();

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        auto walkerItor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), walkerItor);
        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*walkerItor);

        auto &postSync2 = walkerCmd->getPostSyncOpn2();
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION::OPERATION_WRITE_TIMESTAMP, postSync2.getOperation());
        EXPECT_EQ(expectedAddress1, postSync2.getDestinationAddress());
    }

    context->freeMem(devAddress);
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenInterruptModeEnabledWhenDispatchingWalkerWithRegularEventAndNonInOrderCmdListThenSetPostSyncInterruptEnabled, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using MI_USER_INTERRUPT = typename FamilyType::MI_USER_INTERRUPT;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(1, false);

    events[0]->makeCounterBasedImplicitlyDisabled(eventPool->getAllocation());
    EXPECT_FALSE(events[0]->isInterruptModeEnabled());

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    immCmdList->inOrderExecInfo.reset();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();
        EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
    }

    offset = cmdStream->getUsed();
    events[0]->enableInterruptMode();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        auto compactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

        EXPECT_NE(compactEvent, postSyncData.getInterruptSignalEnable());

        if (compactEvent) {
            itor++; // skip walker
            itor++; // event post sync

            auto interruptItor = find<MI_USER_INTERRUPT *>(itor, commands.end());

            ASSERT_NE(interruptItor, commands.end());

            auto interruptCmd = genCmdCast<MI_USER_INTERRUPT *>(*interruptItor);
            EXPECT_NE(nullptr, interruptCmd);
        } else {
            auto interruptItor = find<MI_USER_INTERRUPT *>(itor, commands.end());

            EXPECT_EQ(interruptItor, commands.end());
        }
    }
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenAtomicSignallingEnabledWhenDispatchingWalkerThenSetCorrectPostSyncFields, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);
    debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto compactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()));

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData.getOperation());
            EXPECT_EQ(1u, postSyncData.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
            EXPECT_TRUE(postSyncData.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn1().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);

    immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    cmdStream = immCmdList->getCmdContainer().getCommandStream();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();
        if (compactEvent) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSyncData.getOperation());
            EXPECT_EQ(POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_INC8B, postSyncData.getAtomicOpcode());
            EXPECT_EQ(POSTSYNC_DATA_2::ATOMIC_DATA_SIZE_QWORD, postSyncData.getAtomicDataSize());
            EXPECT_EQ(0u, postSyncData.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
            EXPECT_TRUE(postSyncData.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn1().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenInterruptEventWhenDispatchingWalkerThenSetCorrectPostSyncFields, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    auto eventPool0 = createEvents<FamilyType>(1, false);
    auto eventPool1 = createEvents<FamilyType>(1, true);
    events[0]->enableInterruptMode();
    events[1]->enableInterruptMode();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto compactEvent0 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));
    auto compactEvent1 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[1]->isSignalScope()));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
            EXPECT_TRUE(postSyncData.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData.getSerializePostsyncOps());
            EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn1().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(-1);
    immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    cmdStream = immCmdList->getCmdContainer().getCommandStream();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData.getSerializePostsyncOps());
            EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
        }

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData1.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), postSyncData1.getDestinationAddress());
            EXPECT_TRUE(postSyncData1.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData1.getSerializePostsyncOps());
            EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }

    offset = cmdStream->getUsed();
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_TIMESTAMP, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData.getSerializePostsyncOps());
            EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
        }

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData1.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), postSyncData1.getDestinationAddress());
            EXPECT_TRUE(postSyncData1.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData1.getSerializePostsyncOps());
            EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
        }

        auto &postSyncData2 = walkerCmd->getPostSyncOpn2();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData2.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_TIMESTAMP, postSyncData2.getOperation());
            EXPECT_EQ(events[1]->getPacketAddress(device), postSyncData2.getDestinationAddress());
            EXPECT_FALSE(postSyncData2.getInterruptSignalEnable());
            EXPECT_FALSE(postSyncData2.getSerializePostsyncOps());
            EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenRegularEventWhenDispatchingWalkerThenSetCorrectPostSyncFields, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);
    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    auto eventPool0 = createEvents<FamilyType>(1, false);
    events[0]->makeCounterBasedImplicitlyDisabled(eventPool0->getAllocation());

    auto eventPool1 = createEvents<FamilyType>(1, true);
    events[1]->makeCounterBasedImplicitlyDisabled(eventPool1->getAllocation());

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto compactEvent0 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));
    auto compactEvent1 = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[1]->isSignalScope()));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData.getOperation());
            EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), postSyncData.getImmediateData());
            EXPECT_EQ(events[0]->getPacketAddress(device), postSyncData.getDestinationAddress());
            EXPECT_TRUE(postSyncData.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn1().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(-1);
    immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();
    cmdStream = immCmdList->getCmdContainer().getCommandStream();

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
        }

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData1.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), postSyncData1.getDestinationAddress());
        }

        auto &postSyncData2 = walkerCmd->getPostSyncOpn2();

        if (compactEvent0) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData2.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData2.getOperation());
            EXPECT_EQ(static_cast<uint64_t>(Event::STATE_SIGNALED), postSyncData2.getImmediateData());
            EXPECT_EQ(events[0]->getPacketAddress(device), postSyncData2.getDestinationAddress());
            EXPECT_TRUE(postSyncData2.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData2.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData2.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData2.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }

    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
        }

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData1.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), postSyncData1.getDestinationAddress());
        }

        auto &postSyncData2 = walkerCmd->getPostSyncOpn2();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData2.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_TIMESTAMP, postSyncData2.getOperation());
            EXPECT_EQ(events[1]->getPacketAddress(device), postSyncData2.getDestinationAddress());
            EXPECT_TRUE(postSyncData2.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData2.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData2.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData2.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    offset = cmdStream->getUsed();
    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[1]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData2 = walkerCmd->getPostSyncOpn2();

        if (compactEvent1) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData2.getOperation());
        } else {
            EXPECT_FALSE(postSyncData2.getDataportPipelineFlush());
            EXPECT_FALSE(postSyncData2.getDataportSubsliceCacheFlush());
        }
    }
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(-1);
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenDuplicatedHostStorageEnabledWhenDispatchingWalkerThenSetCorrectPostSyncFields, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    DebugManagerStateRestore restorer;

    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto eventPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto compactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isFlushRequiredForSignal()));

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();

        if (compactEvent) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData.getOperation());
        } else {
            EXPECT_EQ(immCmdList->inOrderExecInfo->getBaseDeviceAddress(), postSyncData.getDestinationAddress());
        }

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        if (compactEvent) {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, postSyncData1.getOperation());
        } else {
            EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData1.getOperation());
            EXPECT_EQ(1u, postSyncData1.getImmediateData());
            EXPECT_EQ(immCmdList->inOrderExecInfo->getHostCounterAllocation()->getGpuAddress(), postSyncData1.getDestinationAddress());
            EXPECT_TRUE(postSyncData1.getDataportPipelineFlush());
            EXPECT_TRUE(postSyncData1.getDataportSubsliceCacheFlush());
            EXPECT_FALSE(postSyncData.getInterruptSignalEnable());
            EXPECT_EQ(device->getNEODevice()->getGmmHelper()->getL3EnabledMOCS(), postSyncData1.getMocs());
        }

        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn2().getOperation());
        EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_NO_WRITE, walkerCmd->getPostSyncOpn3().getOperation());
    }
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenDebugFlagSetWhenSettingPostSyncsThenEnableSerialization, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    debugManager.flags.SerializeWalkerPostSyncOps.set(1);

    auto eventPool = createEvents<FamilyType>(1, false);

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();

    auto offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, events[0]->toHandle(), 0, nullptr, launchParams);

    auto compactEvent = immCmdList->compactL3FlushEvent(immCmdList->getDcFlushRequired(events[0]->isSignalScope()));

    {
        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &postSyncData = walkerCmd->getPostSync();
        EXPECT_NE(compactEvent, postSyncData.getSerializePostsyncOps());

        auto &postSyncData1 = walkerCmd->getPostSyncOpn1();

        EXPECT_NE(compactEvent, postSyncData1.getSerializePostsyncOps());
    }
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenDebugFlagAndAtomicSignalingAndDuplicatedHostStorageDisabledwhenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // Walker

    auto walkerFromContainer = genCmdCast<DefaultWalkerType *>(regularCmdList->inOrderPatchCmds[0].cmd1);
    ASSERT_NE(nullptr, walkerFromContainer);
    DefaultWalkerType *walkerFromParser = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser = genCmdCast<DefaultWalkerType *>(*itor);
    }

    EXPECT_EQ(1u, regularCmdList->inOrderExecInfo->getCounterValue());

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        EXPECT_EQ(1u + appendValue, walkerFromContainer->getPostSync().getImmediateData());
        EXPECT_EQ(1u + appendValue, walkerFromParser->getPostSync().getImmediateData());

        EXPECT_EQ(0u, walkerFromContainer->getPostSyncOpn1().getImmediateData());
        EXPECT_EQ(0u, walkerFromParser->getPostSyncOpn1().getImmediateData());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(2);
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenAtomicSignalingAndDuplicatedHostStorageDisabledwhenUsingRegularCmdListThenDontAddWalkerToPatch, IsAtLeastXe3pCore) {
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(0u, regularCmdList->inOrderPatchCmds.size());
}

struct MultiTileInOrderCmdListTestsXe3pCoreAndLater : public InOrderCmdListTestsXe3pCoreAndLater {
    void SetUp() override {
        NEO::debugManager.flags.CreateMultipleSubDevices.set(partitionCount);
        NEO::debugManager.flags.EnableImplicitScaling.set(1);

        InOrderCmdListTestsXe3pCoreAndLater::SetUp();
    }

    const uint32_t partitionCount = 2;
};

HWTEST2_F(MultiTileInOrderCmdListTestsXe3pCoreAndLater, givenExternalSyncEventWhenAppendCalledThenProgramIncOperation, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    uint64_t counterValue = 4;
    uint64_t incValue = 2 * partitionCount;
    uint64_t programmedIncValue = incValue / partitionCount;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    auto handle = eventObj->toHandle();
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo());

    auto inOrderExecInfo = eventObj->getInOrderExecInfo();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);

    auto cmdListCounterAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();
    auto incAddress = castToUint64(devAddress);

    EXPECT_NE(cmdListCounterAddress, incAddress);
    EXPECT_EQ(cmdListCounterAddress, walkerCmd->getPostSync().getDestinationAddress());

    auto postSync = &walkerCmd->getPostSyncOpn1();
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        postSync = &walkerCmd->getPostSyncOpn2();
    }

    EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSync->getOperation());
    EXPECT_EQ(POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, postSync->getAtomicOpcode());
    EXPECT_EQ(programmedIncValue, postSync->getImmediateData());
    EXPECT_EQ(incAddress, postSync->getDestinationAddress());

    context->freeMem(devAddress);
}

HWTEST2_F(MultiTileInOrderCmdListTestsXe3pCoreAndLater, givenSyncDispatchEnabledWhenAppendingInitSectionThenProgramSemaphore, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using QUEUE_SWITCH_MODE = typename MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE;

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = partitionCount;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();
    regularCmdList->enableSynchronizedDispatch(NEO::SynchronizedDispatchMode::limited);

    regularCmdList->appendSynchronizedDispatchInitializationSection();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, semaphores.size());

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphores[0]);
    EXPECT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenDebugFlagAndAtomicSignalingEnableAndDuplicatedHostStorageDisabledwhenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // Walker

    auto walkerFromContainer = genCmdCast<DefaultWalkerType *>(regularCmdList->inOrderPatchCmds[0].cmd1);
    ASSERT_NE(nullptr, walkerFromContainer);
    DefaultWalkerType *walkerFromParser = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser = genCmdCast<DefaultWalkerType *>(*itor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    auto verifyPatching = [&]() {
        EXPECT_EQ(0u, walkerFromContainer->getPostSync().getImmediateData());
        EXPECT_EQ(0u, walkerFromParser->getPostSync().getImmediateData());

        EXPECT_EQ(0u, walkerFromContainer->getPostSyncOpn1().getImmediateData());
        EXPECT_EQ(0u, walkerFromParser->getPostSyncOpn1().getImmediateData());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching();
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenAtomicSignalingEnableAndDuplicatedHostStorageDisabledwhenUsingRegularCmdListThenDontAddWalkerToPatch, IsAtLeastXe3pCore) {
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    NEO::debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(0u, regularCmdList->inOrderPatchCmds.size()); // Walker
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenDebugFlagAndAtomicSignalingAndDuplicatedHostStorageEnabledwhenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    auto cmdStream = regularCmdList->getCmdContainer().getCommandStream();

    size_t offset = cmdStream->getUsed();

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(1u, regularCmdList->inOrderPatchCmds.size()); // Walker

    auto walkerFromContainer = genCmdCast<DefaultWalkerType *>(regularCmdList->inOrderPatchCmds[0].cmd1);
    ASSERT_NE(nullptr, walkerFromContainer);
    DefaultWalkerType *walkerFromParser = nullptr;

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          ptrOffset(cmdStream->getCpuBase(), offset),
                                                          (cmdStream->getUsed() - offset)));

        auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        walkerFromParser = genCmdCast<DefaultWalkerType *>(*itor);
    }

    EXPECT_EQ(2u, regularCmdList->inOrderExecInfo->getCounterValue());

    auto verifyPatching = [&](uint64_t executionCounter) {
        auto appendValue = regularCmdList->inOrderExecInfo->getCounterValue() * executionCounter;

        EXPECT_EQ(0u, walkerFromContainer->getPostSync().getImmediateData());
        EXPECT_EQ(0u, walkerFromParser->getPostSync().getImmediateData());

        EXPECT_EQ(2u + appendValue, walkerFromContainer->getPostSyncOpn1().getImmediateData());
        EXPECT_EQ(2u + appendValue, walkerFromParser->getPostSyncOpn1().getImmediateData());
    };

    regularCmdList->close();

    auto handle = regularCmdList->toHandle();

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(0);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(1);

    mockCmdQHw->executeCommandLists(1, &handle, nullptr, false, nullptr, nullptr);
    verifyPatching(2);
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenAtomicSignalingAndDuplicatedHostStorageEnabledwhenUsingRegularCmdListThenAddWalkerToPatch, IsAtLeastXe3pCore) {
    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t desc = {};

    auto mockCmdQHw = makeZeUniquePtr<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQHw->initialize(true, false, false);
    auto regularCmdList = createRegularCmdList<FamilyType::gfxCoreFamily>(false);
    regularCmdList->partitionCount = 2;

    regularCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);

    ASSERT_EQ(0u, regularCmdList->inOrderPatchCmds.size()); // Walker
}

HWTEST2_F(InOrderCmdListTestsXe3pCoreAndLater, givenExternalSyncEventWhenAppendCalledThenProgramIncOperation, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using POSTSYNC_DATA_2 = typename FamilyType::POSTSYNC_DATA_2;

    if (!device->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    uint64_t counterValue = 4;
    uint64_t incValue = 2;

    auto devAddress = reinterpret_cast<uint64_t *>(allocDeviceMem(sizeof(uint64_t) * 2));

    auto eventObj = createExternalSyncStorageEvent(counterValue, incValue, devAddress);
    auto handle = eventObj->toHandle();
    ASSERT_NE(nullptr, eventObj->getInOrderExecInfo());

    auto inOrderExecInfo = eventObj->getInOrderExecInfo();

    auto immCmdList = createImmCmdList<FamilyType::gfxCoreFamily>();

    auto cmdStream = immCmdList->getCmdContainer().getCommandStream();
    size_t offset = cmdStream->getUsed();

    immCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, handle, 0, nullptr, launchParams);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), (cmdStream->getUsed() - offset)));

    auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);

    auto cmdListCounterAddress = immCmdList->inOrderExecInfo->getBaseDeviceAddress();
    auto incAddress = castToUint64(devAddress);

    EXPECT_NE(cmdListCounterAddress, incAddress);
    EXPECT_EQ(cmdListCounterAddress, walkerCmd->getPostSync().getDestinationAddress());

    auto postSync = &walkerCmd->getPostSyncOpn1();
    if (immCmdList->inOrderExecInfo->isHostStorageDuplicated()) {
        postSync = &walkerCmd->getPostSyncOpn2();
    }

    EXPECT_EQ(POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSync->getOperation());
    EXPECT_EQ(POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, postSync->getAtomicOpcode());
    EXPECT_EQ(incValue, postSync->getImmediateData());
    EXPECT_EQ(incAddress, postSync->getDestinationAddress());

    context->freeMem(devAddress);
}

using CommandListScratchPatchPrivateHeapsTestXe3pAndLater = Test<CommandListScratchPatchFixture<0, 0, true>>;
using CommandListScratchPatchGlobalStatelessHeapsTestXe3pAndLater = Test<CommandListScratchPatchFixture<1, 0, true>>;

using CommandListScratchPatchPrivateHeapsStateInitTestXe3pAndLater = Test<CommandListScratchPatchFixture<0, 1, true>>;
using CommandListScratchPatchGlobalStatelessHeapsStateInitTestXe3pAndLater = Test<CommandListScratchPatchFixture<1, 1, true>>;

using CommandListScratchPatchImmediatePrivateHeapsStateInitTestXe3pAndLater = Test<CommandListScratchPatchFixture<0, 1, false>>;
using CommandListScratchPatchImmediateGlobalStatelessHeapsStateInitTestXe3pAndLater = Test<CommandListScratchPatchFixture<1, 1, false>>;

HWTEST2_F(CommandListScratchPatchPrivateHeapsTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnImmediateCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(true, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnImmediateCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(true, false);
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnImmediateCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(true, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnImmediateCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(true, false);
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchPrivateHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchGlobalStatelessHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchEnabledOnRegularCmdListWhenAppendingAndExecutingKernelWithScratchThenExpectCorrectAddressPatched, IsAtLeastXe3pCore) {
    testScratchInline<FamilyType>(false, false);
}

HWTEST2_F(CommandListScratchPatchImmediatePrivateHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchDisabledOnRegularCmdListWhenAppendingAndExecutingKernelThenAddressPatchedAtAppendTimeOnly, IsAtLeastXe3pCore) {
    testScratchImmediatePatching<FamilyType>();
}

HWTEST2_F(CommandListScratchPatchImmediateGlobalStatelessHeapsStateInitTestXe3pAndLater,
          givenHeaplessWithScratchPatchDisabledOnRegularCmdListWhenAppendingAndExecutingKernelThenAddressPatchedAtAppendTimeOnly, IsAtLeastXe3pCore) {
    testScratchImmediatePatching<FamilyType>();
}

using CommandListAppendLaunchKernelXe3pAndLater = Test<ModuleFixture>;

HWTEST2_F(CommandListAppendLaunchKernelXe3pAndLater, givenHeaplessModeWhenAppendingThenIndirectDataPointerAddressIsProgrammedCorrectly, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();
    kernel.immutableData.crossThreadDataSize = sizeof(uint64_t);
    kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.implicitArgsBuffer = 32;
    kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    kernel.immutableData.kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 8;
    kernel.privateState.pImplicitArgs = Clonable(new ImplicitArgs());
    UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(*kernel.immutableData.kernelDescriptor);

    kernel.setGroupSize(1, 1, 1);

    ze_result_t returnValue;
    auto backup = std::unique_ptr<NEO::CompilerProductHelper>{new MockCompilerProductHelperHeapless(true)};
    neoDevice->getRootDeviceEnvironmentRef().compilerProductHelper.swap(backup);
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    device->getNEODevice()->getRootDeviceEnvironmentRef().compilerProductHelper.swap(backup);

    void *alloc = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &alloc);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);

    auto indirectHeap = commandList->getCmdContainer().getIndirectHeap(HeapType::indirectObject);
    auto indirectHeapOffset = indirectHeap->getHeapGpuStartOffset() + indirectHeap->getHeapGpuBase();
    result = commandList->appendLaunchKernelIndirect(kernel.toHandle(),
                                                     *static_cast<ze_group_count_t *>(alloc),
                                                     nullptr, 0, nullptr, false);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itor, cmdList.end());
    DefaultWalkerType *walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);

    const auto &kernelDescriptor = kernel.getKernelDescriptor();
    auto indirectDataPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset;
    auto inlineData = reinterpret_cast<char *>(walkerCmd->getInlineDataPointer());
    uint64_t indirectDataStartAddress = 0;
    memcpy_s(&indirectDataStartAddress, sizeof(indirectDataStartAddress), inlineData + indirectDataPointerAddress, kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize);
    EXPECT_EQ(indirectHeapOffset, indirectDataStartAddress);

    context->freeMem(alloc);
}

} // namespace ult
} // namespace L0
