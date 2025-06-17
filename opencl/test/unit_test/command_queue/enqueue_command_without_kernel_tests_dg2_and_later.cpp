/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/memory_manager/resource_surface.h"
#include "opencl/test/unit_test/fixtures/dispatch_flags_fixture.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

namespace NEO {
using IsDG2AndLater = IsAtLeastXeCore;

HWTEST2_F(DispatchFlagsTests, whenSubmittingKernelWithAdditionalKernelExecInfoThenCorrectDispatchFlagIsSet, IsDG2AndLater) {

    UnitTestSetter::disableHeapless(this->restore);
    using CsrType = MockCsrHw2<FamilyType>;
    setUpImpl<CsrType>();

    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), nullptr);
    auto mockCsr = static_cast<CsrType *>(&mockCmdQ->getGpgpuCommandStreamReceiver());

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(0, nullptr, nullptr);
    EventBuilder eventBuilder;

    EnqueueProperties enqueueProperties(false, false, false, true, false, false, nullptr);

    auto cmdStream = new LinearStream(device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, AllocationType::commandBuffer, device->getDeviceBitfield()}));
    auto blockedCommandsData = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals mockKernelWithInternals(*device.get());
    auto pKernel = mockKernelWithInternals.mockKernel;
    MockMultiDispatchInfo multiDispatchInfo(device.get(), pKernel);

    std::unique_ptr<PrintfHandler> printfHandler(PrintfHandler::create(multiDispatchInfo, device->getDevice()));
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::dynamicState, 4096u, dsh);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::indirectObject, 4096u, ioh);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::surfaceState, 4096u, ssh);
    blockedCommandsData->setHeaps(dsh, ioh, ssh);
    std::vector<Surface *> v;

    pKernel->setAdditionalKernelExecInfo(AdditionalKernelExecInfo::disableOverdispatch);
    std::unique_ptr<CommandComputeKernel> cmd(new CommandComputeKernel(*mockCmdQ.get(), blockedCommandsData, v, false, false, false, std::move(printfHandler), PreemptionMode::Disabled, pKernel, 1, nullptr));
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, AdditionalKernelExecInfo::disableOverdispatch);

    pKernel->setAdditionalKernelExecInfo(AdditionalKernelExecInfo::notApplicable);
    mockCsr->setMediaVFEStateDirty(true);
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, AdditionalKernelExecInfo::notApplicable);

    pKernel->setAdditionalKernelExecInfo(AdditionalKernelExecInfo::notSet);
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, AdditionalKernelExecInfo::notSet);
    cmd->submit(1u, false);
    EXPECT_EQ(mockCsr->passedDispatchFlags.additionalKernelExecInfo, AdditionalKernelExecInfo::notSet);
}
} // namespace NEO
