/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/task_information.h"

#include "core/command_stream/linear_stream.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/string.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_queue/enqueue_common.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/enqueue_properties.h"
#include "runtime/helpers/task_information.inl"
#include "runtime/mem_obj/mem_obj.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/surface.h"

namespace NEO {
template void KernelOperation::ResourceCleaner::operator()<LinearStream>(LinearStream *);
template void KernelOperation::ResourceCleaner::operator()<IndirectHeap>(IndirectHeap *);

CommandMapUnmap::CommandMapUnmap(MapOperationType operationType, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                                 CommandQueue &commandQueue)
    : Command(commandQueue), memObj(memObj), copySize(copySize), copyOffset(copyOffset), readOnly(readOnly), operationType(operationType) {
    memObj.incRefInternal();
}

CompletionStamp &CommandMapUnmap::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        memObj.decRefInternal();
        return completionStamp;
    }

    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
    auto commandStreamReceiverOwnership = commandStreamReceiver.obtainUniqueOwnership();
    auto &queueCommandStream = commandQueue.getCS(0);
    size_t offset = queueCommandStream.getUsed();

    DispatchFlags dispatchFlags(
        {},                                                                          //csrDependencies
        nullptr,                                                                     //barrierTimestampPacketNodes
        {},                                                                          //pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                //flushStampReference
        commandQueue.getThrottle(),                                                  //throttle
        PreemptionHelper::taskPreemptionMode(commandQueue.getDevice(), nullptr),     //preemptionMode
        GrfConfig::DefaultGrfNumber,                                                 //numGrfRequired
        L3CachingSettings::l3CacheOn,                                                //l3CacheSettings
        commandQueue.getSliceCount(),                                                //sliceCount
        true,                                                                        //blocking
        true,                                                                        //dcFlush
        false,                                                                       //useSLM
        true,                                                                        //guardCommandBufferWithPipeControl
        false,                                                                       //GSBA32BitRequired
        false,                                                                       //requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,                            //lowPriority
        false,                                                                       //implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), //outOfOrderExecutionAllowed
        commandQueue.isMultiEngineQueue(),                                           //multiEngineQueue
        false                                                                        //epilogueRequired
    );

    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(queueCommandStream,
                                                      offset,
                                                      commandQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u),
                                                      commandQueue.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u),
                                                      commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    if (!memObj.isMemObjZeroCopy()) {
        commandQueue.waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);
        if (operationType == MAP) {
            memObj.transferDataToHostPtr(copySize, copyOffset);
        } else if (!readOnly) {
            DEBUG_BREAK_IF(operationType != UNMAP);
            memObj.transferDataFromHostPtr(copySize, copyOffset);
        }
    }

    memObj.decRefInternal();

    return completionStamp;
}

CommandComputeKernel::CommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> &surfaces,
                                           bool flushDC, bool usesSLM, bool ndRangeKernel, std::unique_ptr<PrintfHandler> printfHandler,
                                           PreemptionMode preemptionMode, Kernel *kernel, uint32_t kernelCount)
    : Command(commandQueue, kernelOperation), flushDC(flushDC), slmUsed(usesSLM),
      NDRangeKernel(ndRangeKernel), printfHandler(std::move(printfHandler)), kernel(kernel),
      kernelCount(kernelCount), preemptionMode(preemptionMode) {
    for (auto surface : surfaces) {
        this->surfaces.push_back(surface);
    }
    UNRECOVERABLE_IF(nullptr == this->kernel);
    kernel->incRefInternal();
}

CommandComputeKernel::~CommandComputeKernel() {
    kernel->decRefInternal();
}

