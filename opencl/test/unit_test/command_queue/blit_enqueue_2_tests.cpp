/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/command_queue/blit_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

namespace NEO {

using BlitEnqueueWithDisabledGpgpuSubmissionTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyThenSubmitToGpgpuOnlyIfPreviousEnqueueWasGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenProfilingEnabledWhenSubmittingWithoutFlushToGpgpuThenSetSubmitTime) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;
    mockCommandQueue->setProfilingEnabled();

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    cl_event clEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    auto event = castToObject<Event>(clEvent);

    uint64_t submitTime = 0;
    event->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(submitTime), &submitTime, nullptr);

    EXPECT_NE(0u, submitTime);

    clReleaseEvent(clEvent);
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenOutEventWhenEnqueuingBcsSubmissionThenSetupBcsCsrInEvent) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    {
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(0);

        cl_event clEvent;
        commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
        EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
        EXPECT_EQ(0u, bcsCsr->peekTaskCount());
        EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

        auto event = castToObject<Event>(clEvent);
        EXPECT_EQ(0u, event->peekBcsTaskCountFromCommandQueue());

        clReleaseEvent(clEvent);
    }
    {
        DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

        cl_event clEvent;
        commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, &clEvent);
        EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
        EXPECT_EQ(1u, bcsCsr->peekTaskCount());
        EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

        auto event = castToObject<Event>(clEvent);
        EXPECT_EQ(1u, event->peekBcsTaskCountFromCommandQueue());

        clReleaseEvent(clEvent);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyThenDontSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredAndEnqueueNotFlushedWhenDoingBcsCopyThenSubmitOnlyOnceAfterEnqueue) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);
    DebugManager.flags.PerformImplicitFlushForNewResource.set(0);
    DebugManager.flags.PerformImplicitFlushForIdleGpu.set(0);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;
    mockCommandQueue->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCommandQueue->getGpgpuCommandStreamReceiver().postInitFlagsSetup();

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenImmediateDispatchCacheFlushNotRequiredAndEnqueueNotFlushedWhenDoingBcsCopyThenSubmitOnlyOnceAfterEnqueue) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);
    DebugManager.flags.PerformImplicitFlushForNewResource.set(0);
    DebugManager.flags.PerformImplicitFlushForIdleGpu.set(0);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;
    mockCommandQueue->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
    mockCommandQueue->getGpgpuCommandStreamReceiver().postInitFlagsSetup();

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::GpuKernel, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyAfterBarrierThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    EXPECT_EQ(0u, gpgpuCsr->peekTaskCount());
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueBarrierWithWaitList(0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushNotRequiredWhenDoingBcsCopyOnBlockedQueueThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = false;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);

    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyOnBlockedQueueThenSubmitToGpgpu) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    UserEvent userEvent;
    cl_event waitlist = &userEvent;

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 1, &waitlist, nullptr);
    EXPECT_EQ(EnqueueProperties::Operation::None, mockCommandQueue->latestSentEnqueueType);

    userEvent.setStatus(CL_COMPLETE);
    EXPECT_EQ(EnqueueProperties::Operation::Blit, mockCommandQueue->latestSentEnqueueType);

    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    EXPECT_FALSE(commandQueue->isQueueBlocked());
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenCacheFlushRequiredWhenDoingBcsCopyThatRequiresCacheFlushThenSubmitToGpgpu) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    DebugManager.flags.ForceGpgpuSubmissionForBcsEnqueue.set(-1);

    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.enabled = true;
    mockCommandQueue->overrideIsCacheFlushForBcsRequired.returnValue = true;

    auto buffer = createBuffer(1, false);
    buffer->forceDisallowCPUCopy = true;
    int hostPtr = 0;

    // enqueue kernel to force gpgpu submission on write buffer
    commandQueue->enqueueKernel(mockKernel->mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(1u, gpgpuCsr->peekTaskCount());

    auto offset = mockCommandQueue->getCS(0).getUsed();

    commandQueue->enqueueWriteBuffer(buffer.get(), false, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(2u, gpgpuCsr->peekTaskCount());

    auto cmdListBcs = getCmdList<FamilyType>(bcsCsr->getCS(0), 0);
    auto cmdListQueue = getCmdList<FamilyType>(mockCommandQueue->getCS(0), offset);

    uint64_t cacheFlushWriteAddress = 0;

    {
        auto cmdFound = expectPipeControl<FamilyType>(cmdListQueue.begin(), cmdListQueue.end());
        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*cmdFound);

        EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
        EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
        cacheFlushWriteAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
        EXPECT_NE(0u, cacheFlushWriteAddress);
    }

    {
        auto cmdFound = expectCommand<MI_SEMAPHORE_WAIT>(cmdListBcs.begin(), cmdListBcs.end());
        verifySemaphore<FamilyType>(cmdFound, cacheFlushWriteAddress);

        cmdFound = expectCommand<XY_COPY_BLT>(cmdListBcs.begin(), cmdListBcs.end());
        EXPECT_NE(cmdListBcs.end(), cmdFound);
    }
}

