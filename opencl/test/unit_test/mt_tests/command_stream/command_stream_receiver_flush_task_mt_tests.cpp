/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/task_information.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <thread>
#include <vector>

using namespace NEO;

class MockCommandQueueInitializeBcs : public MockCommandQueue {
  public:
    MockCommandQueueInitializeBcs() : MockCommandQueue(nullptr, nullptr, 0, false) {}
    MockCommandQueueInitializeBcs(Context &context) : MockCommandQueueInitializeBcs(&context, context.getDevice(0), nullptr, false) {}
    MockCommandQueueInitializeBcs(Context *context, ClDevice *device, const cl_queue_properties *props, bool internalUsage)
        : MockCommandQueue(context, device, props, internalUsage) {
    }
    void initializeBcsEngine(bool internalUsage) override {
        if (initializeBcsEngineCalledTimes == 0) {
            auto th = std::thread([&]() {
                isCsrLocked = reinterpret_cast<MockCommandStreamReceiver *>(&this->getGpgpuCommandStreamReceiver())->isOwnershipMutexLocked();
            });
            th.join();
        }
        initializeBcsEngineCalledTimes++;
        MockCommandQueue::initializeBcsEngine(internalUsage);
    }
    int initializeBcsEngineCalledTimes = 0;
    bool isCsrLocked = false;
};

using CommandStreamReceiverFlushTaskMtTestsWithMockCommandStreamReceiver = UltCommandStreamReceiverTestWithCsr<MockCommandStreamReceiver>;

HWTEST_F(CommandStreamReceiverFlushTaskMtTestsWithMockCommandStreamReceiver, GivenBlockedKernelWhenInitializeBcsCalledThenCrsIsNotLocked) {
    MockContext mockContext;
    uint32_t numGrfRequired = 128u;

    auto pCmdQ = std::make_unique<MockCommandQueueInitializeBcs>(&mockContext, pClDevice, nullptr, false);
    auto mockProgram = std::make_unique<MockProgram>(&mockContext, false, toClDeviceVector(*pClDevice));

    auto pKernel = MockKernel::create(*pDevice, mockProgram.get(), numGrfRequired);
    auto kernelInfos = MockKernel::toKernelInfoContainer(pKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(pKernel), kernelInfos);
    auto event = std::make_unique<MockEvent<Event>>(pCmdQ.get(), CL_COMMAND_MARKER, 0, 0);
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 4096, AllocationType::commandBuffer, pDevice->getDeviceBitfield()}));

    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());

    std::vector<Surface *> surfaces;
    event->setCommand(std::make_unique<CommandComputeKernel>(*pCmdQ, blockedCommandsData, surfaces, false, false, false, nullptr, pDevice->getPreemptionMode(), pKernel, 1, nullptr));
    event->submitCommand(false);
    EXPECT_FALSE(pCmdQ->isCsrLocked);
}
