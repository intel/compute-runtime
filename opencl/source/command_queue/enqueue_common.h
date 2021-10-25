/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/local_work_size.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/program/sync_buffer_handler.inl"
#include "shared/source/utilities/range.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/builtin_kernels_simulation/scheduler_simulation.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/helpers/enqueue_properties.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/migration_controller.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/utilities/cl_logger.h"

#include "pipe_control_args.h"

#include <algorithm>
#include <new>

namespace NEO {

template <typename GfxFamily>
template <uint32_t commandType, size_t surfaceCount>
void CommandQueueHw<GfxFamily>::enqueueHandler(Surface *(&surfaces)[surfaceCount],
                                               bool blocking,
                                               Kernel *kernel,
                                               cl_uint workDim,
                                               const size_t globalOffsets[3],
                                               const size_t workItems[3],
                                               const size_t *localWorkSizesIn,
                                               const size_t *enqueuedWorkSizes,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    BuiltInOwnershipWrapper builtInLock;
    KernelObjsForAuxTranslation kernelObjsForAuxTranslation;
    MultiDispatchInfo multiDispatchInfo(kernel);

    auto auxTranslationMode = AuxTranslationMode::None;

    if (DebugManager.flags.ForceDispatchScheduler.get()) {
        forceDispatchScheduler(multiDispatchInfo);
    } else {

        kernel->updateAuxTranslationRequired();
        if (kernel->isAuxTranslationRequired()) {
            kernel->fillWithKernelObjsForAuxTranslation(kernelObjsForAuxTranslation);
            multiDispatchInfo.setKernelObjsForAuxTranslation(kernelObjsForAuxTranslation);

            if (!kernelObjsForAuxTranslation.empty()) {
                auxTranslationMode = HwHelperHw<GfxFamily>::get().getAuxTranslationMode(device->getHardwareInfo());
            }
        }

        if (AuxTranslationMode::Builtin == auxTranslationMode) {
            auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::AuxTranslation, getClDevice());
            builtInLock.takeOwnership(builder, this->context);

            dispatchAuxTranslationBuiltin(multiDispatchInfo, AuxTranslationDirection::AuxToNonAux);
        }

        if (kernel->getKernelInfo().builtinDispatchBuilder == nullptr) {
            DispatchInfoBuilder<SplitDispatch::Dim::d3D, SplitDispatch::SplitMode::WalkerSplit> builder(getClDevice());
            builder.setDispatchGeometry(workDim, workItems, enqueuedWorkSizes, globalOffsets, Vec3<size_t>{0, 0, 0}, localWorkSizesIn);
            builder.setKernel(kernel);
            builder.bake(multiDispatchInfo);
        } else {
            auto builder = kernel->getKernelInfo().builtinDispatchBuilder;
            builder->buildDispatchInfos(multiDispatchInfo, kernel, workDim, workItems, enqueuedWorkSizes, globalOffsets);

            if (multiDispatchInfo.size() == 0) {
                return;
            }
        }

        if (AuxTranslationMode::Builtin == auxTranslationMode) {
            UNRECOVERABLE_IF(kernel->isParentKernel);
            dispatchAuxTranslationBuiltin(multiDispatchInfo, AuxTranslationDirection::NonAuxToAux);
        }
    }

    if (AuxTranslationMode::Blit == auxTranslationMode) {
        setupBlitAuxTranslation(multiDispatchInfo);
    }