CompletionStamp &CommandComputeKernel::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        for (auto surface : surfaces) {
            delete surface;
        }
        surfaces.clear();
        return completionStamp;
    }
    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
    bool executionModelKernel = kernel->isParentKernel;
    auto devQueue = commandQueue.getContext().getDefaultDeviceQueue();

    auto commandStreamReceiverOwnership = commandStreamReceiver.obtainUniqueOwnership();

    if (executionModelKernel) {
        while (!devQueue->isEMCriticalSectionFree())
            ;

        devQueue->resetDeviceQueue();
        devQueue->acquireEMCriticalSection();
    }

    IndirectHeap *dsh = kernelOperation->dsh.get();
    IndirectHeap *ioh = kernelOperation->ioh.get();
    IndirectHeap *ssh = kernelOperation->ssh.get();

    auto requiresCoherency = false;
    auto anyUncacheableArgs = false;
    for (auto &surface : surfaces) {
        DEBUG_BREAK_IF(!surface);
        surface->makeResident(commandStreamReceiver);
        requiresCoherency |= surface->IsCoherent;
        if (!surface->allowsL3Caching()) {
            anyUncacheableArgs = true;
        }
    }

    if (printfHandler) {
        printfHandler.get()->makeResident(commandStreamReceiver);
    }
    makeTimestampPacketsResident(commandStreamReceiver);

    if (executionModelKernel) {
        uint32_t taskCount = commandStreamReceiver.peekTaskCount() + 1;
        devQueue->setupExecutionModelDispatch(*ssh, *dsh, kernel, kernelCount,
                                              commandStreamReceiver.getTagAllocation()->getGpuAddress(), taskCount, timestamp);

        BuiltIns &builtIns = *this->kernel->getDevice().getExecutionEnvironment()->getBuiltIns();
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
            *kernelOperation->commandStream,
            scheduler,
            preemptionMode,
            ssh,
            dsh);

        scheduler.makeResident(commandStreamReceiver);

        // Update SLM usage
        slmUsed |= scheduler.slmTotalSize > 0;

        this->kernel->getProgram()->getBlockKernelManager()->makeInternalAllocationsResident(commandStreamReceiver);
    }

    DispatchFlags dispatchFlags(
        {},                                                                          //csrDependencies
        nullptr,                                                                     //barrierTimestampPacketNodes
        {false, kernel->isVmeKernel()},                                              //pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                //flushStampReference
        commandQueue.getThrottle(),                                                  //throttle
        preemptionMode,                                                              //preemptionMode
        kernel->getKernelInfo().patchInfo.executionEnvironment->NumGRFRequired,      //numGrfRequired
        L3CachingSettings::l3CacheOn,                                                //l3CacheSettings
        commandQueue.getSliceCount(),                                                //sliceCount
        true,                                                                        //blocking
        flushDC,                                                                     //dcFlush
        slmUsed,                                                                     //useSLM
        true,                                                                        //guardCommandBufferWithPipeControl
        NDRangeKernel,                                                               //GSBA32BitRequired
        requiresCoherency,                                                           //requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,                            //lowPriority
        false,                                                                       //implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), //outOfOrderExecutionAllowed
        commandQueue.isMultiEngineQueue(),                                           //multiEngineQueue
        false                                                                        //epilogueRequired
    );

    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        dispatchFlags.csrDependencies.fillFromEventsRequest(eventsRequest, commandStreamReceiver, CsrDependencies::DependenciesType::OutOfCsr);
    }
    dispatchFlags.pipelineSelectArgs.specialPipelineSelectMode = kernel->requiresSpecialPipelineSelectMode();
    if (anyUncacheableArgs) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3CacheOff;
    } else if (!kernel->areStatelessWritesUsed()) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3AndL1On;
    }

    if (commandQueue.dispatchHints != 0) {
        dispatchFlags.engineHints = commandQueue.dispatchHints;
        dispatchFlags.epilogueRequired = true;
    }

    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(*kernelOperation->commandStream,
                                                      0,
                                                      *dsh,
                                                      *ioh,
                                                      *ssh,
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    if (printfHandler) {
        commandQueue.waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);
        printfHandler.get()->printEnqueueOutput();
    }

    for (auto surface : surfaces) {
        delete surface;
    }
    surfaces.clear();

    return completionStamp;
}

void CommandWithoutKernel::dispatchBlitOperation() {
    auto bcsCsr = commandQueue.getBcsCommandStreamReceiver();

    UNRECOVERABLE_IF(kernelOperation->blitPropertiesContainer.size() != 1);
    auto &blitProperties = *kernelOperation->blitPropertiesContainer.begin();
    blitProperties.csrDependencies.fillFromEventsRequest(eventsRequest, *bcsCsr, CsrDependencies::DependenciesType::All);
    blitProperties.csrDependencies.push_back(&timestampPacketDependencies->previousEnqueueNodes);
    blitProperties.csrDependencies.push_back(&timestampPacketDependencies->barrierNodes);
    blitProperties.outputTimestampPacket = currentTimestampPacketNodes->peekNodes()[0];

    auto bcsTaskCount = bcsCsr->blitBuffer(kernelOperation->blitPropertiesContainer, false);

    commandQueue.updateBcsTaskCount(bcsTaskCount);
}

