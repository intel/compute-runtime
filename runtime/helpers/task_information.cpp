/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/task_information.h"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"

namespace OCLRT {
KernelOperation::~KernelOperation() {
    memoryManager.storeAllocation(std::unique_ptr<GraphicsAllocation>(dsh->getGraphicsAllocation()), REUSABLE_ALLOCATION);
    if (ioh.get() == dsh.get()) {
        ioh.release();
    }
    if (ioh) {
        memoryManager.storeAllocation(std::unique_ptr<GraphicsAllocation>(ioh->getGraphicsAllocation()), REUSABLE_ALLOCATION);
    }
    memoryManager.storeAllocation(std::unique_ptr<GraphicsAllocation>(ssh->getGraphicsAllocation()), REUSABLE_ALLOCATION);
    alignedFree(commandStream->getCpuBase());
}

CommandMapUnmap::CommandMapUnmap(MapOperationType op, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                                 CommandStreamReceiver &csr, CommandQueue &cmdQ)
    : memObj(memObj), copySize(copySize), copyOffset(copyOffset), readOnly(readOnly), csr(csr), cmdQ(cmdQ), op(op) {
    memObj.incRefInternal();
}

CommandMapUnmap::~CommandMapUnmap() {
    memObj.decRefInternal();
}

CompletionStamp &CommandMapUnmap::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        return completionStamp;
    }

    bool blocking = true;
    TakeOwnershipWrapper<Device> deviceOwnership(cmdQ.getDevice());

    auto &queueCommandStream = cmdQ.getCS(0);
    size_t offset = queueCommandStream.getUsed();

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = blocking;
    dispatchFlags.dcFlush = true;
    dispatchFlags.useSLM = true;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.lowPriority = cmdQ.getPriority() == QueuePriority::LOW;
    dispatchFlags.throttle = cmdQ.getThrottle();
    dispatchFlags.preemptionMode = PreemptionHelper::taskPreemptionMode(cmdQ.getDevice(), nullptr);

    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    gtpinNotifyPreFlushTask(&cmdQ);

    completionStamp = csr.flushTask(queueCommandStream,
                                    offset,
                                    cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u),
                                    cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u),
                                    cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
                                    taskLevel,
                                    dispatchFlags);

    cmdQ.waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);

    if (!memObj.isMemObjZeroCopy()) {
        if (op == MAP) {
            memObj.transferDataToHostPtr(copySize, copyOffset);
        } else if (!readOnly) {
            DEBUG_BREAK_IF(op != UNMAP);
            memObj.transferDataFromHostPtr(copySize, copyOffset);
        }
    }

    return completionStamp;
}

CommandComputeKernel::CommandComputeKernel(CommandQueue &commandQueue, CommandStreamReceiver &commandStreamReceiver,
                                           std::unique_ptr<KernelOperation> kernelOperation, std::vector<Surface *> &surfaces,
                                           bool flushDC, bool usesSLM, bool ndRangeKernel, std::unique_ptr<PrintfHandler> printfHandler,
                                           PreemptionMode preemptionMode, Kernel *kernel, uint32_t kernelCount)
    : commandQueue(commandQueue),
      commandStreamReceiver(commandStreamReceiver),
      kernelOperation(std::move(kernelOperation)),
      flushDC(flushDC),
      slmUsed(usesSLM),
      NDRangeKernel(ndRangeKernel),
      printfHandler(std::move(printfHandler)),
      kernel(nullptr),
      kernelCount(0) {
    for (auto surface : surfaces) {
        this->surfaces.push_back(surface);
    }
    this->kernel = kernel;
    UNRECOVERABLE_IF(nullptr == this->kernel);
    kernel->incRefInternal();
    this->kernelCount = kernelCount;
    this->preemptionMode = preemptionMode;
}

CommandComputeKernel::~CommandComputeKernel() {
    for (auto surface : surfaces) {
        delete surface;
    }
    surfaces.clear();
    if (kernelOperation->ioh.get() == kernelOperation->dsh.get()) {
        kernelOperation->doNotFreeISH = true;
    }
    kernel->decRefInternal();
}

