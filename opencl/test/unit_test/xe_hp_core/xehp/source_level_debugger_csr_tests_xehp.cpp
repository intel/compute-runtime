/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/source_level_debugger/source_level_debugger_csr_tests.h"

#include <memory>

using CommandStreamReceiverWithActiveDebuggerXehpTest = CommandStreamReceiverWithActiveDebuggerTest;

XEHPTEST_F(CommandStreamReceiverWithActiveDebuggerXehpTest, GivenASteppingAndActiveDebuggerAndWhenFlushTaskIsCalledThenAlwaysProgramStateBaseAddressAndGlobalSip) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using MI_NOOP = typename FamilyType::MI_NOOP;

    hwInfo = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVID::REVISION_A0, *hwInfo);

    auto mockCsr = createCSR<FamilyType>();
    mockCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

    CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
    auto &commandStream = commandQueue.getCS(4096u);
    auto &csrStream = mockCsr->getCS(0);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
    std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

    auto &neoDevice = device->getDevice();

    mockCsr->flushTask(commandStream,
                       0,
                       heap.get(),
                       heap.get(),
                       heap.get(),
                       0,
                       dispatchFlags,
                       neoDevice);

    mockCsr->flushBatchedSubmissions();

    auto noops = reinterpret_cast<MI_NOOP *>(commandStream.getSpace(8 * sizeof(MI_NOOP)));

    for (int i = 0; i < 8; i++) {
        noops[i] = FamilyType::cmdInitNoop;
    }

    mockCsr->flushTask(commandStream,
                       0,
                       heap.get(),
                       heap.get(),
                       heap.get(),
                       0,
                       dispatchFlags,
                       neoDevice);

    auto sipAllocation = SipKernel::getSipKernel(neoDevice).getSipAllocation();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csrStream);
    hwParser.parseCommands<FamilyType>(commandStream);

    auto itorStateBaseAddr = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto itorBbEnd = find<MI_BATCH_BUFFER_END *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto itorStateBaseAddr2 = find<STATE_BASE_ADDRESS *>(std::next(itorStateBaseAddr), hwParser.cmdList.end());

    ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr);
    ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr2);

    auto itorGlobalSip1 = findMmio<FamilyType>(itorStateBaseAddr, itorBbEnd, GlobalSipRegister<FamilyType>::registerOffset);
    auto itorGlobalSip2 = findMmio<FamilyType>(std::next(itorGlobalSip1), itorBbEnd, GlobalSipRegister<FamilyType>::registerOffset);
    auto itorGlobalSip3 = findMmio<FamilyType>(itorBbEnd, hwParser.cmdList.end(), GlobalSipRegister<FamilyType>::registerOffset);
    auto itorGlobalSip4 = findMmio<FamilyType>(std::next(itorGlobalSip3), hwParser.cmdList.end(), GlobalSipRegister<FamilyType>::registerOffset);

    ASSERT_NE(hwParser.cmdList.end(), itorGlobalSip1);
    ASSERT_NE(hwParser.cmdList.end(), itorGlobalSip2);
    ASSERT_NE(hwParser.cmdList.end(), itorGlobalSip3);
    ASSERT_NE(hwParser.cmdList.end(), itorGlobalSip4);

    EXPECT_NE(itorGlobalSip1, itorGlobalSip2);

    auto expectedSipPosition = --itorBbEnd;
    EXPECT_EQ(expectedSipPosition, itorGlobalSip2);

    auto itorBbEnd2 = find<MI_BATCH_BUFFER_END *>(itorGlobalSip3, hwParser.cmdList.end());

    expectedSipPosition = --itorBbEnd2;
    EXPECT_EQ(expectedSipPosition, itorGlobalSip4);

    MI_LOAD_REGISTER_IMM *globalSip = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorGlobalSip1);

    auto sipAddress = globalSip->getDataDword();
    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress & 0xfffffff8);

    globalSip = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorGlobalSip2);
    auto sipAddress2 = globalSip->getDataDword();
    EXPECT_EQ(0u, sipAddress2);

    globalSip = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorGlobalSip3);

    sipAddress = globalSip->getDataDword();
    EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress & 0xfffffff8);

    globalSip = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorGlobalSip4);
    sipAddress2 = globalSip->getDataDword();
    EXPECT_EQ(0u, sipAddress2);

    alignedFree(buffer);
}