    enqueueHandler<commandType>(surfaces, blocking, multiDispatchInfo, numEventsInWaitList, eventWaitList, event);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::forceDispatchScheduler(NEO::MultiDispatchInfo &multiDispatchInfo) {
    SchedulerKernel &scheduler = getContext().getSchedulerKernel();

    auto devQueue = this->getContext().getDefaultDeviceQueue();
    DeviceQueueHw<GfxFamily> *devQueueHw = castToObjectOrAbort<DeviceQueueHw<GfxFamily>>(devQueue);

    DispatchInfo dispatchInfo(devQueue->getClDevice(), &scheduler, 1, Vec3<size_t>(scheduler.getGws(), 1, 1), Vec3<size_t>(scheduler.getLws(), 1, 1), Vec3<size_t>(0, 0, 0));
    Vec3<size_t> workGroupCount = generateWorkgroupsNumber(dispatchInfo.getGWS(), dispatchInfo.getEnqueuedWorkgroupSize());
    dispatchInfo.setTotalNumberOfWorkgroups(workGroupCount);
    dispatchInfo.setNumberOfWorkgroups(workGroupCount);

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
        enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, blocking, multiDispatchInfo,
                                          numEventsInWaitList, eventWaitList, event);
        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(commandType);
        }
        return;
    }

    Kernel *parentKernel = multiDispatchInfo.peekParentKernel();
    auto devQueue = this->getContext().getDefaultDeviceQueue();
    DeviceQueueHw<GfxFamily> *devQueueHw = castToObject<DeviceQueueHw<GfxFamily>>(devQueue);

    TagNodeBase *hwTimeStamps = nullptr;

    auto commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();

    EventBuilder eventBuilder;
    setupEvent(eventBuilder, event, commandType);

    bool isMarkerWithProfiling = (CL_COMMAND_MARKER == commandType) && (eventBuilder.getEvent() && eventBuilder.getEvent()->isProfilingEnabled());

    std::unique_ptr<KernelOperation> blockedCommandsData;
    std::unique_ptr<PrintfHandler> printfHandler;
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, numEventsInWaitList, eventWaitList, blockQueue, commandType);

    if (parentKernel && !blockQueue) {
        while (!devQueueHw->isEMCriticalSectionFree())
            ;
    }

    enqueueHandlerHook(commandType, multiDispatchInfo);

    bool clearDependenciesForSubCapture = false;
    aubCaptureHook(blocking, clearDependenciesForSubCapture, multiDispatchInfo);

    bool clearAllDependencies = (queueDependenciesClearRequired() || clearDependenciesForSubCapture);

    if (DebugManager.flags.MakeEachEnqueueBlocking.get()) {
        blocking = true;
    }

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    CsrDependencies csrDeps;
    BlitPropertiesContainer blitPropertiesContainer;

    if (this->context->getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForTaskCountContainer(csrDeps, getGpgpuCommandStreamReceiver());
    }

    bool enqueueWithBlitAuxTranslation = isBlitAuxTranslationRequired(multiDispatchInfo);

    if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        if (!clearDependenciesForSubCapture) {
            eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OnCsr);
        }

        auto allocator = getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();

        size_t nodesCount = 0u;
        if (isCacheFlushCommand(commandType) || isMarkerWithProfiling) {
            nodesCount = 1;
        } else if (!multiDispatchInfo.empty()) {
            nodesCount = estimateTimestampPacketNodesCount(multiDispatchInfo);
        }

        if (isCacheFlushForBcsRequired() && enqueueWithBlitAuxTranslation) {
            // Cache flush for aux translation is always required (if supported)
            timestampPacketDependencies.cacheFlushNodes.add(allocator->getTag());
        }

        if (nodesCount > 0) {
            obtainNewTimestampPacketNodes(nodesCount, timestampPacketDependencies.previousEnqueueNodes, clearAllDependencies, getGpgpuCommandStreamReceiver());
            csrDeps.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);
        }
    }

    auto &commandStream = *obtainCommandStream<commandType>(csrDeps, false, blockQueue, multiDispatchInfo, eventsRequest,
                                                            blockedCommandsData, surfacesForResidency, numSurfaceForResidency, isMarkerWithProfiling);
    auto commandStreamStart = commandStream.getUsed();

    if (this->context->getRootDeviceIndices().size() > 1) {
        TimestampPacketHelper::programCsrDependenciesForForTaskCountContainer<GfxFamily>(commandStream, csrDeps);
    }

    if (enqueueWithBlitAuxTranslation) {
        processDispatchForBlitAuxTranslation(*getBcsForAuxTranslation(), multiDispatchInfo, blitPropertiesContainer,
                                             timestampPacketDependencies, eventsRequest, blockQueue);
    }

    if (eventBuilder.getEvent() && getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        eventBuilder.getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
        eventBuilder.getEvent()->addTimestampPacketNodes(timestampPacketDependencies.nonAuxToAuxNodes);
        eventBuilder.getEvent()->addTimestampPacketNodes(timestampPacketDependencies.auxToNonAuxNodes);
    }

    bool flushDependenciesForNonKernelCommand = false;

    if (multiDispatchInfo.empty() == false) {
        processDispatchForKernels<commandType>(multiDispatchInfo, printfHandler, eventBuilder.getEvent(),
                                               hwTimeStamps, blockQueue, devQueueHw, csrDeps, blockedCommandsData.get(),
                                               timestampPacketDependencies);
    } else if (isCacheFlushCommand(commandType)) {
        processDispatchForCacheFlush(surfacesForResidency, numSurfaceForResidency, &commandStream, csrDeps);
    } else if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        if (CL_COMMAND_BARRIER == commandType) {
            getGpgpuCommandStreamReceiver().requestStallingPipeControlOnNextFlush();
        }

        for (size_t i = 0; i < eventsRequest.numEventsInWaitList; i++) {
            auto waitlistEvent = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
            if (waitlistEvent->getTimestampPacketNodes()) {
                flushDependenciesForNonKernelCommand = true;
                if (eventBuilder.getEvent()) {
                    eventBuilder.getEvent()->addTimestampPacketNodes(*waitlistEvent->getTimestampPacketNodes());
                }
            }
        }

        if (flushDependenciesForNonKernelCommand) {
            TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, csrDeps);
        }

        if (isMarkerWithProfiling) {
            if (numEventsInWaitList == 0) {
                PipeControlArgs args(false);
                MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);
            }
            processDispatchForMarkerWithTimestampPacket(*this, &commandStream, eventsRequest, csrDeps);
        }
    } else if (isMarkerWithProfiling) {
        processDispatchForMarker(*this, &commandStream, eventsRequest, csrDeps);
    }

    CompletionStamp completionStamp = {CompletionStamp::notReady, taskLevel, 0};
    const EnqueueProperties enqueueProperties(false, !multiDispatchInfo.empty(), isCacheFlushCommand(commandType),
                                              flushDependenciesForNonKernelCommand, isMarkerWithProfiling, &blitPropertiesContainer);

    bool migratedMemory = false;

    if (!blockQueue && multiDispatchInfo.peekMainKernel() && multiDispatchInfo.peekMainKernel()->requiresMemoryMigration()) {
        for (auto &arg : multiDispatchInfo.peekMainKernel()->getMemObjectsToMigrate()) {
            MigrationController::handleMigration(*this->context, getGpgpuCommandStreamReceiver(), arg.second);
            migratedMemory = true;
        }
    }
    if (!blockQueue) {
        if (parentKernel) {
            processDeviceEnqueue(devQueueHw, multiDispatchInfo, hwTimeStamps, blocking);
        }

        if (enqueueProperties.operation == EnqueueProperties::Operation::GpuKernel) {
            csrDeps.makeResident(getGpgpuCommandStreamReceiver());

            completionStamp = enqueueNonBlocked<commandType>(
                surfacesForResidency,
                numSurfaceForResidency,
                commandStream,
                commandStreamStart,
                blocking,
                clearDependenciesForSubCapture,
                multiDispatchInfo,
                enqueueProperties,
                timestampPacketDependencies,
                eventsRequest,
                eventBuilder,
                taskLevel,
                printfHandler.get(),
                getBcsForAuxTranslation());

            if (parentKernel) {
                getGpgpuCommandStreamReceiver().setMediaVFEStateDirty(true);

                if (devQueueHw->getSchedulerReturnInstance() > 0) {
                    waitUntilComplete(completionStamp.taskCount, {}, completionStamp.flushStamp, false);
                    this->runSchedulerSimulation(*devQueueHw, *parentKernel);
                }
            }
        } else if (enqueueProperties.isFlushWithoutKernelRequired()) {
            completionStamp = enqueueCommandWithoutKernel(
                surfacesForResidency,
                numSurfaceForResidency,
                &commandStream,
                commandStreamStart,
                blocking,
                enqueueProperties,
                timestampPacketDependencies,
                eventsRequest,
                eventBuilder,
                taskLevel,
                csrDeps,
                nullptr);
        } else {
            UNRECOVERABLE_IF(enqueueProperties.operation != EnqueueProperties::Operation::EnqueueWithoutSubmission);

            auto maxTaskCountCurrentRootDevice = this->taskCount;

            for (auto eventId = 0u; eventId < numEventsInWaitList; eventId++) {
                auto event = castToObject<Event>(eventWaitList[eventId]);

                if (event->getCommandQueue() && event->getCommandQueue()->getDevice().getRootDeviceIndex() == this->getDevice().getRootDeviceIndex()) {
                    maxTaskCountCurrentRootDevice = std::max(maxTaskCountCurrentRootDevice, event->peekTaskCount());
                }
            }

            //inherit data from event_wait_list and previous packets
            completionStamp.flushStamp = this->flushStamp->peekStamp();
            completionStamp.taskCount = maxTaskCountCurrentRootDevice;
            completionStamp.taskLevel = taskLevel;

            if (eventBuilder.getEvent() && isProfilingEnabled()) {
                eventBuilder.getEvent()->setSubmitTimeStamp();
                eventBuilder.getEvent()->setStartTimeStamp();
            }
        }
        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
        }

        this->latestSentEnqueueType = enqueueProperties.operation;
    }
    updateFromCompletionStamp(completionStamp, eventBuilder.getEvent());

    if (blockQueue) {
        if (parentKernel) {
            size_t minSizeSSHForEM = HardwareCommandsHelper<GfxFamily>::getSshSizeForExecutionModel(*parentKernel);
            blockedCommandsData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        }

        enqueueBlocked(commandType,
                       surfacesForResidency,
                       numSurfaceForResidency,
                       multiDispatchInfo,
                       timestampPacketDependencies,
                       blockedCommandsData,
                       enqueueProperties,
                       eventsRequest,
                       eventBuilder,
                       std::move(printfHandler),
                       nullptr);
    }

    if (deferredTimestampPackets.get()) {
        timestampPacketDependencies.moveNodesToNewContainer(*deferredTimestampPackets);
    }

    queueOwnership.unlock();
    commandStreamReceiverOwnership.unlock();

    if (blocking) {
        waitForAllEngines(blockQueue, (blockQueue ? nullptr : printfHandler.get()));
    }
    if (migratedMemory) {
        getGpgpuCommandStreamReceiver().flushBatchedSubmissions();
    }
}