CompletionStamp &CommandComputeKernel::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        return completionStamp;
    }
    bool executionModelKernel = kernel->isParentKernel;
    auto devQueue = commandQueue.getContext().getDefaultDeviceQueue();

    TakeOwnershipWrapper<Device> deviceOwnership(commandQueue.getDevice());

    if (executionModelKernel) {
        while (!devQueue->isEMCriticalSectionFree())
            ;

        devQueue->resetDeviceQueue();
        devQueue->acquireEMCriticalSection();
    }

    auto &commandStream = *kernelOperation->commandStream;
    size_t commandsSize = commandStream.getUsed();
    auto &queueCommandStream = commandQueue.getCS(commandStream.getUsed());
    size_t offset = queueCommandStream.getUsed();
    void *pDst = queueCommandStream.getSpace(commandsSize);
    //transfer the memory to commandStream of the queue.
    memcpy_s(pDst, commandsSize, commandStream.getCpuBase(), commandsSize);

    IndirectHeap *dsh = kernelOperation->dsh.get();
    IndirectHeap *ioh = kernelOperation->ioh.get();
    IndirectHeap *ssh = kernelOperation->ssh.get();

    auto requiresCoherency = false;
    for (auto &surface : surfaces) {
        DEBUG_BREAK_IF(!surface);
        surface->makeResident(commandStreamReceiver);
        requiresCoherency |= surface->IsCoherent;
    }

    if (printfHandler) {
        printfHandler.get()->makeResident(commandStreamReceiver);
    }

    if (executionModelKernel) {
        uint32_t taskCount = commandStreamReceiver.peekTaskCount() + 1;
        devQueue->setupExecutionModelDispatch(*ssh, *dsh, kernel, kernelCount, taskCount, timestamp);

        BuiltIns &builtIns = BuiltIns::getInstance();
        SchedulerKernel &scheduler = builtIns.getSchedulerKernel(commandQueue.getContext());

        scheduler.setArgs(devQueue->getQueueBuffer(),
                          devQueue->getStackBuffer(),
                          devQueue->getEventPoolBuffer(),
                          devQueue->getSlbBuffer(),
                          dsh->getGraphicsAllocation(),
                          kernel->getKernelReflectionSurface(),
                          devQueue->getQueueStorageBuffer(),
                          ssh->getGraphicsAllocation(),
                          devQueue->getDebugQueue());

        devQueue->dispatchScheduler(
            commandQueue,
            scheduler,
            preemptionMode,
            ssh,
            dsh);

        scheduler.makeResident(commandStreamReceiver);

        // Update SLM usage
        slmUsed |= scheduler.slmTotalSize > 0;

        this->kernel->getProgram()->getBlockKernelManager()->makeInternalAllocationsResident(commandStreamReceiver);
    }

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = true;
    dispatchFlags.dcFlush = flushDC;
    dispatchFlags.useSLM = slmUsed;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.GSBA32BitRequired = NDRangeKernel;
    dispatchFlags.requiresCoherency = requiresCoherency;
    dispatchFlags.lowPriority = commandQueue.getPriority() == QueuePriority::LOW;
    dispatchFlags.throttle = commandQueue.getThrottle();
    dispatchFlags.preemptionMode = preemptionMode;
    dispatchFlags.mediaSamplerRequired = kernel->isVmeKernel();

    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(queueCommandStream,
                                                      offset,
                                                      *dsh,
                                                      *ioh,
                                                      *ssh,
                                                      taskLevel,
                                                      dispatchFlags);
    for (auto &surface : surfaces) {
        surface->setCompletionStamp(completionStamp, nullptr, nullptr);
    }
    commandQueue.waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);
    if (printfHandler) {
        printfHandler.get()->printEnqueueOutput();
    }

    return completionStamp;
}

CompletionStamp &CommandMarker::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        return completionStamp;
    }

    bool blocking = true;
    TakeOwnershipWrapper<Device> deviceOwnership(cmdQ.getDevice());

    auto &queueCommandStream = cmdQ.getCS(this->commandSize);
    size_t offset = queueCommandStream.getUsed();

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = blocking;
    dispatchFlags.dcFlush = shouldFlushDC(clCommandType, nullptr);
    dispatchFlags.lowPriority = cmdQ.getPriority() == QueuePriority::LOW;
    dispatchFlags.throttle = cmdQ.getThrottle();
    dispatchFlags.preemptionMode = PreemptionHelper::taskPreemptionMode(cmdQ.getDevice(), nullptr);

    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    gtpinNotifyPreFlushTask(&cmdQ);

    completionStamp = csr.flushTask(queueCommandStream,
                                    offset,
                                    cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u),
                                    cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u),
                                    cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
                                    taskLevel,
                                    dispatchFlags);

    cmdQ.waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);

    return completionStamp;
}
} // namespace OCLRT