HWTEST_TEMPLATED_F(BlitEnqueueWithDisabledGpgpuSubmissionTests, givenSubmissionToDifferentEngineWhenRequestingForNewTimestmapPacketThenClearDependencies) {
    auto mockCommandQueue = static_cast<MockCommandQueueHw<FamilyType> *>(commandQueue.get());
    const bool clearDependencies = true;

    {
        TimestampPacketContainer previousNodes;
        mockCommandQueue->obtainNewTimestampPacketNodes(1, previousNodes, clearDependencies, *gpgpuCsr); // init
        EXPECT_EQ(0u, previousNodes.peekNodes().size());
    }

    {
        TimestampPacketContainer previousNodes;
        mockCommandQueue->obtainNewTimestampPacketNodes(1, previousNodes, clearDependencies, *bcsCsr);
        EXPECT_EQ(0u, previousNodes.peekNodes().size());
    }
}

using BlitCopyTests = BlitEnqueueTests<1>;

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithoutAllowedCpuAccessThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    if (kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool()) {
        EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());
    } else {
        EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());
    }

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithAllowedCpuAccessThenDontUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(AllocationType::KERNEL_ISA) - 1));

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWhenCreatingWithDisallowedCpuAccessAndDisabledBlitterThenFallbackToCpuCopy) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;

    uint32_t kernelHeap = 0;
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.KernelHeapSize = 1;
    kernelInfo.heapInfo.pKernelHeap = &kernelHeap;

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernelInfo.createKernelAllocation(device->getDevice(), false);

    EXPECT_EQ(initialTaskCount, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenLocalMemoryAccessNotAllowedWhenGlobalConstantsAreExportedThenUseBlitter) {
    DebugManager.flags.EnableLocalMemory.set(1);
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    char constantData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalConstants.initData = constantData;
    programInfo.globalConstants.size = sizeof(constantData);
    auto mockLinkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    programInfo.linkerInput = std::move(mockLinkerInput);

    MockProgram program(bcsMockContext.get(), false, toClDeviceVector(*device));

    EXPECT_EQ(0u, bcsMockContext->bcsCsr->peekTaskCount());

    program.processProgramInfo(programInfo, *device);

    EXPECT_EQ(1u, bcsMockContext->bcsCsr->peekTaskCount());

    auto rootDeviceIndex = device->getRootDeviceIndex();

    ASSERT_NE(nullptr, program.getConstantSurface(rootDeviceIndex));
    auto gpuAddress = reinterpret_cast<const void *>(program.getConstantSurface(rootDeviceIndex)->getGpuAddress());
    EXPECT_NE(nullptr, bcsMockContext->getSVMAllocsManager()->getSVMAlloc(gpuAddress));
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWithoutCpuAccessAllowedWhenSubstituteKernelHeapIsCalledThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

    MockKernelWithInternals kernel(*device);
    const size_t initialHeapSize = 0x40;
    kernel.kernelInfo.heapInfo.KernelHeapSize = initialHeapSize;

    kernel.kernelInfo.createKernelAllocation(device->getDevice(), false);
    ASSERT_NE(nullptr, kernel.kernelInfo.kernelAllocation);
    EXPECT_TRUE(kernel.kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool());

    const size_t newHeapSize = initialHeapSize;
    char newHeap[newHeapSize];

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    kernel.mockKernel->substituteKernelHeap(newHeap, newHeapSize);

    EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());

    device->getMemoryManager()->freeGraphicsMemory(kernel.kernelInfo.kernelAllocation);
}

HWTEST_TEMPLATED_F(BlitCopyTests, givenKernelAllocationInLocalMemoryWithoutCpuAccessAllowedWhenLinkerRequiresPatchingOfInstructionSegmentsThenUseBcsForTransfer) {
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));
    DebugManager.flags.ForceNonSystemMemoryPlacement.set(1 << (static_cast<int64_t>(AllocationType::KERNEL_ISA) - 1));

    device->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    KernelInfo kernelInfo = {};
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.createKernelAllocation(device->getDevice(), false);
    ASSERT_NE(nullptr, kernelInfo.kernelAllocation);
    EXPECT_TRUE(kernelInfo.kernelAllocation->isAllocatedInLocalMemoryPool());

    std::vector<NEO::ExternalFunctionInfo> externalFunctions;
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    program.getKernelInfoArray(device->getRootDeviceIndex()).push_back(&kernelInfo);
    program.setLinkerInput(device->getRootDeviceIndex(), std::move(linkerInput));

    auto initialTaskCount = bcsMockContext->bcsCsr->peekTaskCount();

    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_EQ(initialTaskCount + 1, bcsMockContext->bcsCsr->peekTaskCount());

    program.getKernelInfoArray(device->getRootDeviceIndex()).clear();
    device->getMemoryManager()->freeGraphicsMemory(kernelInfo.kernelAllocation);
}

} // namespace NEO