template <typename GfxFamily>
template <uint32_t commandType>
void CommandQueueHw<GfxFamily>::processDispatchForKernels(const MultiDispatchInfo &multiDispatchInfo,
                                                          std::unique_ptr<PrintfHandler> &printfHandler,
                                                          Event *event,
                                                          TagNodeBase *&hwTimeStamps,
                                                          bool blockQueue,
                                                          DeviceQueueHw<GfxFamily> *devQueueHw,
                                                          CsrDependencies &csrDeps,
                                                          KernelOperation *blockedCommandsData,
                                                          TimestampPacketDependencies &timestampPacketDependencies) {
    TagNodeBase *hwPerfCounter = nullptr;
    getClFileLogger().dumpKernelArgs(&multiDispatchInfo);

    printfHandler.reset(PrintfHandler::create(multiDispatchInfo, *device));
    if (printfHandler) {
        printfHandler->prepareDispatch(multiDispatchInfo);
    }

    if (multiDispatchInfo.peekMainKernel()->usesSyncBuffer()) {
        auto &gws = multiDispatchInfo.begin()->getGWS();
        auto &lws = multiDispatchInfo.begin()->getLocalWorkgroupSize();
        size_t workGroupsCount = (gws.x * gws.y * gws.z) /
                                 (lws.x * lws.y * lws.z);
        device->getDevice().syncBufferHandler->prepareForEnqueue(workGroupsCount, *multiDispatchInfo.peekMainKernel());
    }

    if (commandType == CL_COMMAND_NDRANGE_KERNEL) {
        if (multiDispatchInfo.peekMainKernel()->isKernelDebugEnabled()) {
            setupDebugSurface(multiDispatchInfo.peekMainKernel());
        }
    }

    if (event && this->isProfilingEnabled()) {
        // Get allocation for timestamps
        hwTimeStamps = event->getHwTimeStampNode();
    }

    if (auto parentKernel = multiDispatchInfo.peekParentKernel()) {
        parentKernel->createReflectionSurface();
        parentKernel->patchDefaultDeviceQueue(context->getDefaultDeviceQueue());
        parentKernel->patchEventPool(context->getDefaultDeviceQueue());
        parentKernel->patchReflectionSurface(context->getDefaultDeviceQueue(), printfHandler.get());
        if (!blockQueue) {
            devQueueHw->resetDeviceQueue();
            devQueueHw->acquireEMCriticalSection();
        }
    }

    if (event && this->isPerfCountersEnabled()) {
        hwPerfCounter = event->getHwPerfCounterNode();
    }

    HardwareInterface<GfxFamily>::dispatchWalker(
        *this,
        multiDispatchInfo,
        csrDeps,
        blockedCommandsData,
        hwTimeStamps,
        hwPerfCounter,
        &timestampPacketDependencies,
        timestampPacketContainer.get(),
        commandType);

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        for (auto &dispatchInfo : multiDispatchInfo) {
            for (auto &patchInfoData : dispatchInfo.getKernel()->getPatchInfoDataList()) {
                getGpgpuCommandStreamReceiver().getFlatBatchBufferHelper().setPatchInfoData(patchInfoData);
            }
        }
    }

    getGpgpuCommandStreamReceiver().setRequiredScratchSizes(multiDispatchInfo.getRequiredScratchSize(), multiDispatchInfo.getRequiredPrivateScratchSize());
}

