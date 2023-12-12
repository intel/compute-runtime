/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/program/sync_buffer_handler.inl"
#include "shared/source/utilities/range.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/source/helpers/dispatch_info_builder.h"
#include "opencl/source/helpers/enqueue_properties.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/migration_controller.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/utilities/cl_logger.h"

#include <algorithm>

namespace NEO {
struct RootDeviceEnvironment;

template <typename GfxFamily>
template <uint32_t commandType, size_t surfaceCount>
cl_int CommandQueueHw<GfxFamily>::enqueueHandler(Surface *(&surfaces)[surfaceCount],
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
    std::unique_ptr<KernelObjsForAuxTranslation> kernelObjsForAuxTranslation;
    MultiDispatchInfo multiDispatchInfo(kernel);

    auto auxTranslationMode = AuxTranslationMode::None;

    kernel->updateAuxTranslationRequired();
    if (kernel->isAuxTranslationRequired()) {
        kernelObjsForAuxTranslation = kernel->fillWithKernelObjsForAuxTranslation();

        if (!kernelObjsForAuxTranslation->empty()) {
            auxTranslationMode = GfxCoreHelperHw<GfxFamily>::getAuxTranslationMode(device->getHardwareInfo());
        }
        multiDispatchInfo.setKernelObjsForAuxTranslation(std::move(kernelObjsForAuxTranslation));
    }

    if (AuxTranslationMode::Builtin == auxTranslationMode) {
        auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::auxTranslation, getClDevice());
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
            return CL_SUCCESS;
        }
    }

    if constexpr (commandType == CL_COMMAND_NDRANGE_KERNEL) {
        if (!multiDispatchInfo.empty()) {
            for (auto &dispatchInfo : multiDispatchInfo) {
                auto nwgs = dispatchInfo.getNumberOfWorkgroups();

                for (auto i = 0u; i < workDim; i++) {
                    uint64_t dimension = static_cast<uint64_t>(nwgs[i]);
                    if (dimension > std::numeric_limits<uint32_t>::max()) {
                        return CL_INVALID_GLOBAL_WORK_SIZE;
                    }
                }
            }
        }
    }

    if (AuxTranslationMode::Builtin == auxTranslationMode) {
        dispatchAuxTranslationBuiltin(multiDispatchInfo, AuxTranslationDirection::NonAuxToAux);
    }

    if (AuxTranslationMode::Blit == auxTranslationMode) {
        setupBlitAuxTranslation(multiDispatchInfo);
    }

    return enqueueHandler<commandType>(surfaces, blocking, multiDispatchInfo, numEventsInWaitList, eventWaitList, event);
}