CompletionStamp &CommandWithoutKernel::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        return completionStamp;
    }

    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();

    if (!kernelOperation) {
        completionStamp.taskCount = commandStreamReceiver.peekTaskCount();
        completionStamp.taskLevel = commandStreamReceiver.peekTaskLevel();
        completionStamp.flushStamp = commandStreamReceiver.obtainCurrentFlushStamp();

        return completionStamp;
    }

    auto lockCSR = commandStreamReceiver.obtainUniqueOwnership();

    if (kernelOperation->blitEnqueue) {
        if (commandStreamReceiver.isStallingPipeControlOnNextFlushRequired()) {
            timestampPacketDependencies->barrierNodes.add(commandStreamReceiver.getTimestampPacketAllocator()->getTag());
        }
        dispatchBlitOperation();
    }

    DispatchFlags dispatchFlags(
        {},                                                   //csrDependencies
        &timestampPacketDependencies->barrierNodes,           //barrierTimestampPacketNodes
        {},                                                   //pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),         //flushStampReference
        commandQueue.getThrottle(),                           //throttle
        commandQueue.getDevice().getPreemptionMode(),         //preemptionMode
        GrfConfig::DefaultGrfNumber,                          //numGrfRequired
        L3CachingSettings::l3CacheOn,                         //l3CacheSettings
        commandQueue.getSliceCount(),                         //sliceCount
        true,                                                 //blocking
        false,                                                //dcFlush
        false,                                                //useSLM
        true,                                                 //guardCommandBufferWithPipeControl
        false,                                                //GSBA32BitRequired
        false,                                                //requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,     //lowPriority
        false,                                                //implicitFlush
        commandStreamReceiver.isNTo1SubmissionModelEnabled(), //outOfOrderExecutionAllowed
        commandQueue.isMultiEngineQueue(),                    //multiEngineQueue
        false                                                 //epilogueRequired
    );

    UNRECOVERABLE_IF(!commandStreamReceiver.peekTimestampPacketWriteEnabled());

    dispatchFlags.csrDependencies.fillFromEventsRequest(eventsRequest, commandStreamReceiver, CsrDependencies::DependenciesType::OutOfCsr);

    makeTimestampPacketsResident(commandStreamReceiver);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(*kernelOperation->commandStream,
                                                      0,
                                                      commandQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u),
                                                      commandQueue.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u),
                                                      commandQueue.getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    return completionStamp;
}

void Command::setEventsRequest(EventsRequest &eventsRequest) {
    this->eventsRequest = eventsRequest;
    if (eventsRequest.numEventsInWaitList > 0) {
        eventsWaitlist.resize(eventsRequest.numEventsInWaitList);
        auto size = eventsRequest.numEventsInWaitList * sizeof(cl_event);
        memcpy_s(&eventsWaitlist[0], size, eventsRequest.eventWaitList, size);
        this->eventsRequest.eventWaitList = &eventsWaitlist[0];
    }
}

void Command::setTimestampPacketNode(TimestampPacketContainer &current, TimestampPacketDependencies &&dependencies) {
    currentTimestampPacketNodes = std::make_unique<TimestampPacketContainer>();
    currentTimestampPacketNodes->assignAndIncrementNodesRefCounts(current);

    timestampPacketDependencies = std::make_unique<TimestampPacketDependencies>();
    *timestampPacketDependencies = std::move(dependencies);
}

Command::~Command() {
    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        for (cl_event &eventFromWaitList : eventsWaitlist) {
            auto event = castToObjectOrAbort<Event>(eventFromWaitList);
            event->decRefInternal();
        }
    }
}

void Command::makeTimestampPacketsResident(CommandStreamReceiver &commandStreamReceiver) {
    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        for (cl_event &eventFromWaitList : eventsWaitlist) {
            auto event = castToObjectOrAbort<Event>(eventFromWaitList);
            if (event->getTimestampPacketNodes()) {
                event->getTimestampPacketNodes()->makeResident(commandStreamReceiver);
            }
        }
    }

    if (currentTimestampPacketNodes) {
        currentTimestampPacketNodes->makeResident(commandStreamReceiver);
    }
    if (timestampPacketDependencies) {
        timestampPacketDependencies->previousEnqueueNodes.makeResident(commandStreamReceiver);
    }
}

Command::Command(CommandQueue &commandQueue) : commandQueue(commandQueue) {}

Command::Command(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation)
    : commandQueue(commandQueue), kernelOperation(std::move(kernelOperation)) {}
} // namespace NEO