template <typename GfxFamily>
BlitProperties CommandQueueHw<GfxFamily>::processDispatchForBlitEnqueue(CommandStreamReceiver &blitCommandStreamReceiver,
                                                                        const MultiDispatchInfo &multiDispatchInfo,
                                                                        TimestampPacketDependencies &timestampPacketDependencies,
                                                                        const EventsRequest &eventsRequest, LinearStream *commandStream,
                                                                        uint32_t commandType, bool queueBlocked) {
    auto blitDirection = ClBlitProperties::obtainBlitDirection(commandType);

    auto blitProperties = ClBlitProperties::constructProperties(blitDirection, blitCommandStreamReceiver,
                                                                multiDispatchInfo.peekBuiltinOpParams());
    if (!queueBlocked) {
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(blitProperties.csrDependencies, blitCommandStreamReceiver,
                                                                     CsrDependencies::DependenciesType::All);

        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.cacheFlushNodes);
        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);
        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.barrierNodes);
    }

    auto currentTimestampPacketNode = timestampPacketContainer->peekNodes().at(0);
    blitProperties.outputTimestampPacket = currentTimestampPacketNode;

    if (commandStream) {
        if (timestampPacketDependencies.cacheFlushNodes.peekNodes().size() > 0) {
            auto cacheFlushTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketDependencies.cacheFlushNodes.peekNodes()[0]);
            PipeControlArgs args(true);
            MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
                *commandStream,
                GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                cacheFlushTimestampPacketGpuAddress,
                0,
                device->getHardwareInfo(),
                args);
        }

        TimestampPacketHelper::programSemaphore<GfxFamily>(*commandStream, *currentTimestampPacketNode);
    }
    return blitProperties;
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForBlitAuxTranslation(CommandStreamReceiver &bcsCsr,
                                                                     const MultiDispatchInfo &multiDispatchInfo,
                                                                     BlitPropertiesContainer &blitPropertiesContainer,
                                                                     TimestampPacketDependencies &timestampPacketDependencies,
                                                                     const EventsRequest &eventsRequest, bool queueBlocked) {
    auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    auto nodesAllocator = getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    auto numKernelObjs = multiDispatchInfo.getKernelObjsForAuxTranslation()->size();
    blitPropertiesContainer.resize(numKernelObjs * 2);

    auto bufferIndex = 0;
    for (auto &kernelObj : *multiDispatchInfo.getKernelObjsForAuxTranslation()) {
        GraphicsAllocation *allocation = nullptr;
        if (kernelObj.type == KernelObjForAuxTranslation::Type::MEM_OBJ) {
            auto buffer = static_cast<Buffer *>(kernelObj.object);
            allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
        } else {
            DEBUG_BREAK_IF(kernelObj.type != KernelObjForAuxTranslation::Type::GFX_ALLOC);
            allocation = static_cast<GraphicsAllocation *>(kernelObj.object);
        }
        {
            // Aux to NonAux
            blitPropertiesContainer[bufferIndex] = BlitProperties::constructPropertiesForAuxTranslation(
                AuxTranslationDirection::AuxToNonAux, allocation, getGpgpuCommandStreamReceiver().getClearColorAllocation());
            auto auxToNonAuxNode = nodesAllocator->getTag();
            timestampPacketDependencies.auxToNonAuxNodes.add(auxToNonAuxNode);
        }

        {
            // NonAux to Aux
            blitPropertiesContainer[bufferIndex + numKernelObjs] = BlitProperties::constructPropertiesForAuxTranslation(
                AuxTranslationDirection::NonAuxToAux, allocation, getGpgpuCommandStreamReceiver().getClearColorAllocation());
            auto nonAuxToAuxNode = nodesAllocator->getTag();
            timestampPacketDependencies.nonAuxToAuxNodes.add(nonAuxToAuxNode);
        }
        bufferIndex++;
    }

    if (!queueBlocked) {
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, bcsCsr, CsrDependencies::DependenciesType::All);
        BlitProperties::setupDependenciesForAuxTranslation(blitPropertiesContainer, timestampPacketDependencies,
                                                           *this->timestampPacketContainer, csrDeps,
                                                           getGpgpuCommandStreamReceiver(), bcsCsr);
    }

    eventsRequest.setupBcsCsrForOutputEvent(bcsCsr);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForCacheFlush(Surface **surfaces,
                                                             size_t numSurfaces,
                                                             LinearStream *commandStream,
                                                             CsrDependencies &csrDeps) {

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(*commandStream, csrDeps);

    uint64_t postSyncAddress = 0;
    if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNodeForPostSync = timestampPacketContainer->peekNodes().at(0);
        timestampPacketNodeForPostSync->setProfilingCapable(false);
        postSyncAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNodeForPostSync);
    }

    submitCacheFlush(surfaces, numSurfaces, commandStream, postSyncAddress);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForMarker(CommandQueue &commandQueue,
                                                         LinearStream *commandStream,
                                                         EventsRequest &eventsRequest,
                                                         CsrDependencies &csrDeps) {
    auto event = castToObjectOrAbort<Event>(*eventsRequest.outEvent);

    TagNodeBase *hwTimeStamps = nullptr;
    TagNodeBase *hwPerfCounter = nullptr;

    hwTimeStamps = event->getHwTimeStampNode();

    HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);
    HardwareInterface<GfxFamily>::dispatchProfilingPerfEndCommands(hwTimeStamps, hwPerfCounter, commandStream, commandQueue);
    getGpgpuCommandStreamReceiver().makeResident(*hwTimeStamps->getBaseGraphicsAllocation());
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForMarkerWithTimestampPacket(CommandQueue &commandQueue,
                                                                            LinearStream *commandStream,
                                                                            EventsRequest &eventsRequest,
                                                                            CsrDependencies &csrDeps) {
    auto currentTimestampPacketNode = commandQueue.getTimestampPacketContainer()->peekNodes().at(0);

    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(*currentTimestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(*currentTimestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timestampContextStartGpuAddress);
    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, REG_GLOBAL_TIMESTAMP_LDW, timestampGlobalStartAddress);

    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*currentTimestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(*currentTimestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timestampContextEndGpuAddress);
    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, REG_GLOBAL_TIMESTAMP_LDW, timestampGlobalEndAddress);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDeviceEnqueue(DeviceQueueHw<GfxFamily> *devQueueHw,
                                                     const MultiDispatchInfo &multiDispatchInfo,
                                                     TagNodeBase *hwTimeStamps,
                                                     bool &blocking) {
    auto parentKernel = multiDispatchInfo.peekParentKernel();
    size_t minSizeSSHForEM = HardwareCommandsHelper<GfxFamily>::getSshSizeForExecutionModel(*parentKernel);
    bool isCcsUsed = EngineHelpers::isCcs(gpgpuEngine->osContext->getEngineType());

    uint32_t taskCount = getGpgpuCommandStreamReceiver().peekTaskCount() + 1;
    devQueueHw->setupExecutionModelDispatch(getIndirectHeap(IndirectHeap::SURFACE_STATE, minSizeSSHForEM),
                                            *devQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE),
                                            parentKernel,
                                            (uint32_t)multiDispatchInfo.size(),
                                            getGpgpuCommandStreamReceiver().getTagAllocation()->getGpuAddress(),
                                            taskCount,
                                            hwTimeStamps,
                                            isCcsUsed);

    SchedulerKernel &scheduler = getContext().getSchedulerKernel();

    scheduler.setArgs(devQueueHw->getQueueBuffer(),
                      devQueueHw->getStackBuffer(),
                      devQueueHw->getEventPoolBuffer(),
                      devQueueHw->getSlbBuffer(),
                      devQueueHw->getDshBuffer(),
                      parentKernel->getKernelReflectionSurface(),
                      devQueueHw->getQueueStorageBuffer(),
                      this->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getGraphicsAllocation(),
                      devQueueHw->getDebugQueue());

    auto preemptionMode = ClPreemptionHelper::taskPreemptionMode(getDevice(), multiDispatchInfo);
    GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
        *this->commandStream,
        *devQueueHw,
        preemptionMode,
        scheduler,
        &getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
        devQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE),
        isCcsUsed);

    scheduler.makeResident(getGpgpuCommandStreamReceiver());

    parentKernel->getProgram()->getBlockKernelManager()->makeInternalAllocationsResident(getGpgpuCommandStreamReceiver());

    if (parentKernel->isAuxTranslationRequired()) {
        blocking = true;
    }
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::obtainTaskLevelAndBlockedStatus(unsigned int &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueueStatus, unsigned int commandType) {
    auto isQueueBlockedStatus = isQueueBlocked();
    taskLevel = getTaskLevelFromWaitList(this->taskLevel, numEventsInWaitList, eventWaitList);
    blockQueueStatus = (taskLevel == CompletionStamp::notReady) || isQueueBlockedStatus;

    auto taskLevelUpdateRequired = isTaskLevelUpdateRequired(taskLevel, eventWaitList, numEventsInWaitList, commandType);
    if (taskLevelUpdateRequired) {
        taskLevel++;
        this->taskLevel = taskLevel;
    }

    DBG_LOG(EventsDebugEnable, "blockQueue", blockQueueStatus, "virtualEvent", virtualEvent, "taskLevel", taskLevel);
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isTaskLevelUpdateRequired(const uint32_t &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType) {
    bool updateTaskLevel = true;
    //if we are blocked by user event then no update
    if (taskLevel == CompletionStamp::notReady) {
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
    bool clearDependenciesForSubCapture,
    const MultiDispatchInfo &multiDispatchInfo,
    const EnqueueProperties &enqueueProperties,
    TimestampPacketDependencies &timestampPacketDependencies,
    EventsRequest &eventsRequest,
    EventBuilder &eventBuilder,
    uint32_t taskLevel,
    PrintfHandler *printfHandler,
    CommandStreamReceiver *bcsCsr) {

    UNRECOVERABLE_IF(multiDispatchInfo.empty());

    auto implicitFlush = false;

    if (printfHandler) {
        blocking = true;
        printfHandler->makeResident(getGpgpuCommandStreamReceiver());
    }

    if (multiDispatchInfo.peekMainKernel()->usesSyncBuffer()) {
        device->getDevice().syncBufferHandler->makeResident(getGpgpuCommandStreamReceiver());
    }

    if (timestampPacketContainer) {
        timestampPacketContainer->makeResident(getGpgpuCommandStreamReceiver());
        timestampPacketDependencies.previousEnqueueNodes.makeResident(getGpgpuCommandStreamReceiver());
        timestampPacketDependencies.cacheFlushNodes.makeResident(getGpgpuCommandStreamReceiver());
    }

    bool anyUncacheableArgs = false;
    auto requiresCoherency = false;
    for (auto surface : CreateRange(surfaces, surfaceCount)) {
        surface->makeResident(getGpgpuCommandStreamReceiver());
        requiresCoherency |= surface->IsCoherent;
        if (!surface->allowsL3Caching()) {
            anyUncacheableArgs = true;
        }
    }

    auto mediaSamplerRequired = false;
    uint32_t numGrfRequired = GrfConfig::DefaultGrfNumber;
    auto specialPipelineSelectMode = false;
    Kernel *kernel = nullptr;
    bool usePerDssBackedBuffer = false;
    bool auxTranslationRequired = false;
    bool useGlobalAtomics = false;

    for (auto &dispatchInfo : multiDispatchInfo) {
        if (kernel != dispatchInfo.getKernel()) {
            kernel = dispatchInfo.getKernel();
        } else {
            continue;
        }
        kernel->makeResident(getGpgpuCommandStreamReceiver());
        requiresCoherency |= kernel->requiresCoherency();
        mediaSamplerRequired |= kernel->isVmeKernel();
        auto numGrfRequiredByKernel = static_cast<uint32_t>(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.numGrfRequired);
        numGrfRequired = std::max(numGrfRequired, numGrfRequiredByKernel);
        specialPipelineSelectMode |= kernel->requiresSpecialPipelineSelectMode();
        auxTranslationRequired |= kernel->isAuxTranslationRequired();
        if (kernel->hasUncacheableStatelessArgs()) {
            anyUncacheableArgs = true;
        }

        if (kernel->requiresPerDssBackedBuffer()) {
            usePerDssBackedBuffer = true;
        }

        if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics) {
            useGlobalAtomics = true;
        }
    }

    if (mediaSamplerRequired) {
        DEBUG_BREAK_IF(device->getDeviceInfo().preemptionSupported != false);
    }

    if (isProfilingEnabled() && eventBuilder.getEvent()) {
        eventBuilder.getEvent()->setSubmitTimeStamp();

        auto hwTimestampNode = eventBuilder.getEvent()->getHwTimeStampNode();
        if (hwTimestampNode) {
            getGpgpuCommandStreamReceiver().makeResident(*hwTimestampNode->getBaseGraphicsAllocation());
        }

        if (isPerfCountersEnabled()) {
            getGpgpuCommandStreamReceiver().makeResident(*eventBuilder.getEvent()->getHwPerfCounterNode()->getBaseGraphicsAllocation());
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

    auto allocNeedsFlushDC = false;
    if (!device->isFullRangeSvm()) {
        if (std::any_of(getGpgpuCommandStreamReceiver().getResidencyAllocations().begin(), getGpgpuCommandStreamReceiver().getResidencyAllocations().end(), [](const auto allocation) { return allocation->isFlushL3Required(); })) {
            allocNeedsFlushDC = true;
        }
    }

    auto memoryCompressionState = getGpgpuCommandStreamReceiver().getMemoryCompressionState(auxTranslationRequired, device->getHardwareInfo());

    DispatchFlags dispatchFlags(
        {},                                                                                         //csrDependencies
        &timestampPacketDependencies.barrierNodes,                                                  //barrierTimestampPacketNodes
        {},                                                                                         //pipelineSelectArgs
        this->flushStamp->getStampReference(),                                                      //flushStampReference
        getThrottle(),                                                                              //throttle
        ClPreemptionHelper::taskPreemptionMode(getDevice(), multiDispatchInfo),                     //preemptionMode
        numGrfRequired,                                                                             //numGrfRequired
        L3CachingSettings::l3CacheOn,                                                               //l3CacheSettings
        kernel->getThreadArbitrationPolicy(),                                                       //threadArbitrationPolicy
        kernel->getAdditionalKernelExecInfo(),                                                      //additionalKernelExecInfo
        kernel->getExecutionType(),                                                                 //kernelExecutionType
        memoryCompressionState,                                                                     //memoryCompressionState
        getSliceCount(),                                                                            //sliceCount
        blocking,                                                                                   //blocking
        shouldFlushDC(commandType, printfHandler) || allocNeedsFlushDC,                             //dcFlush
        multiDispatchInfo.usesSlm() || multiDispatchInfo.peekParentKernel(),                        //useSLM
        true,                                                                                       //guardCommandBufferWithPipeControl
        commandType == CL_COMMAND_NDRANGE_KERNEL,                                                   //GSBA32BitRequired
        requiresCoherency,                                                                          //requiresCoherency
        (QueuePriority::LOW == priority),                                                           //lowPriority
        implicitFlush,                                                                              //implicitFlush
        !eventBuilder.getEvent() || getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), //outOfOrderExecutionAllowed
        false,                                                                                      //epilogueRequired
        usePerDssBackedBuffer,                                                                      //usePerDssBackedBuffer
        kernel->isSingleSubdevicePreferred(),                                                       //useSingleSubdevice
        useGlobalAtomics,                                                                           //useGlobalAtomics
        kernel->areMultipleSubDevicesInContext(),                                                   //areMultipleSubDevicesInContext
        kernel->requiresMemoryMigration()                                                           //memoryMigrationRequired
    );

    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = mediaSamplerRequired;
    dispatchFlags.pipelineSelectArgs.specialPipelineSelectMode = specialPipelineSelectMode;

    if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() && !clearDependenciesForSubCapture) {
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(dispatchFlags.csrDependencies, getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OutOfCsr);
        dispatchFlags.csrDependencies.makeResident(getGpgpuCommandStreamReceiver());
    }

    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    if (anyUncacheableArgs) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3CacheOff;
    } else if (!kernel->areStatelessWritesUsed()) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3AndL1On;
    }

    if (this->dispatchHints != 0) {
        dispatchFlags.engineHints = this->dispatchHints;
        dispatchFlags.epilogueRequired = true;
    }

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyPreFlushTask(this);
    }

    if (enqueueProperties.blitPropertiesContainer->size() > 0) {
        const auto newTaskCount = bcsCsr->blitBuffer(*enqueueProperties.blitPropertiesContainer, false, this->isProfilingEnabled(), getDevice());
        this->updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), newTaskCount);
        dispatchFlags.implicitFlush = true;
    }

    PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout, "preemption = %d.\n", static_cast<int>(dispatchFlags.preemptionMode));
    CompletionStamp completionStamp = getGpgpuCommandStreamReceiver().flushTask(
        commandStream,
        commandStreamStart,
        *dsh,
        *ioh,
        getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
        taskLevel,
        dispatchFlags,
        getDevice());

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyFlushTask(completionStamp.taskCount);
    }

    return completionStamp;
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::enqueueBlocked(
    uint32_t commandType,
    Surface **surfaces,
    size_t surfaceCount,
    const MultiDispatchInfo &multiDispatchInfo,
    TimestampPacketDependencies &timestampPacketDependencies,
    std::unique_ptr<KernelOperation> &blockedCommandsData,
    const EnqueueProperties &enqueueProperties,
    EventsRequest &eventsRequest,
    EventBuilder &externalEventBuilder,
    std::unique_ptr<PrintfHandler> &&printfHandler,
    CommandStreamReceiver *bcsCsr) {

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
    auto outEvent = eventBuilder->getEvent();

    //update queue taskCount
    taskCount = outEvent->getCompletionStamp();

    std::unique_ptr<Command> command;
    bool storeTimestampPackets = false;

    if (blockedCommandsData) {
        if (enqueueProperties.blitPropertiesContainer) {
            blockedCommandsData->blitPropertiesContainer = *enqueueProperties.blitPropertiesContainer;
            blockedCommandsData->bcsCsr = bcsCsr;
            blockedCommandsData->blitEnqueue = true;
        }

        storeTimestampPackets = (timestampPacketContainer != nullptr);
    }

    if (enqueueProperties.operation != EnqueueProperties::Operation::GpuKernel) {
        command = std::make_unique<CommandWithoutKernel>(*this, blockedCommandsData);
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

        PreemptionMode preemptionMode = ClPreemptionHelper::taskPreemptionMode(getDevice(), multiDispatchInfo);
        bool slmUsed = multiDispatchInfo.usesSlm() || multiDispatchInfo.peekParentKernel();
        command = std::make_unique<CommandComputeKernel>(*this,
                                                         blockedCommandsData,
                                                         allSurfaces,
                                                         shouldFlushDC(commandType, printfHandler.get()),
                                                         slmUsed,
                                                         commandType == CL_COMMAND_NDRANGE_KERNEL,
                                                         std::move(printfHandler),
                                                         preemptionMode,
                                                         multiDispatchInfo.peekMainKernel(),
                                                         (uint32_t)multiDispatchInfo.size());
    }
    if (storeTimestampPackets) {
        command->setTimestampPacketNode(*timestampPacketContainer, std::move(timestampPacketDependencies));
        command->setEventsRequest(eventsRequest);
    } else if (this->context->getRootDeviceIndices().size() > 1) {
        command->setEventsRequest(eventsRequest);
    }

    outEvent->setCommand(std::move(command));

    eventBuilder->addParentEvents(ArrayRef<const cl_event>(eventsRequest.eventWaitList, eventsRequest.numEventsInWaitList));
    eventBuilder->addParentEvent(this->virtualEvent);
    eventBuilder->finalize();

    if (this->virtualEvent) {
        this->virtualEvent->decRefInternal();
    }

    this->virtualEvent = outEvent;
}

