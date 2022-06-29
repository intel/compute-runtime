/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/source_level_debugger/source_level_debugger_csr_tests.h"

#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"

#include <memory>

HWTEST_F(CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndDisabledPreemptionWhenFlushTaskIsCalledThenSipKernelIsMadeResident) {

    auto mockCsr = createCSR<FamilyType>();
    auto sipType = SipKernel::getSipKernelType(device->getDevice());
    SipKernel::initSipKernel(sipType, device->getDevice());

    CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
    std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

    auto &baseDevice = device->getDevice();

    mockCsr->flushTask(commandStream,
                       0,
                       heap.get(),
                       heap.get(),
                       heap.get(),
                       0,
                       dispatchFlags,
                       baseDevice);

    auto sipAllocation = SipKernel::getSipKernel(baseDevice).getSipAllocation();
    bool found = false;
    for (auto allocation : mockCsr->copyOfAllocations) {
        if (allocation == sipAllocation) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    alignedFree(buffer);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndDisabledPreemptionWhenFlushTaskIsCalledThenStateSipCmdIsProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockCsr = createCSR<FamilyType>();

    if (device->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
        auto &commandStream = commandQueue.getCS(4096u);
        auto &preambleStream = mockCsr->getCS(0);

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        auto &baseDevice = device->getDevice();

        mockCsr->flushTask(commandStream,
                           0,
                           heap.get(),
                           heap.get(),
                           heap.get(),
                           0,
                           dispatchFlags,
                           baseDevice);

        auto sipAllocation = SipKernel::getSipKernel(baseDevice).getSipAllocation();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(preambleStream);
        auto itorStateBaseAddr = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip);

        STATE_BASE_ADDRESS *sba = (STATE_BASE_ADDRESS *)*itorStateBaseAddr;
        STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
        EXPECT_LT(reinterpret_cast<void *>(sba), reinterpret_cast<void *>(stateSipCmd));

        auto sipAddress = stateSipCmd->getSystemInstructionPointer();

        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
        alignedFree(buffer);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndWhenFlushTaskIsCalledThenAlwaysProgramStateBaseAddressAndSip) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockCsr = createCSR<FamilyType>();

    if (device->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

        CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
        auto &commandStream = commandQueue.getCS(4096u);
        auto &preambleStream = mockCsr->getCS(0);

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        auto &baseDevice = device->getDevice();

        mockCsr->flushTask(commandStream,
                           0,
                           heap.get(),
                           heap.get(),
                           heap.get(),
                           0,
                           dispatchFlags,
                           baseDevice);

        mockCsr->flushBatchedSubmissions();

        mockCsr->flushTask(commandStream,
                           0,
                           heap.get(),
                           heap.get(),
                           heap.get(),
                           0,
                           dispatchFlags,
                           baseDevice);

        auto sipAllocation = SipKernel::getSipKernel(baseDevice).getSipAllocation();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(preambleStream);
        auto itorStateBaseAddr = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip);

        auto itorStateBaseAddr2 = find<STATE_BASE_ADDRESS *>(std::next(itorStateBaseAddr), hwParser.cmdList.end());
        auto itorStateSip2 = find<STATE_SIP *>(std::next(itorStateSip), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr2);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip2);

        STATE_BASE_ADDRESS *sba = (STATE_BASE_ADDRESS *)*itorStateBaseAddr2;
        STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip2;
        EXPECT_LT(reinterpret_cast<void *>(sba), reinterpret_cast<void *>(stateSipCmd));

        auto sipAddress = stateSipCmd->getSystemInstructionPointer();

        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
        alignedFree(buffer);
    }
}