template <typename GfxFamily>
template <uint32_t commandType>
cl_int CommandQueueHw<GfxFamily>::enqueueHandler(Surface **surfacesForResidency,
                                                 size_t numSurfaceForResidency,
                                                 bool blocking,
                                                 const MultiDispatchInfo &multiDispatchInfo,
                                                 cl_uint numEventsInWaitList,
                                                 const cl_event *eventWaitList,
                                                 cl_event *event) {

    if (multiDispatchInfo.empty() && !isCommandWithoutKernel(commandType)) {
        const auto enqueueResult = enqueueHandler<CL_COMMAND_MARKER>(nullptr, 0, blocking, multiDispatchInfo,
                                                                     numEventsInWaitList, eventWaitList, event);
        if (enqueueResult != CL_SUCCESS) {
            return enqueueResult;
        }

        if (event) {
            castToObjectOrAbort<Event>(*event)->setCmdType(commandType);
        }
        return CL_SUCCESS;
    }

    TagNodeBase *hwTimeStamps = nullptr;
    CommandStreamReceiver &computeCommandStreamReceiver = getGpgpuCommandStreamReceiver();

    if (NEO::debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.get()) {
        auto pSvmAllocMgr = this->context->getSVMAllocsManager();
        pSvmAllocMgr->prefetchSVMAllocs(this->getDevice(), computeCommandStreamReceiver);
    }

    EventBuilder eventBuilder;
    setupEvent(eventBuilder, event, commandType);

    const bool isMarkerWithPostSyncWrite = (CL_COMMAND_MARKER == commandType) && ((eventBuilder.getEvent() && eventBuilder.getEvent()->isProfilingEnabled()) || multiDispatchInfo.peekBuiltinOpParams().bcsSplit);

    std::unique_ptr<KernelOperation> blockedCommandsData;
    std::unique_ptr<PrintfHandler> printfHandler;
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);
    auto commandStreamReceiverOwnership = computeCommandStreamReceiver.obtainUniqueOwnership();

    registerGpgpuCsrClient();

    auto blockQueue = false;
    TaskCountType taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, numEventsInWaitList, eventWaitList, blockQueue, commandType);

    enqueueHandlerHook(commandType, multiDispatchInfo);

    bool clearDependenciesForSubCapture = false;
    aubCaptureHook(blocking, clearDependenciesForSubCapture, multiDispatchInfo);

    const bool clearAllDependencies = (queueDependenciesClearRequired() || clearDependenciesForSubCapture);

    if (debugManager.flags.MakeEachEnqueueBlocking.get()) {
        blocking = true;
    }

    TimestampPacketDependencies timestampPacketDependencies;
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    CsrDependencies csrDeps;
    BlitPropertiesContainer blitPropertiesContainer;

    if (this->context->getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, computeCommandStreamReceiver);
    }

    const bool enqueueWithBlitAuxTranslation = isBlitAuxTranslationRequired(multiDispatchInfo);
    const auto &hwInfo = this->getDevice().getHardwareInfo();
    auto &productHelper = getDevice().getProductHelper();
    bool canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies = false;
    bool isNonStallingIoqBarrier = (CL_COMMAND_BARRIER == commandType) && !isOOQEnabled() && (debugManager.flags.OptimizeIoqBarriersHandling.get() != 0);
    const bool isNonStallingIoqBarrierWithDependencies = isNonStallingIoqBarrier && (eventsRequest.numEventsInWaitList > 0);

    if (computeCommandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies = this->peekLatestSentEnqueueOperation() == EnqueueProperties::Operation::GpuKernel &&
                                                                   productHelper.isResolveDependenciesByPipeControlsSupported(hwInfo, this->isOOQEnabled(), this->taskCount, computeCommandStreamReceiver);
        if (false == clearDependenciesForSubCapture) {
            if (false == canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies) {
                eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, computeCommandStreamReceiver, CsrDependencies::DependenciesType::onCsr);
            }
            eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, computeCommandStreamReceiver, CsrDependencies::DependenciesType::outOfCsr);
        }
        auto allocator = computeCommandStreamReceiver.getTimestampPacketAllocator();

        size_t nodesCount = 0u;
        if (isCacheFlushCommand(commandType) || isMarkerWithPostSyncWrite || isNonStallingIoqBarrierWithDependencies) {
            nodesCount = 1;
        } else if (!multiDispatchInfo.empty()) {
            if (isOOQEnabled() && !event) {
                // TSP not needed. Release current node.
                timestampPacketContainer->moveNodesToNewContainer(*deferredTimestampPackets);
            } else {
                nodesCount = estimateTimestampPacketNodesCount(multiDispatchInfo);
            }
        }

        if (isCacheFlushForBcsRequired() && enqueueWithBlitAuxTranslation) {
            // Cache flush for aux translation is always required (if supported)
            timestampPacketDependencies.cacheFlushNodes.add(allocator->getTag());
        }

        if (nodesCount > 0) {
            obtainNewTimestampPacketNodes(nodesCount, timestampPacketDependencies.previousEnqueueNodes, clearAllDependencies, computeCommandStreamReceiver);
            if (false == canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies && timestampPacketDependencies.previousEnqueueNodes.peekNodes().size() > 0) {
                csrDeps.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);
            }
        }
    }

    auto &commandStream = *obtainCommandStream<commandType>(csrDeps, false, blockQueue, multiDispatchInfo, eventsRequest,
                                                            blockedCommandsData, surfacesForResidency, numSurfaceForResidency, canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies, isMarkerWithPostSyncWrite);
    auto commandStreamStart = commandStream.getUsed();

    if (canUsePipeControlInsteadOfSemaphoresForOnCsrDependencies) {
        UNRECOVERABLE_IF(relaxedOrderingForGpgpuAllowed(1)); // IOQ has >=1 dependencies
        PipeControlArgs args;
        args.csStallOnly = true;
        args.hdcPipelineFlush = true;
        args.unTypedDataPortCacheFlush = true;
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStream, args);
    }

    if (this->context->getRootDeviceIndices().size() > 1) {
        TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStream, csrDeps);
    }

    if (enqueueWithBlitAuxTranslation) {
        processDispatchForBlitAuxTranslation(*getBcsForAuxTranslation(), multiDispatchInfo, blitPropertiesContainer,
                                             timestampPacketDependencies, eventsRequest, blockQueue);
    }

    if (eventBuilder.getEvent() && computeCommandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        eventBuilder.getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
        eventBuilder.getEvent()->addTimestampPacketNodes(timestampPacketDependencies.nonAuxToAuxNodes);
        eventBuilder.getEvent()->addTimestampPacketNodes(timestampPacketDependencies.auxToNonAuxNodes);
    }

    bool flushDependenciesForNonKernelCommand = false;
    bool relaxedOrderingEnabled = false;
    bool programBarrierInTaskStream = false;
    if (multiDispatchInfo.empty() == false) {
        relaxedOrderingEnabled = relaxedOrderingForGpgpuAllowed(static_cast<uint32_t>(csrDeps.timestampPacketContainer.size()));
        programBarrierInTaskStream = !relaxedOrderingEnabled && !getGpgpuCommandStreamReceiver().isMultiTileOperationEnabled() && isStallingCommandsOnNextFlushRequired() && !isBlitAuxTranslationRequired(multiDispatchInfo);
        if (programBarrierInTaskStream) {
            CsrDependencies csrDeps{};
            fillCsrDependenciesWithLastBcsPackets(csrDeps);
            TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, csrDeps, false);

            setupBarrierTimestampForBcsEngines(getGpgpuCommandStreamReceiver().getOsContext().getEngineType(), timestampPacketDependencies);
            getGpgpuCommandStreamReceiver().programStallingCommandsForBarrier(commandStream, &timestampPacketDependencies.barrierNodes, isDcFlushRequiredOnStallingCommandsOnNextFlush());

            clearLastBcsPackets();
            setStallingCommandsOnNextFlush(false);
        }
        processDispatchForKernels<commandType>(multiDispatchInfo, printfHandler, eventBuilder.getEvent(),
                                               hwTimeStamps, blockQueue, csrDeps, blockedCommandsData.get(),
                                               timestampPacketDependencies, relaxedOrderingEnabled);
    } else if (isCacheFlushCommand(commandType)) {
        processDispatchForCacheFlush(surfacesForResidency, numSurfaceForResidency, &commandStream, csrDeps);
    } else if (computeCommandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        if (CL_COMMAND_BARRIER == commandType && !isNonStallingIoqBarrier) {
            setStallingCommandsOnNextFlush(true);
            const bool isDcFlushRequiredOnBarrier = NEO::debugManager.flags.SkipDcFlushOnBarrierWithoutEvents.get() == 0 || event;
            setDcFlushRequiredOnStallingCommandsOnNextFlush(isDcFlushRequiredOnBarrier);
            this->splitBarrierRequired = true;
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

        if (isMarkerWithPostSyncWrite) {
            setStallingCommandsOnNextFlush(true);
            flushDependenciesForNonKernelCommand = true;
        }

        if (flushDependenciesForNonKernelCommand) {
            if (isNonStallingIoqBarrierWithDependencies) {
                relaxedOrderingEnabled = relaxedOrderingForGpgpuAllowed(static_cast<uint32_t>(csrDeps.timestampPacketContainer.size()));
            }

            if (relaxedOrderingEnabled) {
                RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(commandStream);
            }

            TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, csrDeps, relaxedOrderingEnabled);
        }

        if (isNonStallingIoqBarrierWithDependencies) {
            auto node = this->timestampPacketContainer->peekNodes()[0];

            if (getGpgpuCommandStreamReceiver().isMultiTileOperationEnabled()) {
                node->setPacketsUsed(getGpgpuCommandStreamReceiver().getActivePartitions());
            }

            TimestampPacketHelper::nonStallingContextEndNodeSignal<GfxFamily>(commandStream, *node, getGpgpuCommandStreamReceiver().isMultiTileOperationEnabled());
        }

        if (isMarkerWithPostSyncWrite) {
            if (numEventsInWaitList == 0) {
                computeCommandStreamReceiver.programComputeBarrierCommand(commandStream);
            }
            processDispatchForMarkerWithTimestampPacket(*this, &commandStream, eventsRequest, csrDeps);
        }
    } else if (isMarkerWithPostSyncWrite) {
        processDispatchForMarker(*this, &commandStream, eventsRequest, csrDeps);
    }
    TagNodeBase *multiRootEventSyncStamp = nullptr;
    if (eventBuilder.getEvent() && eventBuilder.getEvent()->getContext()->getRootDeviceIndices().size() > 1 &&
        !(multiDispatchInfo.empty() && CL_COMMAND_MARKER != commandType)) {
        multiRootEventSyncStamp = eventBuilder.getEvent()->getMultiRootTimestampSyncNode();
        if (!blockQueue) {
            this->getGpgpuCommandStreamReceiver().makeResident(*multiRootEventSyncStamp->getBaseGraphicsAllocation());
        }
        processSignalMultiRootDeviceNode(&commandStream, multiRootEventSyncStamp);
        if (CL_COMMAND_MARKER == commandType) {
            flushDependenciesForNonKernelCommand = true;
        }
    }

    CompletionStamp completionStamp = {CompletionStamp::notReady, taskLevel, 0};
    const EnqueueProperties enqueueProperties(false, !multiDispatchInfo.empty(), isCacheFlushCommand(commandType),
                                              flushDependenciesForNonKernelCommand, isMarkerWithPostSyncWrite, programBarrierInTaskStream, &blitPropertiesContainer);

    if (!blockQueue && isOOQEnabled()) {
        setupBarrierTimestampForBcsEngines(computeCommandStreamReceiver.getOsContext().getEngineType(), timestampPacketDependencies);
    }

    bool migratedMemory = false;

    if (!blockQueue && multiDispatchInfo.peekMainKernel() && multiDispatchInfo.peekMainKernel()->requiresMemoryMigration()) {
        for (auto &arg : multiDispatchInfo.peekMainKernel()->getMemObjectsToMigrate()) {
            MigrationController::handleMigration(*this->context, computeCommandStreamReceiver, arg.second);
            migratedMemory = true;
        }
    }
    if (!blockQueue) {

        if (enqueueProperties.operation == EnqueueProperties::Operation::GpuKernel) {
            csrDeps.makeResident(computeCommandStreamReceiver);

            completionStamp = enqueueNonBlocked(
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
                relaxedOrderingEnabled,
                commandType);
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
                nullptr,
                relaxedOrderingEnabled);
        } else {
            UNRECOVERABLE_IF(enqueueProperties.operation != EnqueueProperties::Operation::EnqueueWithoutSubmission);

            auto maxTaskCountCurrentRootDevice = this->taskCount;

            for (auto eventId = 0u; eventId < numEventsInWaitList; eventId++) {
                auto event = castToObject<Event>(eventWaitList[eventId]);

                if (event->getCommandQueue() && event->getCommandQueue()->getDevice().getRootDeviceIndex() == this->getDevice().getRootDeviceIndex()) {
                    maxTaskCountCurrentRootDevice = std::max(maxTaskCountCurrentRootDevice, event->peekTaskCount());
                }
            }

            // inherit data from event_wait_list and previous packets
            completionStamp.flushStamp = this->flushStamp->peekStamp();
            completionStamp.taskCount = maxTaskCountCurrentRootDevice;
            completionStamp.taskLevel = taskLevel;

            if (eventBuilder.getEvent() && isProfilingEnabled()) {
                eventBuilder.getEvent()->setSubmitTimeStamp();
                eventBuilder.getEvent()->setStartTimeStamp();
            }

            // check if we have BCS associated, if so we need to make sure it is completed as well
            if (eventBuilder.getEvent() && this->bcsQueueEngineType.has_value()) {
                eventBuilder.getEvent()->setupBcs(this->getBcsCommandStreamReceiver(*bcsQueueEngineType)->getOsContext().getEngineType());
            }
        }
        if (eventBuilder.getEvent()) {
            eventBuilder.getEvent()->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
        }

        this->latestSentEnqueueType = enqueueProperties.operation;
    }

    if (completionStamp.taskCount > CompletionStamp::notReady) {
        if (deferredTimestampPackets.get()) {
            timestampPacketContainer->moveNodesToNewContainer(*deferredTimestampPackets);
        }
        return CommandQueue::getErrorCodeFromTaskCount(completionStamp.taskCount);
    }

    updateFromCompletionStamp(completionStamp, eventBuilder.getEvent());

    if (blockQueue) {
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
                       nullptr,
                       multiRootEventSyncStamp);
    }

    if (deferredTimestampPackets.get()) {
        timestampPacketDependencies.moveNodesToNewContainer(*deferredTimestampPackets);
        csrDeps.copyNodesToNewContainer(*deferredTimestampPackets);
    }
    if (deferredMultiRootSyncNodes.get()) {
        csrDeps.copyRootDeviceSyncNodesToNewContainer(*deferredMultiRootSyncNodes);
    }

    commandStreamReceiverOwnership.unlock();
    queueOwnership.unlock();

    if (blocking) {
        auto waitStatus = WaitStatus::ready;
        auto &builtinOpParams = multiDispatchInfo.peekBuiltinOpParams();
        if (builtinOpParams.userPtrForPostOperationCpuCopy) {
            waitStatus = waitForAllEngines(blockQueue, (blockQueue ? nullptr : printfHandler.get()), false);
            if (waitStatus == WaitStatus::gpuHang) {
                return CL_OUT_OF_RESOURCES;
            }

            auto hostPtrAlloc = builtinOpParams.transferAllocation;
            UNRECOVERABLE_IF(nullptr == hostPtrAlloc);
            auto size = hostPtrAlloc->getUnderlyingBufferSize();
            [[maybe_unused]] int cpuCopyStatus = memcpy_s(builtinOpParams.userPtrForPostOperationCpuCopy, size, hostPtrAlloc->getUnderlyingBuffer(), size);
            DEBUG_BREAK_IF(cpuCopyStatus != 0);

            waitStatus = waitForAllEngines(blockQueue, (blockQueue ? nullptr : printfHandler.get()), true);
        } else {
            waitStatus = waitForAllEngines(blockQueue, (blockQueue ? nullptr : printfHandler.get()), true);
        }

        if (waitStatus == WaitStatus::gpuHang) {
            return CL_OUT_OF_RESOURCES;
        }
    }

    if (migratedMemory) {
        computeCommandStreamReceiver.flushBatchedSubmissions();
        computeCommandStreamReceiver.flushTagUpdate();
    }

    return CL_SUCCESS;
}