template <typename GfxFamily>
CompletionStamp CommandQueueHw<GfxFamily>::enqueueCommandWithoutKernel(
    Surface **surfaces,
    size_t surfaceCount,
    LinearStream *commandStream,
    size_t commandStreamStart,
    bool &blocking,
    const EnqueueProperties &enqueueProperties,
    TimestampPacketDependencies &timestampPacketDependencies,
    EventsRequest &eventsRequest,
    EventBuilder &eventBuilder,
    uint32_t taskLevel,
    CsrDependencies &csrDeps,
    CommandStreamReceiver *bcsCsr) {

    CompletionStamp completionStamp = {this->taskCount, this->taskLevel, this->flushStamp->peekStamp()};
    bool flushGpgpuCsr = true;

    if ((enqueueProperties.operation == EnqueueProperties::Operation::Blit) && !isGpgpuSubmissionForBcsRequired(false)) {
        flushGpgpuCsr = false;
    } else {
        csrDeps.makeResident(getGpgpuCommandStreamReceiver());
    }

    if (eventBuilder.getEvent() && isProfilingEnabled()) {
        eventBuilder.getEvent()->setSubmitTimeStamp();
        eventBuilder.getEvent()->setStartTimeStamp();
    }

    if (flushGpgpuCsr) {
        if (timestampPacketContainer) {
            timestampPacketContainer->makeResident(getGpgpuCommandStreamReceiver());
            timestampPacketDependencies.previousEnqueueNodes.makeResident(getGpgpuCommandStreamReceiver());
            timestampPacketDependencies.cacheFlushNodes.makeResident(getGpgpuCommandStreamReceiver());
        }

        for (auto surface : CreateRange(surfaces, surfaceCount)) {
            surface->makeResident(getGpgpuCommandStreamReceiver());
        }

        auto rootDeviceIndex = getDevice().getRootDeviceIndex();
        DispatchFlags dispatchFlags(
            {},                                                                  //csrDependencies
            &timestampPacketDependencies.barrierNodes,                           //barrierTimestampPacketNodes
            {},                                                                  //pipelineSelectArgs
            flushStamp->getStampReference(),                                     //flushStampReference
            getThrottle(),                                                       //throttle
            device->getPreemptionMode(),                                         //preemptionMode
            GrfConfig::NotApplicable,                                            //numGrfRequired
            L3CachingSettings::NotApplicable,                                    //l3CacheSettings
            ThreadArbitrationPolicy::NotPresent,                                 //threadArbitrationPolicy
            AdditionalKernelExecInfo::NotApplicable,                             //additionalKernelExecInfo
            KernelExecutionType::NotApplicable,                                  //kernelExecutionType
            MemoryCompressionState::NotApplicable,                               //memoryCompressionState
            getSliceCount(),                                                     //sliceCount
            blocking,                                                            //blocking
            false,                                                               //dcFlush
            false,                                                               //useSLM
            true,                                                                //guardCommandBufferWithPipeControl
            false,                                                               //GSBA32BitRequired
            false,                                                               //requiresCoherency
            false,                                                               //lowPriority
            (enqueueProperties.operation == EnqueueProperties::Operation::Blit), //implicitFlush
            getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(),      //outOfOrderExecutionAllowed
            false,                                                               //epilogueRequired
            false,                                                               //usePerDssBackedBuffer
            false,                                                               //useSingleSubdevice
            false,                                                               //useGlobalAtomics
            context->containsMultipleSubDevices(rootDeviceIndex),                //areMultipleSubDevicesInContext
            false);                                                              //memoryMigrationRequired

        if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            eventsRequest.fillCsrDependenciesForTimestampPacketContainer(dispatchFlags.csrDependencies, getGpgpuCommandStreamReceiver(), CsrDependencies::DependenciesType::OutOfCsr);
            dispatchFlags.csrDependencies.makeResident(getGpgpuCommandStreamReceiver());
        }

        completionStamp = getGpgpuCommandStreamReceiver().flushTask(
            *commandStream,
            commandStreamStart,
            getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u),
            getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u),
            getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
            taskLevel,
            dispatchFlags,
            getDevice());
    }

    if (enqueueProperties.operation == EnqueueProperties::Operation::Blit) {
        UNRECOVERABLE_IF(!enqueueProperties.blitPropertiesContainer);
        const auto newTaskCount = bcsCsr->blitBuffer(*enqueueProperties.blitPropertiesContainer, false, this->isProfilingEnabled(), getDevice());
        this->updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), newTaskCount);
    }

    return completionStamp;
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
size_t CommandQueueHw<GfxFamily>::calculateHostPtrSizeForImage(const size_t *region, size_t rowPitch, size_t slicePitch, Image *image) {
    auto bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto dstRowPitch = rowPitch ? rowPitch : region[0] * bytesPerPixel;
    auto dstSlicePitch = slicePitch ? slicePitch : ((image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

    return Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, image->getImageDesc().image_type);
}

template <typename GfxFamily>
template <uint32_t cmdType>
void CommandQueueHw<GfxFamily>::enqueueBlit(const MultiDispatchInfo &multiDispatchInfo, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &bcsCsr) {
    auto commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();

    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    EventBuilder eventBuilder;

    setupEvent(eventBuilder, eventsRequest.outEvent, cmdType);
    eventsRequest.setupBcsCsrForOutputEvent(bcsCsr);

    std::unique_ptr<KernelOperation> blockedCommandsData;
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    auto blockQueue = false;
    auto taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, blockQueue, cmdType);
    auto clearAllDependencies = queueDependenciesClearRequired();

    enqueueHandlerHook(cmdType, multiDispatchInfo);
    aubCaptureHook(blocking, clearAllDependencies, multiDispatchInfo);

    if (DebugManager.flags.MakeEachEnqueueBlocking.get()) {
        blocking = true;
    }

    TimestampPacketDependencies timestampPacketDependencies;
    BlitPropertiesContainer blitPropertiesContainer;
    CsrDependencies csrDeps;

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, bcsCsr, CsrDependencies::DependenciesType::All);
    auto allocator = bcsCsr.getTimestampPacketAllocator();

    if (isCacheFlushForBcsRequired() && isGpgpuSubmissionForBcsRequired(blockQueue)) {
        timestampPacketDependencies.cacheFlushNodes.add(allocator->getTag());
    }

    if (!blockQueue && getGpgpuCommandStreamReceiver().isStallingPipeControlOnNextFlushRequired()) {
        timestampPacketDependencies.barrierNodes.add(allocator->getTag());
    }

    obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, clearAllDependencies, bcsCsr);
    csrDeps.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);

    LinearStream *gpgpuCommandStream = {};
    size_t gpgpuCommandStreamStart = {};
    if (isGpgpuSubmissionForBcsRequired(blockQueue)) {
        gpgpuCommandStream = obtainCommandStream<cmdType>(csrDeps, true, blockQueue, multiDispatchInfo, eventsRequest, blockedCommandsData, nullptr, 0, false);
        gpgpuCommandStreamStart = gpgpuCommandStream->getUsed();
    }

    if (eventBuilder.getEvent()) {
        eventBuilder.getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
    }

    blitPropertiesContainer.push_back(processDispatchForBlitEnqueue(bcsCsr, multiDispatchInfo, timestampPacketDependencies,
                                                                    eventsRequest, gpgpuCommandStream, cmdType, blockQueue));

    CompletionStamp completionStamp = {CompletionStamp::notReady, taskLevel, 0};

    const EnqueueProperties enqueueProperties(true, false, false, false, false, &blitPropertiesContainer);

    if (!blockQueue) {
        completionStamp = enqueueCommandWithoutKernel(nullptr, 0, gpgpuCommandStream, gpgpuCommandStreamStart, blocking,
                                                      enqueueProperties, timestampPacketDependencies, eventsRequest,
                                                      eventBuilder, taskLevel, csrDeps, &bcsCsr);

        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
        }

        this->latestSentEnqueueType = enqueueProperties.operation;
    }
    updateFromCompletionStamp(completionStamp, eventBuilder.getEvent());

    if (blockQueue) {
        enqueueBlocked(cmdType, nullptr, 0, multiDispatchInfo, timestampPacketDependencies, blockedCommandsData, enqueueProperties, eventsRequest, eventBuilder, nullptr, &bcsCsr);
    }

    timestampPacketDependencies.moveNodesToNewContainer(*deferredTimestampPackets);

    queueOwnership.unlock();
    commandStreamReceiverOwnership.unlock();

    if (blocking) {
        waitForAllEngines(blockQueue, nullptr);
    }
}

