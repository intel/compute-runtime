/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/hardware_interface.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/event/user_event.h"
#include "runtime/event/event_builder.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/array_count.h"
#include "runtime/helpers/dispatch_info_builder.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/task_information.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/program/printf_handler.h"
#include "runtime/program/block_kernel_manager.h"
#include "runtime/utilities/range.h"
#include "runtime/utilities/tag_allocator.h"
#include <algorithm>
#include <new>

namespace OCLRT {

template <typename GfxFamily>
template <uint32_t commandType, size_t surfaceCount>
void CommandQueueHw<GfxFamily>::enqueueHandler(Surface *(&surfaces)[surfaceCount],
                                               bool blocking,
                                               Kernel *kernel,
                                               cl_uint workDim,
                                               const size_t globalOffsets[3],
                                               const size_t workItems[3],
                                               const size_t *localWorkSizesIn,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    if (kernel == nullptr) {
        enqueueHandler<commandType>(surfaces, blocking, MultiDispatchInfo(), numEventsInWaitList, eventWaitList, event);
    } else {
        BuiltInOwnershipWrapper builtInLock;
        MultiDispatchInfo multiDispatchInfo(kernel);

        if (DebugManager.flags.ForceDispatchScheduler.get()) {
            forceDispatchScheduler(multiDispatchInfo);
        } else {
            BuffersForAuxTranslation buffersForAuxTranslation;
            if (kernel->isAuxTranslationRequired()) {
                auto &builder = getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, getContext(), getDevice());
                builtInLock.takeOwnership(builder, this->context);
                kernel->fillWithBuffersForAuxTranslation(buffersForAuxTranslation);
                dispatchAuxTranslation(multiDispatchInfo, buffersForAuxTranslation, AuxTranslationDirection::AuxToNonAux);
            }

            if (kernel->getKernelInfo().builtinDispatchBuilder == nullptr) {
                DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::WalkerSplit> builder;
                builder.setDispatchGeometry(workDim, workItems, localWorkSizesIn, globalOffsets);
                builder.setKernel(kernel);
                builder.bake(multiDispatchInfo);
            } else {
                auto builder = kernel->getKernelInfo().builtinDispatchBuilder;
                builder->buildDispatchInfos(multiDispatchInfo, kernel, workDim, workItems, localWorkSizesIn, globalOffsets);

                if (multiDispatchInfo.size() == 0) {
                    return;
                }
            }
            if (kernel->isAuxTranslationRequired()) {
                if (kernel->isParentKernel) {
                    for (auto &buffer : buffersForAuxTranslation) {
                        buffer->getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
                    }
                } else {
                    dispatchAuxTranslation(multiDispatchInfo, buffersForAuxTranslation, AuxTranslationDirection::NonAuxToAux);
                }
            }
        }

        enqueueHandler<commandType>(surfaces, blocking, multiDispatchInfo, numEventsInWaitList, eventWaitList, event);
    }
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::forceDispatchScheduler(OCLRT::MultiDispatchInfo &multiDispatchInfo) {
    BuiltIns &builtIns = *getDevice().getExecutionEnvironment()->getBuiltIns();
    SchedulerKernel &scheduler = builtIns.getSchedulerKernel(this->getContext());
    DispatchInfo dispatchInfo(&scheduler, 1, Vec3<size_t>(scheduler.getGws(), 1, 1), Vec3<size_t>(scheduler.getLws(), 1, 1), Vec3<size_t>(0, 0, 0));

    auto devQueue = this->getContext().getDefaultDeviceQueue();
    DeviceQueueHw<GfxFamily> *devQueueHw = castToObject<DeviceQueueHw<GfxFamily>>(devQueue);

    scheduler.createReflectionSurface();
    GraphicsAllocation *reflectionSurface = scheduler.getKernelReflectionSurface();

    devQueueHw->resetDeviceQueue();

    scheduler.setArgs(devQueueHw->getQueueBuffer(),
                      devQueueHw->getStackBuffer(),
                      devQueueHw->getEventPoolBuffer(),
                      devQueueHw->getSlbBuffer(),
                      devQueueHw->getDshBuffer(),
                      reflectionSurface,
                      devQueueHw->getQueueStorageBuffer(),
                      this->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getGraphicsAllocation());

    multiDispatchInfo.push(dispatchInfo);
}

template <typename GfxFamily>
template <uint32_t commandType>
void CommandQueueHw<GfxFamily>::enqueueHandler(Surface **surfacesForResidency,
                                               size_t numSurfaceForResidency,
                                               bool blocking,
                                               const MultiDispatchInfo &multiDispatchInfo,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    if (multiDispatchInfo.empty() && !isCommandWithoutKernel(commandType)) {
        enqueueHandler<CL_COMMAND_MARKER>(surfacesForResidency, numSurfaceForResidency, blocking, multiDispatchInfo,
                                          numEventsInWaitList, eventWaitList, event);
        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(commandType);
        }
        return;
    }

