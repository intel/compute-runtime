/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    auto sipAllocation = device->getBuiltIns().getSipKernel(sipType, *device.get()).getSipAllocation();
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