template <typename GfxFamily>
template <uint32_t commandType>
void CommandQueueHw<GfxFamily>::processDispatchForKernels(const MultiDispatchInfo &multiDispatchInfo,
                                                          std::unique_ptr<PrintfHandler> &printfHandler,
                                                          Event *event,
                                                          TagNodeBase *&hwTimeStamps,
                                                          bool blockQueue,
                                                          CsrDependencies &csrDeps,
                                                          KernelOperation *blockedCommandsData,
                                                          TimestampPacketDependencies &timestampPacketDependencies,
                                                          bool relaxedOrderingEnabled) {
    TagNodeBase *hwPerfCounter = nullptr;
    getClFileLogger().dumpKernelArgs(&multiDispatchInfo);

    printfHandler.reset(PrintfHandler::create(multiDispatchInfo, device->getDevice()));
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

    if (event && this->isProfilingEnabled()) {
        // Get allocation for timestamps
        hwTimeStamps = event->getHwTimeStampNode();
    }

    if (event && this->isPerfCountersEnabled()) {
        hwPerfCounter = event->getHwPerfCounterNode();
    }

    HardwareInterfaceWalkerArgs dispatchWalkerArgs = {};
    dispatchWalkerArgs.blockedCommandsData = blockedCommandsData;
    dispatchWalkerArgs.hwTimeStamps = hwTimeStamps;
    dispatchWalkerArgs.hwPerfCounter = hwPerfCounter;
    dispatchWalkerArgs.timestampPacketDependencies = &timestampPacketDependencies;
    dispatchWalkerArgs.currentTimestampPacketNodes = timestampPacketContainer.get();
    dispatchWalkerArgs.commandType = commandType;
    dispatchWalkerArgs.event = event;
    dispatchWalkerArgs.relaxedOrderingEnabled = relaxedOrderingEnabled;

    HardwareInterface<GfxFamily>::dispatchWalkerCommon(*this, multiDispatchInfo, csrDeps, dispatchWalkerArgs);

    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
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
                                                                        uint32_t commandType, bool queueBlocked, TagNodeBase *multiRootDeviceEventSync) {
    auto blitDirection = ClBlitProperties::obtainBlitDirection(commandType);

    auto blitProperties = ClBlitProperties::constructProperties(blitDirection, blitCommandStreamReceiver,
                                                                multiDispatchInfo.peekBuiltinOpParams());
    if (!queueBlocked) {
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(blitProperties.csrDependencies, blitCommandStreamReceiver,
                                                                     CsrDependencies::DependenciesType::all);

        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.cacheFlushNodes);
        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);
        blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies.barrierNodes);
    }
    blitProperties.multiRootDeviceEventSync = multiRootDeviceEventSync;
    auto currentTimestampPacketNode = timestampPacketContainer->peekNodes().at(0);
    blitProperties.outputTimestampPacket = currentTimestampPacketNode;

    if (commandStream) {
        if (timestampPacketDependencies.cacheFlushNodes.peekNodes().size() > 0) {
            auto cacheFlushTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketDependencies.cacheFlushNodes.peekNodes()[0]);
            PipeControlArgs args;
            args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, device->getRootDeviceEnvironment());
            MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandStream,
                PostSyncMode::immediateData,
                cacheFlushTimestampPacketGpuAddress,
                0,
                device->getRootDeviceEnvironment(),
                args);
        }
    }
    return blitProperties;
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForBlitAuxTranslation(CommandStreamReceiver &bcsCsr,
                                                                     const MultiDispatchInfo &multiDispatchInfo,
                                                                     BlitPropertiesContainer &blitPropertiesContainer,
                                                                     TimestampPacketDependencies &timestampPacketDependencies,
                                                                     const EventsRequest &eventsRequest, bool queueBlocked) {
    const auto rootDeviceIndex = getDevice().getRootDeviceIndex();
    const auto nodesAllocator = getGpgpuCommandStreamReceiver().getTimestampPacketAllocator();
    const auto numKernelObjs = multiDispatchInfo.getKernelObjsForAuxTranslation()->size();
    blitPropertiesContainer.resize(numKernelObjs * 2);

    auto bufferIndex = 0;
    for (const auto &kernelObj : *multiDispatchInfo.getKernelObjsForAuxTranslation()) {
        GraphicsAllocation *allocation = nullptr;
        if (kernelObj.type == KernelObjForAuxTranslation::Type::MEM_OBJ) {
            const auto buffer = static_cast<Buffer *>(kernelObj.object);
            allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
        } else {
            DEBUG_BREAK_IF(kernelObj.type != KernelObjForAuxTranslation::Type::GFX_ALLOC);
            allocation = static_cast<GraphicsAllocation *>(kernelObj.object);
        }
        {
            // Aux to NonAux
            blitPropertiesContainer[bufferIndex] = BlitProperties::constructPropertiesForAuxTranslation(
                AuxTranslationDirection::AuxToNonAux, allocation, getGpgpuCommandStreamReceiver().getClearColorAllocation());
            const auto auxToNonAuxNode = nodesAllocator->getTag();
            timestampPacketDependencies.auxToNonAuxNodes.add(auxToNonAuxNode);
        }

        {
            // NonAux to Aux
            blitPropertiesContainer[bufferIndex + numKernelObjs] = BlitProperties::constructPropertiesForAuxTranslation(
                AuxTranslationDirection::NonAuxToAux, allocation, getGpgpuCommandStreamReceiver().getClearColorAllocation());
            const auto nonAuxToAuxNode = nodesAllocator->getTag();
            timestampPacketDependencies.nonAuxToAuxNodes.add(nonAuxToAuxNode);
        }
        bufferIndex++;
    }

    if (!queueBlocked) {
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, bcsCsr, CsrDependencies::DependenciesType::all);
        BlitProperties::setupDependenciesForAuxTranslation(blitPropertiesContainer, timestampPacketDependencies,
                                                           *this->timestampPacketContainer, csrDeps,
                                                           getGpgpuCommandStreamReceiver(), bcsCsr);
        setStallingCommandsOnNextFlush(true);
        setDcFlushRequiredOnStallingCommandsOnNextFlush(true);
    }

    eventsRequest.setupBcsCsrForOutputEvent(bcsCsr);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForCacheFlush(Surface **surfaces,
                                                             size_t numSurfaces,
                                                             LinearStream *commandStream,
                                                             CsrDependencies &csrDeps) {

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(*commandStream, csrDeps, false);

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
void CommandQueueHw<GfxFamily>::processSignalMultiRootDeviceNode(LinearStream *commandStream,
                                                                 TagNodeBase *node) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, device->getRootDeviceEnvironment());
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *commandStream,
        PostSyncMode::immediateData,
        node->getGpuAddress() + node->getContextEndOffset(),
        std::numeric_limits<uint64_t>::max(),
        device->getRootDeviceEnvironment(),
        args);
}
template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::processDispatchForMarkerWithTimestampPacket(CommandQueue &commandQueue,
                                                                            LinearStream *commandStream,
                                                                            EventsRequest &eventsRequest,
                                                                            CsrDependencies &csrDeps) {
    auto currentTimestampPacketNode = commandQueue.getTimestampPacketContainer()->peekNodes().at(0);

    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(*currentTimestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(*currentTimestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextStartGpuAddress, false);
    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress, false);

    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*currentTimestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(*currentTimestampPacketNode);

    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextEndGpuAddress, false);
    EncodeStoreMMIO<GfxFamily>::encode(*commandStream, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress, false);
}

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::obtainTaskLevelAndBlockedStatus(TaskCountType &taskLevel, cl_uint &numEventsInWaitList, const cl_event *&eventWaitList, bool &blockQueueStatus, unsigned int commandType) {
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
bool CommandQueueHw<GfxFamily>::isTaskLevelUpdateRequired(const TaskCountType &taskLevel, const cl_event *eventWaitList, const cl_uint &numEventsInWaitList, unsigned int commandType) {
    bool updateTaskLevel = true;
    // if we are blocked by user event then no update
    if (taskLevel == CompletionStamp::notReady) {
        updateTaskLevel = false;
    }
    // if we are executing command without kernel then it will inherit state from
    // previous commands, barrier is exception
    if (isCommandWithoutKernel(commandType) && commandType != CL_COMMAND_BARRIER) {
        updateTaskLevel = false;
    }
    // ooq special cases starts here
    if (this->isOOQEnabled()) {
        // if no wait list and barrier , do not update task level
        if (eventWaitList == nullptr && commandType != CL_COMMAND_BARRIER) {
            updateTaskLevel = false;
        }
        // if we have waitlist then deduce task level from waitlist and check if it is higher then current task level of queue
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
    TaskCountType taskLevel,
    PrintfHandler *printfHandler,
    bool relaxedOrderingEnabled,
    uint32_t commandType) {

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
    for (auto surface : createRange(surfaces, surfaceCount)) {
        surface->makeResident(getGpgpuCommandStreamReceiver());
        if (!surface->allowsL3Caching()) {
            anyUncacheableArgs = true;
        }
    }

    auto mediaSamplerRequired = false;
    uint32_t numGrfRequired = GrfConfig::defaultGrfNumber;
    auto systolicPipelineSelectMode = false;
    Kernel *kernel = nullptr;
    bool auxTranslationRequired = false;
    bool useGlobalAtomics = false;

    for (auto &dispatchInfo : multiDispatchInfo) {
        if (kernel != dispatchInfo.getKernel()) {
            kernel = dispatchInfo.getKernel();
        } else {
            continue;
        }
        kernel->makeResident(getGpgpuCommandStreamReceiver());
        mediaSamplerRequired |= kernel->isVmeKernel();
        auto numGrfRequiredByKernel = static_cast<uint32_t>(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.numGrfRequired);
        numGrfRequired = std::max(numGrfRequired, numGrfRequiredByKernel);
        systolicPipelineSelectMode |= kernel->requiresSystolicPipelineSelectMode();
        auxTranslationRequired |= kernel->isAuxTranslationRequired();
        if (kernel->hasUncacheableStatelessArgs()) {
            anyUncacheableArgs = true;
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

    dsh = &getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 0u);
    ioh = &getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 0u);

    auto allocNeedsFlushDC = false;
    if (!device->isFullRangeSvm()) {
        if (std::any_of(getGpgpuCommandStreamReceiver().getResidencyAllocations().begin(), getGpgpuCommandStreamReceiver().getResidencyAllocations().end(), [](const auto allocation) { return allocation->isFlushL3Required(); })) {
            allocNeedsFlushDC = true;
        }
    }

    auto memoryCompressionState = getGpgpuCommandStreamReceiver().getMemoryCompressionState(auxTranslationRequired);
    bool hasStallingCmds = enqueueProperties.hasStallingCmds || (!relaxedOrderingEnabled && (eventsRequest.numEventsInWaitList > 0 || timestampPacketDependencies.previousEnqueueNodes.peekNodes().size() > 0));

    DispatchFlags dispatchFlags(
        &timestampPacketDependencies.barrierNodes,                                                              // barrierTimestampPacketNodes
        {},                                                                                                     // pipelineSelectArgs
        this->flushStamp->getStampReference(),                                                                  // flushStampReference
        getThrottle(),                                                                                          // throttle
        ClPreemptionHelper::taskPreemptionMode(getDevice(), multiDispatchInfo),                                 // preemptionMode
        numGrfRequired,                                                                                         // numGrfRequired
        L3CachingSettings::l3CacheOn,                                                                           // l3CacheSettings
        kernel->getDescriptor().kernelAttributes.threadArbitrationPolicy,                                       // threadArbitrationPolicy
        kernel->getAdditionalKernelExecInfo(),                                                                  // additionalKernelExecInfo
        kernel->getExecutionType(),                                                                             // kernelExecutionType
        memoryCompressionState,                                                                                 // memoryCompressionState
        getSliceCount(),                                                                                        // sliceCount
        blocking,                                                                                               // blocking
        shouldFlushDC(commandType, printfHandler) || allocNeedsFlushDC,                                         // dcFlush
        multiDispatchInfo.usesSlm(),                                                                            // useSLM
        !getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled() || commandType == CL_COMMAND_FILL_BUFFER, // guardCommandBufferWithPipeControl
        commandType == CL_COMMAND_NDRANGE_KERNEL,                                                               // GSBA32BitRequired
        (QueuePriority::LOW == priority),                                                                       // lowPriority
        implicitFlush,                                                                                          // implicitFlush
        !eventBuilder.getEvent() || getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(),             // outOfOrderExecutionAllowed
        false,                                                                                                  // epilogueRequired
        false,                                                                                                  // usePerDssBackedBuffer
        useGlobalAtomics,                                                                                       // useGlobalAtomics
        kernel->areMultipleSubDevicesInContext(),                                                               // areMultipleSubDevicesInContext
        kernel->requiresMemoryMigration(),                                                                      // memoryMigrationRequired
        isTextureCacheFlushNeeded(commandType),                                                                 // textureCacheFlush
        hasStallingCmds,                                                                                        // hasStallingCmds
        relaxedOrderingEnabled,                                                                                 // hasRelaxedOrderingDependencies
        false,                                                                                                  // stateCacheInvalidation
        isStallingCommandsOnNextFlushRequired(),                                                                // isStallingCommandsOnNextFlushRequired
        isDcFlushRequiredOnStallingCommandsOnNextFlush()                                                        // isDcFlushRequiredOnStallingCommandsOnNextFlush
    );

    dispatchFlags.pipelineSelectArgs.mediaSamplerRequired = mediaSamplerRequired;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = systolicPipelineSelectMode;
    uint32_t lws[3] = {static_cast<uint32_t>(multiDispatchInfo.begin()->getLocalWorkgroupSize().x), static_cast<uint32_t>(multiDispatchInfo.begin()->getLocalWorkgroupSize().y), static_cast<uint32_t>(multiDispatchInfo.begin()->getLocalWorkgroupSize().z)};
    uint32_t groupCount[3] = {static_cast<uint32_t>(multiDispatchInfo.begin()->getNumberOfWorkgroups().x), static_cast<uint32_t>(multiDispatchInfo.begin()->getNumberOfWorkgroups().y), static_cast<uint32_t>(multiDispatchInfo.begin()->getNumberOfWorkgroups().z)};
    dispatchFlags.disableEUFusion = kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion ||
                                    device->getProductHelper().isFusedEuDisabledForDpas(systolicPipelineSelectMode, lws, groupCount, this->getDevice().getHardwareInfo());

    const bool isHandlingBarrier = isStallingCommandsOnNextFlushRequired();

    if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() && !clearDependenciesForSubCapture) {
        if (isHandlingBarrier) {
            fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
        }
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
        auto bcsCsr = getBcsForAuxTranslation();
        const auto newTaskCount = bcsCsr->flushBcsTask(*enqueueProperties.blitPropertiesContainer, false, this->isProfilingEnabled(), getDevice());
        if (newTaskCount > CompletionStamp::notReady) {
            CompletionStamp completionStamp{};
            completionStamp.taskCount = newTaskCount;

            return completionStamp;
        }

        this->updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), newTaskCount);
        dispatchFlags.implicitFlush = true;
    }

    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stdout, "preemption = %d.\n", static_cast<int>(dispatchFlags.preemptionMode));
    CompletionStamp completionStamp = getGpgpuCommandStreamReceiver().flushTask(
        commandStream,
        commandStreamStart,
        dsh,
        ioh,
        &getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0u),
        taskLevel,
        dispatchFlags,
        getDevice());

    if (isHandlingBarrier) {
        clearLastBcsPackets();
        setStallingCommandsOnNextFlush(false);
    }

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
    CommandStreamReceiver *bcsCsr,
    TagNodeBase *multiRootDeviceSyncNode) {

    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    // store previous virtual event as it will add dependecies to new virtual event
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

    // update queue taskCount
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
        // store task data in event
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

        allSurfaces.reserve(allSurfaces.size() + surfaceCount);
        for (auto &surface : createRange(surfaces, surfaceCount)) {
            allSurfaces.push_back(surface->duplicate());
        }

        PreemptionMode preemptionMode = ClPreemptionHelper::taskPreemptionMode(getDevice(), multiDispatchInfo);
        bool slmUsed = multiDispatchInfo.usesSlm();
        command = std::make_unique<CommandComputeKernel>(*this,
                                                         blockedCommandsData,
                                                         std::move(allSurfaces),
                                                         shouldFlushDC(commandType, printfHandler.get()),
                                                         slmUsed,
                                                         commandType,
                                                         std::move(printfHandler),
                                                         preemptionMode,
                                                         multiDispatchInfo.peekMainKernel(),
                                                         (uint32_t)multiDispatchInfo.size(),
                                                         multiRootDeviceSyncNode);
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
    TaskCountType taskLevel,
    CsrDependencies &csrDeps,
    CommandStreamReceiver *bcsCsr,
    bool hasRelaxedOrderingDependencies) {

    CompletionStamp completionStamp = {this->taskCount, this->taskLevel, this->flushStamp->peekStamp()};
    bool flushGpgpuCsr = true;

    if ((enqueueProperties.operation == EnqueueProperties::Operation::Blit) && commandStream == nullptr) {
        flushGpgpuCsr = false;
    } else {
        csrDeps.makeResident(getGpgpuCommandStreamReceiver());
    }

    if (eventBuilder.getEvent() && isProfilingEnabled() && !isBcsSplitInitialized()) {
        eventBuilder.getEvent()->setSubmitTimeStamp();
        eventBuilder.getEvent()->setStartTimeStamp();
    }

    if (flushGpgpuCsr) {
        if (timestampPacketContainer) {
            timestampPacketContainer->makeResident(getGpgpuCommandStreamReceiver());
            timestampPacketDependencies.previousEnqueueNodes.makeResident(getGpgpuCommandStreamReceiver());
            timestampPacketDependencies.cacheFlushNodes.makeResident(getGpgpuCommandStreamReceiver());
        }

        for (auto surface : createRange(surfaces, surfaceCount)) {
            surface->makeResident(getGpgpuCommandStreamReceiver());
        }
        bool stateCacheInvalidationNeeded = false;
        if (getGpgpuCommandStreamReceiver().getDcFlushSupport() && enqueueProperties.operation == EnqueueProperties::Operation::Blit) {
            stateCacheInvalidationNeeded = true;
        }

        auto rootDeviceIndex = getDevice().getRootDeviceIndex();
        DispatchFlags dispatchFlags(
            &timestampPacketDependencies.barrierNodes,                           // barrierTimestampPacketNodes
            {},                                                                  // pipelineSelectArgs
            flushStamp->getStampReference(),                                     // flushStampReference
            getThrottle(),                                                       // throttle
            device->getPreemptionMode(),                                         // preemptionMode
            GrfConfig::notApplicable,                                            // numGrfRequired
            L3CachingSettings::notApplicable,                                    // l3CacheSettings
            ThreadArbitrationPolicy::NotPresent,                                 // threadArbitrationPolicy
            AdditionalKernelExecInfo::notApplicable,                             // additionalKernelExecInfo
            KernelExecutionType::notApplicable,                                  // kernelExecutionType
            MemoryCompressionState::notApplicable,                               // memoryCompressionState
            getSliceCount(),                                                     // sliceCount
            blocking,                                                            // blocking
            false,                                                               // dcFlush
            false,                                                               // useSLM
            !getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled(),       // guardCommandBufferWithPipeControl
            false,                                                               // GSBA32BitRequired
            false,                                                               // lowPriority
            (enqueueProperties.operation == EnqueueProperties::Operation::Blit), // implicitFlush
            getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(),      // outOfOrderExecutionAllowed
            false,                                                               // epilogueRequired
            false,                                                               // usePerDssBackedBuffer
            false,                                                               // useGlobalAtomics
            context->containsMultipleSubDevices(rootDeviceIndex),                // areMultipleSubDevicesInContext
            false,                                                               // memoryMigrationRequired
            false,                                                               // textureCacheFlush
            !hasRelaxedOrderingDependencies,                                     // hasStallingCmds
            hasRelaxedOrderingDependencies,                                      // hasRelaxedOrderingDependencies
            stateCacheInvalidationNeeded,                                        // stateCacheInvalidation
            isStallingCommandsOnNextFlushRequired(),                             // isStallingCommandsOnNextFlushRequired
            isDcFlushRequiredOnStallingCommandsOnNextFlush()                     // isDcFlushRequiredOnStallingCommandsOnNextFlush
        );

        const bool isHandlingBarrier = isStallingCommandsOnNextFlushRequired();

        if (getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
            if (isHandlingBarrier) {
                fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
            }
            dispatchFlags.csrDependencies.makeResident(getGpgpuCommandStreamReceiver());
        }

        completionStamp = getGpgpuCommandStreamReceiver().flushTask(
            *commandStream,
            commandStreamStart,
            &getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 0u),
            &getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 0u),
            &getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0u),
            taskLevel,
            dispatchFlags,
            getDevice());

        if (isHandlingBarrier) {
            clearLastBcsPackets();
            setStallingCommandsOnNextFlush(false);
        }
    }

    if (enqueueProperties.operation == EnqueueProperties::Operation::Blit) {
        UNRECOVERABLE_IF(!enqueueProperties.blitPropertiesContainer);
        const auto newTaskCount = bcsCsr->flushBcsTask(*enqueueProperties.blitPropertiesContainer, false, this->isProfilingEnabled(), getDevice());
        if (newTaskCount > CompletionStamp::notReady) {
            CompletionStamp completionStamp{};
            completionStamp.taskCount = newTaskCount;

            return completionStamp;
        }

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
    auto bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
    auto dstRowPitch = rowPitch ? rowPitch : region[0] * bytesPerPixel;
    auto dstSlicePitch = slicePitch ? slicePitch : ((image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

    return Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, image->getImageDesc().image_type);
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isSplitEnqueueBlitNeeded(TransferDirection transferDirection, size_t transferSize, CommandStreamReceiver &csr) {
    auto bcsSplit = getDevice().isBcsSplitSupported() &&
                    csr.getOsContext().getEngineType() == aub_stream::EngineType::ENGINE_BCS &&
                    transferSize >= minimalSizeForBcsSplit &&
                    transferDirection != TransferDirection::LocalToLocal;

    if (bcsSplit) {
        this->constructBcsEnginesForSplit();
    }

    return bcsSplit;
}

template <typename GfxFamily>
size_t CommandQueueHw<GfxFamily>::getTotalSizeFromRectRegion(const size_t *region) {
    auto size = region[0];
    size *= (region[1] == 0 ? 1 : region[1]);
    size *= (region[2] == 0 ? 1 : region[2]);
    return size;
}

template <typename GfxFamily>
template <uint32_t cmdType>
cl_int CommandQueueHw<GfxFamily>::enqueueBlitSplit(MultiDispatchInfo &dispatchInfo, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &csr) {
    auto ret = CL_SUCCESS;
    this->releaseMainCopyEngine();

    StackVec<std::unique_lock<CommandStreamReceiver::MutexType>, 2u> locks;
    StackVec<CommandStreamReceiver *, 2u> copyEngines;

    auto splitEngines = this->splitEngines;
    if (dispatchInfo.peekBuiltinOpParams().direction == NEO::TransferDirection::HostToLocal) {
        splitEngines = this->h2dEngines;
    } else if (dispatchInfo.peekBuiltinOpParams().direction == NEO::TransferDirection::LocalToHost) {
        splitEngines = this->d2hEngines;
    }

    for (uint32_t i = 0; i < bcsInfoMaskSize; i++) {
        if (splitEngines.test(i)) {
            auto engineType = EngineHelpers::mapBcsIndexToEngineType(i, true);
            auto bcs = getBcsCommandStreamReceiver(engineType);
            if (bcs) {
                locks.push_back(std::move(bcs->obtainUniqueOwnership()));
                copyEngines.push_back(bcs);
            }
        }
    }

    DEBUG_BREAK_IF(copyEngines.size() == 0);
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);

    if (isOOQEnabled() && (isStallingCommandsOnNextFlushRequired() || this->splitBarrierRequired)) {
        this->setStallingCommandsOnNextFlush(true);
        this->setDcFlushRequiredOnStallingCommandsOnNextFlush(true);
        NullSurface s;
        Surface *surfaces[] = {&s};
        BuiltinOpParams params{};
        params.bcsSplit = true;
        MultiDispatchInfo di(params);
        ret = enqueueHandler<CL_COMMAND_MARKER>(surfaces,
                                                false,
                                                di,
                                                numEventsInWaitList,
                                                eventWaitList,
                                                nullptr);
        DEBUG_BREAK_IF(ret != CL_SUCCESS);
    }

    TimestampPacketContainer splitNodes;
    TimestampPacketContainer previousEnqueueNode;
    previousEnqueueNode.swapNodes(*this->timestampPacketContainer);

    auto srcOffset = dispatchInfo.peekBuiltinOpParams().srcOffset.x;
    auto dstOffset = dispatchInfo.peekBuiltinOpParams().dstOffset.x;
    auto size = dispatchInfo.peekBuiltinOpParams().size.x;
    auto remainingSize = size;

    EventBuilder externalEventBuilder;
    EventBuilder *pEventBuilder = nullptr;
    DEBUG_BREAK_IF(!this->isBcsSplitInitialized());
    if (event && this->isProfilingEnabled()) {
        pEventBuilder = &externalEventBuilder;
        setupEvent(*pEventBuilder, event, cmdType);
        castToObjectOrAbort<Event>(*event)->setSubmitTimeStamp();
        castToObjectOrAbort<Event>(*event)->setStartTimeStamp();
    }

    for (size_t i = 0; i < copyEngines.size(); i++) {
        auto localSize = remainingSize / (copyEngines.size() - i);
        auto localParams = dispatchInfo.peekBuiltinOpParams();
        localParams.size.x = localSize;
        localParams.srcOffset.x = (srcOffset + size - remainingSize);
        localParams.dstOffset.x = (dstOffset + size - remainingSize);

        dispatchInfo.setBuiltinOpParams(localParams);
        remainingSize -= localSize;

        this->timestampPacketContainer->assignAndIncrementNodesRefCounts(previousEnqueueNode);

        ret = enqueueBlit<cmdType>(dispatchInfo, numEventsInWaitList, eventWaitList, remainingSize == 0 ? event : nullptr, false, *copyEngines[i], pEventBuilder);
        DEBUG_BREAK_IF(ret != CL_SUCCESS);

        this->timestampPacketContainer->moveNodesToNewContainer(splitNodes);
    }

    if (event) {
        auto e = castToObjectOrAbort<Event>(*event);
        e->addTimestampPacketNodes(splitNodes);
    }

    this->timestampPacketContainer->swapNodes(splitNodes);
    this->splitBarrierRequired = false;

    queueOwnership.unlock();
    for (auto &lock : locks) {
        lock.unlock();
    }

    if (blocking) {
        ret = this->finish();
    }

    return ret;
}

template <typename GfxFamily>
template <uint32_t cmdType>
cl_int CommandQueueHw<GfxFamily>::enqueueBlit(const MultiDispatchInfo &multiDispatchInfo, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &bcsCsr, EventBuilder *pExternalEventBuilder) {
    auto bcsCommandStreamReceiverOwnership = bcsCsr.obtainUniqueOwnership();
    std::unique_lock<NEO::CommandStreamReceiver::MutexType> commandStreamReceiverOwnership;

    registerBcsCsrClient(bcsCsr);

    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);
    EventBuilder internalEventBuilder;
    EventBuilder *pEventBuilder = nullptr;

    if (pExternalEventBuilder) {
        DEBUG_BREAK_IF(!this->isBcsSplitInitialized() || !this->isProfilingEnabled());
        pEventBuilder = pExternalEventBuilder;
    } else {
        pEventBuilder = &internalEventBuilder;
        setupEvent(*pEventBuilder, eventsRequest.outEvent, cmdType);
    }
    eventsRequest.setupBcsCsrForOutputEvent(bcsCsr);

    std::unique_ptr<KernelOperation> blockedCommandsData;
    TakeOwnershipWrapper<CommandQueueHw<GfxFamily>> queueOwnership(*this);
    if (debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.get() != 1) {
        commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
    }

    auto blockQueue = false;
    bool migratedMemory = false;
    TaskCountType taskLevel = 0u;
    obtainTaskLevelAndBlockedStatus(taskLevel, eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, blockQueue, cmdType);
    auto clearAllDependencies = queueDependenciesClearRequired();

    enqueueHandlerHook(cmdType, multiDispatchInfo);
    aubCaptureHook(blocking, clearAllDependencies, multiDispatchInfo);

    if (debugManager.flags.MakeEachEnqueueBlocking.get()) {
        blocking = true;
    }

    TimestampPacketDependencies timestampPacketDependencies;
    TagNodeBase *multiRootEventSyncStamp = nullptr;
    BlitPropertiesContainer blitPropertiesContainer;
    CsrDependencies csrDeps;

    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, bcsCsr, CsrDependencies::DependenciesType::all);
    if (this->context->getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, bcsCsr);
    }
    auto allocator = bcsCsr.getTimestampPacketAllocator();

    if (!blockQueue) {
        setupBarrierTimestampForBcsEngines(bcsCsr.getOsContext().getEngineType(), timestampPacketDependencies);
        if (isOOQEnabled()) {
            TimestampPacketContainer clearBarrierNodes;
            timestampPacketDependencies.barrierNodes.swapNodes(clearBarrierNodes);
        }
    }
    processBarrierTimestampForBcsEngine(bcsCsr.getOsContext().getEngineType(), timestampPacketDependencies);

    if (!blockQueue && this->getContext().getRootDeviceIndices().size() > 1) {
        migratedMemory = migrateMultiGraphicsAllocationsIfRequired(multiDispatchInfo.peekBuiltinOpParams(), bcsCsr);
    }

    auto gpgpuSubmission = isGpgpuSubmissionForBcsRequired(blockQueue, timestampPacketDependencies);
    if (isCacheFlushForBcsRequired() && gpgpuSubmission) {
        timestampPacketDependencies.cacheFlushNodes.add(allocator->getTag());
    }

    obtainNewTimestampPacketNodes(1, timestampPacketDependencies.previousEnqueueNodes, clearAllDependencies, bcsCsr);
    csrDeps.timestampPacketContainer.push_back(&timestampPacketDependencies.previousEnqueueNodes);

    if (pEventBuilder->getEvent()) {
        pEventBuilder->getEvent()->addTimestampPacketNodes(*timestampPacketContainer);
    }
    if (pEventBuilder->getEvent() && pEventBuilder->getEvent()->getContext()->getRootDeviceIndices().size() > 1) {
        multiRootEventSyncStamp = pEventBuilder->getEvent()->getMultiRootTimestampSyncNode();
        bcsCsr.makeResident(*multiRootEventSyncStamp->getBaseGraphicsAllocation());
    }

    CompletionStamp completionStamp = {CompletionStamp::notReady, taskLevel, 0};

    const EnqueueProperties enqueueProperties(true, false, false, false, false, false, &blitPropertiesContainer);

    LinearStream *gpgpuCommandStream = {};
    size_t gpgpuCommandStreamStart = {};
    if (gpgpuSubmission) {
        registerGpgpuCsrClient();

        if (debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.get() == 1) {
            commandStreamReceiverOwnership = getGpgpuCommandStreamReceiver().obtainUniqueOwnership();
        }
        gpgpuCommandStream = obtainCommandStream<cmdType>(csrDeps, true, blockQueue, multiDispatchInfo, eventsRequest, blockedCommandsData, nullptr, 0, false, false);
        gpgpuCommandStreamStart = gpgpuCommandStream->getUsed();
    }

    blitPropertiesContainer.push_back(processDispatchForBlitEnqueue(bcsCsr, multiDispatchInfo, timestampPacketDependencies,
                                                                    eventsRequest, gpgpuCommandStream, cmdType, blockQueue, multiRootEventSyncStamp));

    if (!blockQueue) {
        completionStamp = enqueueCommandWithoutKernel(nullptr, 0, gpgpuCommandStream, gpgpuCommandStreamStart, blocking,
                                                      enqueueProperties, timestampPacketDependencies, eventsRequest,
                                                      *pEventBuilder, taskLevel, csrDeps, &bcsCsr, false);
        if (completionStamp.taskCount > CompletionStamp::notReady) {
            return CommandQueue::getErrorCodeFromTaskCount(completionStamp.taskCount);
        }

        if (gpgpuSubmission) {
            if (debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.get() == 1) {
                commandStreamReceiverOwnership.unlock();
            }
        }

        if (pEventBuilder->getEvent()) {
            pEventBuilder->getEvent()->flushStamp->replaceStampObject(this->flushStamp->getStampReference());
        }

        this->latestSentEnqueueType = enqueueProperties.operation;

        setLastBcsPacket(bcsCsr.getOsContext().getEngineType());
    }
    updateFromCompletionStamp(completionStamp, pEventBuilder->getEvent());

    if (blockQueue) {
        enqueueBlocked(cmdType, nullptr, 0, multiDispatchInfo, timestampPacketDependencies, blockedCommandsData, enqueueProperties, eventsRequest, *pEventBuilder, nullptr, &bcsCsr, multiRootEventSyncStamp);

        if (gpgpuSubmission) {
            if (debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.get() == 1) {
                commandStreamReceiverOwnership.unlock();
            }
        }
    }

    timestampPacketDependencies.moveNodesToNewContainer(*deferredTimestampPackets);
    csrDeps.copyNodesToNewContainer(*deferredTimestampPackets);
    if (deferredMultiRootSyncNodes.get()) {
        csrDeps.copyRootDeviceSyncNodesToNewContainer(*deferredMultiRootSyncNodes);
    }
    if (debugManager.flags.ForceCsrLockInBcsEnqueueOnlyForGpgpuSubmission.get() != 1) {
        commandStreamReceiverOwnership.unlock();
    }
    queueOwnership.unlock();

    if (migratedMemory) {
        bcsCsr.flushBatchedSubmissions();
        bcsCsr.flushTagUpdate();
    }

    bcsCommandStreamReceiverOwnership.unlock();
    if (blocking) {
        const auto waitStatus = waitForAllEngines(blockQueue, nullptr);
        if (waitStatus == WaitStatus::gpuHang) {
            return CL_OUT_OF_RESOURCES;
        }
    }

    return CL_SUCCESS;
}