    Kernel *parentKernel = multiDispatchInfo.peekParentKernel();
    auto devQueue = this->getContext().getDefaultDeviceQueue();
    DeviceQueueHw<GfxFamily> *devQueueHw = castToObject<DeviceQueueHw<GfxFamily>>(devQueue);

    HwTimeStamps *hwTimeStamps = nullptr;
    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto commandStreamRecieverOwnership = commandStreamReceiver.obtainUniqueOwnership();

    TimeStampData queueTimeStamp;
    if (isProfilingEnabled() && event) {
        this->getDevice().getOSTime()->getCpuGpuTime(&queueTimeStamp);
    }

    EventBuilder eventBuilder;
    if (event) {
        eventBuilder.create<Event>(this, commandType, Event::eventNotReady, 0);
        *event = eventBuilder.getEvent();
        if (eventBuilder.getEvent()->isProfilingEnabled()) {
            eventBuilder.getEvent()->setQueueTimeStamp(&queueTimeStamp);
            if (isCommandWithoutKernel(commandType)) {
                eventBuilder.getEvent()->setCPUProfilingPath(true);
                eventBuilder.getEvent()->setQueueTimeStamp();
            }
        }
        DBG_LOG(EventsDebugEnable, "enqueueHandler commandType", commandType, "output Event", eventBuilder.getEvent());
    }

