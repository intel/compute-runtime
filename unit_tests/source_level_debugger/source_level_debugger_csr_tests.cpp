/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device.h"

#include "test.h"
#include <memory>

typedef ::testing::Test CommandStreamReceiverWithActiveDebuggerTest;

HWTEST_F(CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndDisabledPreemptionWhenFlushTaskIsCalledThenSipKernelIsMadeResident) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    device->setSourceLevelDebuggerActive(true);
    device->allocatePreemptionAllocationIfNotPresent();
    auto mockCsr = new MockCsrHw2<FamilyType>(*platformDevices[0], *device->executionEnvironment);

    device->resetCommandStreamReceiver(mockCsr);

    CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags;
    dispatchFlags.preemptionMode = PreemptionMode::Disabled;

    void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    std::unique_ptr<GraphicsAllocation> allocation(new GraphicsAllocation(buffer, MemoryConstants::pageSize));
    std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

    mockCsr->flushTask(commandStream,
                       0,
                       *heap.get(),
                       *heap.get(),
                       *heap.get(),
                       0,
                       dispatchFlags,
                       *device);

    auto sipType = SipKernel::getSipKernelType(device->getHardwareInfo().pPlatform->eRenderCoreFamily, true);
    auto sipAllocation = device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, *device.get()).getSipAllocation();
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