template <typename GfxFamily>
template <uint32_t cmdType, size_t surfaceCount>
cl_int CommandQueueHw<GfxFamily>::dispatchBcsOrGpgpuEnqueue(MultiDispatchInfo &dispatchInfo, Surface *(&surfaces)[surfaceCount], EBuiltInOps::Type builtInOperation, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event, bool blocking, CommandStreamReceiver &csr) {
    const bool blit = EngineHelpers::isBcs(csr.getOsContext().getEngineType());

    if (blit) {
        cl_int ret = CL_SUCCESS;

        if (dispatchInfo.peekBuiltinOpParams().bcsSplit) {
            ret = enqueueBlitSplit<cmdType>(dispatchInfo, numEventsInWaitList, eventWaitList, event, blocking, csr);
        } else {
            ret = enqueueBlit<cmdType>(dispatchInfo, numEventsInWaitList, eventWaitList, event, blocking, csr, nullptr);
        }

        return ret;
    } else {
        auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtInOperation,
                                                                                this->getClDevice());
        BuiltInOwnershipWrapper builtInLock(builder, this->context);

        builder.buildDispatchInfos(dispatchInfo);

        return enqueueHandler<cmdType>(
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
           (GfxCoreHelperHw<GfxFamily>::getAuxTranslationMode(device->getHardwareInfo()) == AuxTranslationMode::Blit);
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::relaxedOrderingForGpgpuAllowed(uint32_t numWaitEvents) const {
    auto &gpgpuCsr = getGpgpuCommandStreamReceiver();

    if ((debugManager.flags.DirectSubmissionRelaxedOrdering.get() == 0) || gpgpuCsr.isRecyclingTagForHeapStorageRequired()) {
        return false;
    }

    return RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(gpgpuCsr, numWaitEvents);
}

} // namespace NEO