    bool profilingRequired = (this->isProfilingEnabled() && event != nullptr);
    bool perfCountersRequired = false;
    perfCountersRequired = (this->isPerfCountersEnabled() && event != nullptr);
    KernelOperation *blockedCommandsData = nullptr;
    std::unique_ptr<PrintfHandler> printfHandler;
    bool slmUsed = false;
    EngineType engineType = device->getEngineType();
    auto preemption = PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo);
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, numEventsInWaitList, eventWaitList, blockQueue, commandType);

    auto &commandStream = getCommandStream<GfxFamily, commandType>(*this, numEventsInWaitList, profilingRequired, perfCountersRequired, multiDispatchInfo);
    auto commandStreamStart = commandStream.getUsed();

    DBG_LOG(EventsDebugEnable, "blockQueue", blockQueue, "virtualEvent", virtualEvent, "taskLevel", taskLevel);

    if (parentKernel && !blockQueue) {
        while (!devQueueHw->isEMCriticalSectionFree())
            ;
    }

    enqueueHandlerHook(commandType, multiDispatchInfo);

    if (DebugManager.flags.AUBDumpSubCaptureMode.get()) {
        commandStreamReceiver.activateAubSubCapture(multiDispatchInfo);
    }

    if (DebugManager.flags.MakeEachEnqueueBlocking.get()) {
        blocking = true;
    }

    TimestampPacketContainer previousTimestampPacketNodes(device->getMemoryManager());
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    if (multiDispatchInfo.empty() == false) {
        HwPerfCounter *hwPerfCounter = nullptr;
        DebugManager.dumpKernelArgs(&multiDispatchInfo);

        printfHandler.reset(PrintfHandler::create(multiDispatchInfo, *device));
        if (printfHandler) {
            printfHandler.get()->prepareDispatch(multiDispatchInfo);
        }

        if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
            if (multiDispatchInfo.peekMainKernel()->getProgram()->isKernelDebugEnabled()) {
                setupDebugSurface(multiDispatchInfo.peekMainKernel());
            }
        }

        if (device->getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            obtainNewTimestampPacketNodes(multiDispatchInfo.size(), previousTimestampPacketNodes);
        }

        if (eventBuilder.getEvent()) {
            if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
                eventBuilder.getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
            }
            if (this->isProfilingEnabled()) {
                // Get allocation for timestamps
                hwTimeStamps = eventBuilder.getEvent()->getHwTimeStamp();
                if (this->isPerfCountersEnabled()) {
                    hwPerfCounter = eventBuilder.getEvent()->getHwPerfCounter();
                    // PERF COUNTER: copy current configuration from queue to event
                    eventBuilder.getEvent()->copyPerfCounters(this->getPerfCountersConfigData());
                }
            }
        }

        if (parentKernel) {
            parentKernel->createReflectionSurface();
            parentKernel->patchDefaultDeviceQueue(context->getDefaultDeviceQueue());
            parentKernel->patchEventPool(context->getDefaultDeviceQueue());
            parentKernel->patchReflectionSurface(context->getDefaultDeviceQueue(), printfHandler.get());
            if (!blockQueue) {
                devQueueHw->resetDeviceQueue();
                devQueueHw->acquireEMCriticalSection();
            }
        }

        HardwareInterface<GfxFamily>::dispatchWalker(
            *this,
            multiDispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            &blockedCommandsData,
            hwTimeStamps,
            hwPerfCounter,
            &previousTimestampPacketNodes,
            timestampPacketContainer.get(),
            preemption,
            blockQueue,
            commandType);

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            for (auto &dispatchInfo : multiDispatchInfo) {
                for (auto &patchInfoData : dispatchInfo.getKernel()->getPatchInfoDataList()) {
                    commandStreamReceiver.getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
                }
            }
        }

        commandStreamReceiver.setRequiredScratchSize(multiDispatchInfo.getRequiredScratchSize());

        slmUsed = multiDispatchInfo.usesSlm();
    } else if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        if (CL_COMMAND_BARRIER == commandType) {
            commandStreamReceiver.requestStallingPipeControlOnNextFlush();
        }
        if (eventBuilder.getEvent()) {
            // Event from non-kernel enqueue inherits TimestampPackets from waitlist and command queue
            eventBuilder.getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
            for (size_t i = 0; i < eventsRequest.numEventsInWaitList; i++) {
                auto waitlistEvent = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
                if (!waitlistEvent->isUserEvent()) {
                    eventBuilder.getEvent()->addTimestampPacketNodes(*waitlistEvent->getTimestampPacketNodes());
                }
            }
        }
    }

    CompletionStamp completionStamp;
    if (!blockQueue) {
        if (parentKernel) {
            size_t minSizeSSHForEM = KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(const_cast<const Kernel &>(*parentKernel));

            uint32_t taskCount = commandStreamReceiver.peekTaskCount() + 1;
            devQueueHw->setupExecutionModelDispatch(getIndirectHeap(IndirectHeap::SURFACE_STATE, minSizeSSHForEM),
                                                    *devQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE),
                                                    parentKernel,
                                                    (uint32_t)multiDispatchInfo.size(),
                                                    taskCount,
                                                    hwTimeStamps);

            BuiltIns &builtIns = *getDevice().getExecutionEnvironment()->getBuiltIns();
            SchedulerKernel &scheduler = builtIns.getSchedulerKernel(this->getContext());

            scheduler.setArgs(devQueueHw->getQueueBuffer(),
                              devQueueHw->getStackBuffer(),
                              devQueueHw->getEventPoolBuffer(),
                              devQueueHw->getSlbBuffer(),
                              devQueueHw->getDshBuffer(),
                              parentKernel->getKernelReflectionSurface(),
                              devQueueHw->getQueueStorageBuffer(),
                              this->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getGraphicsAllocation(),
                              devQueueHw->getDebugQueue());

            GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
                *this,
                *devQueueHw,
                preemption,
                scheduler,
                &getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
                devQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE));

            scheduler.makeResident(commandStreamReceiver);

            // Update SLM usage
            slmUsed |= scheduler.slmTotalSize > 0;

            parentKernel->getProgram()->getBlockKernelManager()->makeInternalAllocationsResident(commandStreamReceiver);
            if (parentKernel->isAuxTranslationRequired()) {
                blocking = true;
            }
        }

        auto submissionRequired = !isCommandWithoutKernel(commandType);

        if (submissionRequired) {
            completionStamp = enqueueNonBlocked<commandType>(
                surfacesForResidency,
                numSurfaceForResidency,
                commandStream,
                commandStreamStart,
                blocking,
                multiDispatchInfo,
                &previousTimestampPacketNodes,
                eventsRequest,
                eventBuilder,
                taskLevel,
                slmUsed,
                printfHandler.get());

            if (eventBuilder.getEvent()) {
                eventBuilder.getEvent()->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
            }

            if (parentKernel) {
                commandStreamReceiver.overrideMediaVFEStateDirty(true);

                if (devQueueHw->getSchedulerReturnInstance() > 0) {
                    waitUntilComplete(completionStamp.taskCount, completionStamp.flushStamp, false);

                    BuiltinKernelsSimulation::SchedulerSimulation<GfxFamily> simulation;
                    simulation.runSchedulerSimulation(devQueueHw->getQueueBuffer(),
                                                      devQueueHw->getStackBuffer(),
                                                      devQueueHw->getEventPoolBuffer(),
                                                      devQueueHw->getSlbBuffer(),
                                                      devQueueHw->getDshBuffer(),
                                                      parentKernel->getKernelReflectionSurface(),
                                                      devQueueHw->getQueueStorageBuffer(),
                                                      this->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getGraphicsAllocation(),
                                                      devQueueHw->getDebugQueue());
                }
            }
        } else {
            auto maxTaskCount = this->taskCount;
            for (auto eventId = 0u; eventId < numEventsInWaitList; eventId++) {
                auto event = castToObject<Event>(eventWaitList[eventId]);
                if (!event->isUserEvent() && !event->isExternallySynchronized()) {
                    maxTaskCount = std::max(maxTaskCount, event->peekTaskCount());
                }
            }

            //inherit data from event_wait_list and previous packets
            completionStamp.flushStamp = this->flushStamp->peekStamp();
            completionStamp.taskCount = maxTaskCount;
            completionStamp.taskLevel = taskLevel;

            if (eventBuilder.getEvent() && isProfilingEnabled()) {
                TimeStampData submitTimeStamp;
                this->getDevice().getOSTime()->getCpuGpuTime(&submitTimeStamp);
                eventBuilder.getEvent()->setSubmitTimeStamp(&submitTimeStamp);
                eventBuilder.getEvent()->setSubmitTimeStamp();
                eventBuilder.getEvent()->setStartTimeStamp();
            }
        }
    } else {
        CompletionStamp cmplStamp = {
            Event::eventNotReady,
            taskLevel,
            0,
            engineType};
        completionStamp = cmplStamp;
    }
    updateFromCompletionStamp(completionStamp);

    if (eventBuilder.getEvent()) {
        eventBuilder.getEvent()->updateCompletionStamp(completionStamp.taskCount, completionStamp.taskLevel, completionStamp.flushStamp);
        DebugManager.log(DebugManager.flags.EventsDebugEnable.get(), "updateCompletionStamp Event", eventBuilder.getEvent(), "taskLevel", eventBuilder.getEvent()->taskLevel.load());
    }

    if (blockQueue) {
        if (parentKernel) {
            size_t minSizeSSHForEM = KernelCommandsHelper<GfxFamily>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(const_cast<const Kernel &>(*parentKernel));
            blockedCommandsData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        }

        enqueueBlocked<commandType>(
            surfacesForResidency,
            numSurfaceForResidency,
            blocking,
            multiDispatchInfo,
            &previousTimestampPacketNodes,
            blockedCommandsData,
            eventsRequest,
            slmUsed,
            eventBuilder,
            std::move(printfHandler));
    }

    queueOwnership.unlock();
    commandStreamRecieverOwnership.unlock();

    if (blocking) {
        if (blockQueue) {
            while (isQueueBlocked())
                ;
            waitUntilComplete(taskCount, flushStamp->peekStamp(), false);
        } else {
            waitUntilComplete(taskCount, flushStamp->peekStamp(), false);
            if (printfHandler) {
                printfHandler->printEnqueueOutput();
            }
            commandStreamReceiver.waitForTaskCountAndCleanAllocationList(completionStamp.taskCount, TEMPORARY_ALLOCATION);
        }
    }
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::obtainTaskLevelAndBlockedStatus(unsigned int &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueue, unsigned int commandType) {
    auto isQueueBlockedStatus = isQueueBlocked();
    taskLevel = getTaskLevelFromWaitList(this->taskLevel, numEventsInWaitList, eventWaitList);
    blockQueue = (taskLevel == Event::eventNotReady) || isQueueBlockedStatus;

    auto updateTaskLevel = isTaskLevelUpdateRequired(taskLevel, eventWaitList, numEventsInWaitList, commandType);
    if (updateTaskLevel) {
        taskLevel++;
        this->taskLevel = taskLevel;
    }
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isTaskLevelUpdateRequired(const uint32_t &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType) {
    bool updateTaskLevel = true;
    //if we are blocked by user event then no update
    if (taskLevel == Event::eventNotReady) {
        updateTaskLevel = false;
    }
    //if we are executing command without kernel then it will inherit state from
    //previous commands, barrier is exception
    if (isCommandWithoutKernel(commandType) && commandType != CL_COMMAND_BARRIER) {
        updateTaskLevel = false;
    }
    //ooq special cases starts here
    if (this->isOOQEnabled()) {
        //if no wait list and barrier , do not update task level
        if (eventWaitList == nullptr && commandType != CL_COMMAND_BARRIER) {
            updateTaskLevel = false;
        }
        //if we have waitlist then deduce task level from waitlist and check if it is higher then current task level of queue
        if (eventWaitList != nullptr) {
            auto taskLevelFromEvents = getTaskLevelFromWaitList(0, numEventsInWaitList, eventWaitList);
            taskLevelFromEvents++;
            if (taskLevelFromEvents <= this->taskLevel) {
                updateTaskLevel = false;
            }
        }
    }
    return updateTaskLevel;
}

template <typename GfxFamily>
template <uint32_t commandType>
CompletionStamp CommandQueueHw<GfxFamily>::enqueueNonBlocked(
    Surface **surfaces,
    size_t surfaceCount,
    LinearStream &commandStream,
    size_t commandStreamStart,
    bool &blocking,
    const MultiDispatchInfo &multiDispatchInfo,
    TimestampPacketContainer *previousTimestampPacketNodes,
    EventsRequest &eventsRequest,
    EventBuilder &eventBuilder,
    uint32_t taskLevel,
    bool slmUsed,
    PrintfHandler *printfHandler) {

    UNRECOVERABLE_IF(multiDispatchInfo.empty());

    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto implicitFlush = false;

    if (printfHandler) {
        blocking = true;
        printfHandler->makeResident(commandStreamReceiver);
    }
    if (timestampPacketContainer) {
        timestampPacketContainer->makeResident(device->getCommandStreamReceiver());
        previousTimestampPacketNodes->makeResident(device->getCommandStreamReceiver());
    }

    auto requiresCoherency = false;
    for (auto surface : CreateRange(surfaces, surfaceCount)) {
        surface->makeResident(commandStreamReceiver);
        requiresCoherency |= surface->IsCoherent;
    }

    auto mediaSamplerRequired = false;
    uint32_t numGrfRequired = GrfConfig::DefaultGrfNumber;
    auto specialPipelineSelectMode = false;
    Kernel *kernel = nullptr;
    for (auto &dispatchInfo : multiDispatchInfo) {
        if (kernel != dispatchInfo.getKernel()) {
            kernel = dispatchInfo.getKernel();
        } else {
            continue;
        }
        kernel->makeResident(commandStreamReceiver);
        requiresCoherency |= kernel->requiresCoherency();
        mediaSamplerRequired |= kernel->isVmeKernel();
        auto numGrfRequiredByKernel = kernel->getKernelInfo().patchInfo.executionEnvironment->NumGRFRequired;
        numGrfRequired = std::max(numGrfRequired, numGrfRequiredByKernel);
        specialPipelineSelectMode |= kernel->requiresSpecialPipelineSelectMode();
    }

    if (mediaSamplerRequired) {
        DEBUG_BREAK_IF(device->getDeviceInfo().preemptionSupported != false);
    }

    TimeStampData submitTimeStamp;
    if (isProfilingEnabled() && eventBuilder.getEvent()) {
        this->getDevice().getOSTime()->getCpuGpuTime(&submitTimeStamp);
        eventBuilder.getEvent()->setSubmitTimeStamp(&submitTimeStamp);
        this->getDevice().getCommandStreamReceiver().makeResident(*eventBuilder.getEvent()->getHwTimeStampAllocation());
        if (isPerfCountersEnabled()) {
            this->getDevice().getCommandStreamReceiver().makeResident(*eventBuilder.getEvent()->getHwPerfCounterAllocation());
        }
    }

    IndirectHeap *dsh = nullptr;
    IndirectHeap *ioh = nullptr;

    if (multiDispatchInfo.peekParentKernel()) {
        DeviceQueueHw<GfxFamily> *pDevQueue = castToObject<DeviceQueueHw<GfxFamily>>(this->getContext().getDefaultDeviceQueue());
        DEBUG_BREAK_IF(pDevQueue == nullptr);
        dsh = pDevQueue->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);
        // In ExecutionModel IOH is the same as DSH to eliminate StateBaseAddress reprogramming for scheduler kernel and blocks.
        ioh = dsh;
        implicitFlush = true;
    } else {
        dsh = &getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u);
        ioh = &getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);
    }

    commandStreamReceiver.requestThreadArbitrationPolicy(multiDispatchInfo.peekMainKernel()->getThreadArbitrationPolicy<GfxFamily>());

    auto allocNeedsFlushDC = false;
    if (!device->isFullRangeSvm()) {
        if (std::any_of(commandStreamReceiver.getResidencyAllocations().begin(), commandStreamReceiver.getResidencyAllocations().end(), [](const auto allocation) { return allocation->flushL3Required; })) {
            allocNeedsFlushDC = true;
        }
    }

    DispatchFlags dispatchFlags;
    dispatchFlags.blocking = blocking;
    dispatchFlags.dcFlush = shouldFlushDC(commandType, printfHandler) || allocNeedsFlushDC;
    dispatchFlags.useSLM = slmUsed;
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.GSBA32BitRequired = commandType == CL_COMMAND_NDRANGE_KERNEL;
    dispatchFlags.mediaSamplerRequired = mediaSamplerRequired;
    dispatchFlags.requiresCoherency = requiresCoherency;
    dispatchFlags.lowPriority = priority == QueuePriority::LOW;
    dispatchFlags.implicitFlush = implicitFlush;
    dispatchFlags.flushStampReference = this->flushStamp->getStampReference();
    dispatchFlags.preemptionMode = PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo);
    dispatchFlags.outOfOrderExecutionAllowed = !eventBuilder.getEvent() || commandStreamReceiver.isNTo1SubmissionModelEnabled();
    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        dispatchFlags.outOfDeviceDependencies = &eventsRequest;
    }
    dispatchFlags.numGrfRequired = numGrfRequired;
    dispatchFlags.specialPipelineSelectMode = specialPipelineSelectMode;
    DEBUG_BREAK_IF(taskLevel >= Event::eventNotReady);

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyPreFlushTask(this);
    }

    CompletionStamp completionStamp = commandStreamReceiver.flushTask(
        commandStream,
        commandStreamStart,
        *dsh,
        *ioh,
        getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
        taskLevel,
        dispatchFlags,
        *device);

    return completionStamp;
}