template <typename GfxFamily>
template <uint32_t cmdType, size_t surfaceCount>
void CommandQueueHw<GfxFamily>::dispatchBcsOrGpgpuEnqueue(MultiDispatchInfo &dispatchInfo, Surface *(&surfaces)[surfaceCount], EBuiltInOps::Type builtInOperation, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &csr) {
    const bool blit = EngineHelpers::isBcs(csr.getOsContext().getEngineType());

    if (blit) {
        enqueueBlit<cmdType>(dispatchInfo, numEventsInWaitList, eventWaitList, event, blocking, csr);
    } else {
        auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInOperation,
                                                                                this->getClDevice());
        BuiltInOwnershipWrapper builtInLock(builder, this->context);

        builder.buildDispatchInfos(dispatchInfo);

        enqueueHandler<cmdType>(
            surfaces,
            blocking,
            dispatchInfo,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isBlitAuxTranslationRequired(const MultiDispatchInfo &multiDispatchInfo) {
    return multiDispatchInfo.getKernelObjsForAuxTranslation() &&
           (multiDispatchInfo.getKernelObjsForAuxTranslation()->size() > 0) &&
           (HwHelperHw<GfxFamily>::get().getAuxTranslationMode(device->getHardwareInfo()) == AuxTranslationMode::Blit);
}

} // namespace NEO