template <typename GfxFamily>
template <uint32_t commandType>
void CommandQueueHw<GfxFamily>::enqueueBlocked(
    Surface **surfaces,
    size_t surfaceCount,
    bool &blocking,
    const MultiDispatchInfo &multiDispatchInfo,
    TimestampPacketContainer *previousTimestampPacketNodes,
    KernelOperation *blockedCommandsData,
    EventsRequest &eventsRequest,
    bool slmUsed,
    EventBuilder &externalEventBuilder,
    std::unique_ptr<PrintfHandler> printfHandler) {

    auto &commandStreamReceiver = device->getCommandStreamReceiver();

    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    //store previous virtual event as it will add dependecies to new virtual event
    if (this->virtualEvent) {
        DBG_LOG(EventsDebugEnable, "enqueueBlocked", "previousVirtualEvent", this->virtualEvent);
    }

    EventBuilder internalEventBuilder;
    EventBuilder *eventBuilder;
    // check if event will be exposed externally
    if (externalEventBuilder.getEvent()) {
        externalEventBuilder.getEvent()->incRefInternal();
        eventBuilder = &externalEventBuilder;
        DBG_LOG(EventsDebugEnable, "enqueueBlocked", "output event as virtualEvent", virtualEvent);
    } else {
        // it will be an internal event
        internalEventBuilder.create<VirtualEvent>(this, context);
        eventBuilder = &internalEventBuilder;
        DBG_LOG(EventsDebugEnable, "enqueueBlocked", "new virtualEvent", eventBuilder->getEvent());
    }
    eventBuilder->getEvent()->setCurrentCmdQVirtualEvent(true);

    //update queue taskCount
    taskCount = eventBuilder->getEvent()->getCompletionStamp();

    if (multiDispatchInfo.empty()) {
        DEBUG_BREAK_IF(!isCommandWithoutKernel(commandType));
        auto cmdSize = static_cast<uint32_t>(EnqueueOperation<GfxFamily>::getSizeRequiredCS(commandType,
                                                                                            isProfilingEnabled(),
                                                                                            isPerfCountersEnabled(),
                                                                                            *this,
                                                                                            nullptr));

        auto cmd = std::make_unique<CommandMarker>(*this, commandStreamReceiver, commandType, cmdSize);

        eventBuilder->getEvent()->setCommand(std::move(cmd));
    } else {
        //store task data in event
        std::vector<Surface *> allSurfaces;
        Kernel *kernel = nullptr;
        for (auto &dispatchInfo : multiDispatchInfo) {
            if (kernel != dispatchInfo.getKernel()) {
                kernel = dispatchInfo.getKernel();
            } else {
                continue;
            }
            kernel->getResidency(allSurfaces);
        }
        for (auto &surface : CreateRange(surfaces, surfaceCount)) {
            allSurfaces.push_back(surface->duplicate());
        }
        PreemptionMode preemptionMode = PreemptionHelper::taskPreemptionMode(*device, multiDispatchInfo);
        auto kernelOperation = std::unique_ptr<KernelOperation>(blockedCommandsData); // marking ownership
        auto cmd = std::make_unique<CommandComputeKernel>(
            *this,
            std::move(kernelOperation),
            allSurfaces,
            shouldFlushDC(commandType, printfHandler.get()),
            slmUsed,
            commandType == CL_COMMAND_NDRANGE_KERNEL,
            std::move(printfHandler),
            preemptionMode,
            multiDispatchInfo.peekMainKernel(),
            (uint32_t)multiDispatchInfo.size());

        if (timestampPacketContainer.get()) {
            cmd->setTimestampPacketNode(*timestampPacketContainer, *previousTimestampPacketNodes);
        }
        cmd->setEventsRequest(eventsRequest);
        eventBuilder->getEvent()->setCommand(std::move(cmd));
    }

    eventBuilder->addParentEvents(ArrayRef<const cl_event>(eventsRequest.eventWaitList, eventsRequest.numEventsInWaitList));
    eventBuilder->addParentEvent(this->virtualEvent);
    eventBuilder->finalize();

    if (this->virtualEvent) {
        this->virtualEvent->setCurrentCmdQVirtualEvent(false);
        this->virtualEvent->decRefInternal();
    }

    this->virtualEvent = eventBuilder->getEvent();
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::computeOffsetsValueForRectCommands(size_t *bufferOffset,
                                                                   size_t *hostOffset,
                                                                   const size_t *bufferOrigin,
                                                                   const size_t *hostOrigin,
                                                                   const size_t *region,
                                                                   size_t bufferRowPitch,
                                                                   size_t bufferSlicePitch,
                                                                   size_t hostRowPitch,
                                                                   size_t hostSlicePitch) {
    size_t computedBufferRowPitch = bufferRowPitch ? bufferRowPitch : region[0];
    size_t computedBufferSlicePitch = bufferSlicePitch ? bufferSlicePitch : region[1] * computedBufferRowPitch;
    size_t computedHostRowPitch = hostRowPitch ? hostRowPitch : region[0];
    size_t computedHostSlicePitch = hostSlicePitch ? hostSlicePitch : region[1] * computedHostRowPitch;
    *bufferOffset = bufferOrigin[2] * computedBufferSlicePitch + bufferOrigin[1] * computedBufferRowPitch + bufferOrigin[0];
    *hostOffset = hostOrigin[2] * computedHostSlicePitch + hostOrigin[1] * computedHostRowPitch + hostOrigin[0];
}

template <typename GfxFamily>
size_t CommandQueueHw<GfxFamily>::calculateHostPtrSizeForImage(size_t *region, size_t rowPitch, size_t slicePitch, Image *image) {
    auto bytesPerPixel = image->getSurfaceFormatInfo().ImageElementSizeInBytes;
    auto dstRowPitch = rowPitch ? rowPitch : region[0] * bytesPerPixel;
    auto dstSlicePitch = slicePitch ? slicePitch : ((image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

    return Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, image->getImageDesc().image_type);
}
} // namespace OCLRT
