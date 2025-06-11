/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/task_information.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/enqueue_common.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/source/helpers/enqueue_properties.h"
#include "opencl/source/helpers/task_information.inl"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {
template void KernelOperation::ResourceCleaner::operator()<LinearStream>(LinearStream *);
template void KernelOperation::ResourceCleaner::operator()<IndirectHeap>(IndirectHeap *);

static_assert(NEO::NonCopyableAndNonMovable<DecRefInternalAtScopeEnd<MemObj>>);

CommandMapUnmap::CommandMapUnmap(MapOperationType operationType, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                                 CommandQueue &commandQueue)
    : Command(commandQueue), memObj(memObj), copySize(copySize), copyOffset(copyOffset), readOnly(readOnly), operationType(operationType) {
    memObj.incRefInternal();
}

CompletionStamp &CommandMapUnmap::submit(TaskCountType taskLevel, bool terminated) {
    DecRefInternalAtScopeEnd decRefInternalAtScopeEnd{memObj};

    if (terminated) {
        this->terminated = true;
        return completionStamp;
    }

    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
    auto commandStreamReceiverOwnership = commandStreamReceiver.obtainUniqueOwnership();
    auto &queueCommandStream = commandQueue.getCS(0);
    size_t offset = queueCommandStream.getUsed();
    MultiDispatchInfo multiDispatch;
    Device &device = commandQueue.getDevice();

    DispatchFlags dispatchFlags(
        nullptr,                                                                     // barrierTimestampPacketNodes
        {},                                                                          // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                // flushStampReference
        commandQueue.getThrottle(),                                                  // throttle
        ClPreemptionHelper::taskPreemptionMode(device, multiDispatch),               // preemptionMode
        GrfConfig::notApplicable,                                                    // numGrfRequired
        L3CachingSettings::notApplicable,                                            // l3CacheSettings
        ThreadArbitrationPolicy::NotPresent,                                         // threadArbitrationPolicy
        AdditionalKernelExecInfo::notApplicable,                                     // additionalKernelExecInfo
        KernelExecutionType::notApplicable,                                          // kernelExecutionType
        MemoryCompressionState::notApplicable,                                       // memoryCompressionState
        commandQueue.getSliceCount(),                                                // sliceCount
        true,                                                                        // blocking
        true,                                                                        // dcFlush
        false,                                                                       // useSLM
        !commandQueue.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled(),  // guardCommandBufferWithPipeControl
        false,                                                                       // GSBA32BitRequired
        commandQueue.getPriority() == QueuePriority::low,                            // lowPriority
        false,                                                                       // implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), // outOfOrderExecutionAllowed
        false,                                                                       // epilogueRequired
        false,                                                                       // usePerDssBackedBuffer
        false,                                                                       // areMultipleSubDevicesInContext
        false,                                                                       // memoryMigrationRequired
        false,                                                                       // textureCacheFlush
        false,                                                                       // hasStallingCmds
        false,                                                                       // hasRelaxedOrderingDependencies
        false,                                                                       // stateCacheInvalidation
        commandQueue.isStallingCommandsOnNextFlushRequired(),                        // isStallingCommandsOnNextFlushRequired
        commandQueue.isDcFlushRequiredOnStallingCommandsOnNextFlush()                // isDcFlushRequiredOnStallingCommandsOnNextFlush
    );

    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(queueCommandStream,
                                                      offset,
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::surfaceState, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    commandQueue.updateLatestSentEnqueueType(EnqueueProperties::Operation::dependencyResolveOnGpu);

    if (!memObj.isMemObjZeroCopy()) {
        const auto waitStatus = commandQueue.waitUntilComplete(completionStamp.taskCount, {}, completionStamp.flushStamp, false);
        if (waitStatus == WaitStatus::gpuHang) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
            return completionStamp;
        }

        if (operationType == MapOperationType::map) {
            memObj.transferDataToHostPtr(copySize, copyOffset);
        } else if (!readOnly) {
            DEBUG_BREAK_IF(operationType != MapOperationType::unmap);
            memObj.transferDataFromHostPtr(copySize, copyOffset);
        }
    }

    return completionStamp;
}

CommandComputeKernel::CommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> surfaces,
                                           bool flushDC, bool usesSLM, uint32_t commandType, std::unique_ptr<PrintfHandler> &&printfHandler,
                                           PreemptionMode preemptionMode, Kernel *kernel, uint32_t kernelCount,
                                           TagNodeBase *multiRootDeviceSyncNode)
    : Command(commandQueue, kernelOperation, nullptr), surfaces(std::move(surfaces)), flushDC(flushDC), slmUsed(usesSLM),
      commandType(commandType), printfHandler(std::move(printfHandler)), kernel(kernel),
      kernelCount(kernelCount), preemptionMode(preemptionMode), multiRootDeviceSyncNode(multiRootDeviceSyncNode) {
    UNRECOVERABLE_IF(nullptr == this->kernel);
    kernel->incRefInternal();
}

CommandComputeKernel::~CommandComputeKernel() {
    kernel->decRefInternal();
}

CompletionStamp &CommandComputeKernel::submit(TaskCountType taskLevel, bool terminated) {
    if (terminated) {
        this->terminated = true;
        for (auto surface : surfaces) {
            delete surface;
        }
        surfaces.clear();
        return completionStamp;
    }
    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
    auto bcsCsrForAuxTranslation = commandQueue.getBcsForAuxTranslation();

    auto commandStreamReceiverOwnership = commandStreamReceiver.obtainUniqueOwnership();

    IndirectHeap *dsh = kernelOperation->dsh.get();
    IndirectHeap *ioh = kernelOperation->ioh.get();
    IndirectHeap *ssh = kernelOperation->ssh.get();

    auto anyUncacheableArgs = false;
    for (auto &surface : surfaces) {
        DEBUG_BREAK_IF(!surface);
        surface->makeResident(commandStreamReceiver);
        if (!surface->allowsL3Caching()) {
            anyUncacheableArgs = true;
        }
    }

    if (printfHandler) {
        printfHandler->makeResident(commandStreamReceiver);
    }
    makeTimestampPacketsResident(commandStreamReceiver);
    if (multiRootDeviceSyncNode != nullptr) {
        commandStreamReceiver.makeResident(*multiRootDeviceSyncNode->getBaseGraphicsAllocation());
    }

    if (kernelOperation->blitPropertiesContainer.size() > 0) {
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, *bcsCsrForAuxTranslation, CsrDependencies::DependenciesType::all);

        BlitProperties::setupDependenciesForAuxTranslation(kernelOperation->blitPropertiesContainer, *timestampPacketDependencies,
                                                           *currentTimestampPacketNodes, csrDeps,
                                                           commandQueue.getGpgpuCommandStreamReceiver(), *bcsCsrForAuxTranslation);
        commandQueue.setStallingCommandsOnNextFlush(true);
        commandQueue.setDcFlushRequiredOnStallingCommandsOnNextFlush(true);
    }

    if (timestampPacketDependencies && commandQueue.isOOQEnabled()) {
        commandQueue.setupBarrierTimestampForBcsEngines(commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getEngineType(), *timestampPacketDependencies);
    }

    const auto &kernelDescriptor = kernel->getKernelInfo().kernelDescriptor;

    auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(kernel->isAuxTranslationRequired());

    DispatchFlags dispatchFlags(
        nullptr,                                                                     // barrierTimestampPacketNodes
        {false, kernel->isVmeKernel()},                                              // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                // flushStampReference
        commandQueue.getThrottle(),                                                  // throttle
        preemptionMode,                                                              // preemptionMode
        kernelDescriptor.kernelAttributes.numGrfRequired,                            // numGrfRequired
        L3CachingSettings::l3CacheOn,                                                // l3CacheSettings
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy,                   // threadArbitrationPolicy
        kernel->getAdditionalKernelExecInfo(),                                       // additionalKernelExecInfo
        kernel->getExecutionType(),                                                  // kernelExecutionType
        memoryCompressionState,                                                      // memoryCompressionState
        commandQueue.getSliceCount(),                                                // sliceCount
        true,                                                                        // blocking
        flushDC,                                                                     // dcFlush
        slmUsed,                                                                     // useSLM
        !commandQueue.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled(),  // guardCommandBufferWithPipeControl
        commandType == CL_COMMAND_NDRANGE_KERNEL,                                    // GSBA32BitRequired
        commandQueue.getPriority() == QueuePriority::low,                            // lowPriority
        false,                                                                       // implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), // outOfOrderExecutionAllowed
        false,                                                                       // epilogueRequired
        false,                                                                       // usePerDssBackedBuffer
        kernel->areMultipleSubDevicesInContext(),                                    // areMultipleSubDevicesInContext
        kernel->requiresMemoryMigration(),                                           // memoryMigrationRequired
        commandQueue.isTextureCacheFlushNeeded(this->commandType),                   // textureCacheFlush
        false,                                                                       // hasStallingCmds
        false,                                                                       // hasRelaxedOrderingDependencies
        false,                                                                       // stateCacheInvalidation
        commandQueue.isStallingCommandsOnNextFlushRequired(),                        // isStallingCommandsOnNextFlushRequired
        commandQueue.isDcFlushRequiredOnStallingCommandsOnNextFlush()                // isDcFlushRequiredOnStallingCommandsOnNextFlush
    );
    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForRootDevices(dispatchFlags.csrDependencies, commandStreamReceiver);
    }

    const bool isHandlingBarrier = commandQueue.isStallingCommandsOnNextFlushRequired();

    if (timestampPacketDependencies) {
        if (isHandlingBarrier) {
            commandQueue.fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
        }
        dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies->barrierNodes;
    }
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = kernel->requiresSystolicPipelineSelectMode();
    if (anyUncacheableArgs) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3CacheOff;
    } else if (!kernel->areStatelessWritesUsed()) {
        dispatchFlags.l3CacheSettings = L3CachingSettings::l3AndL1On;
    }

    if (commandQueue.dispatchHints != 0) {
        dispatchFlags.engineHints = commandQueue.dispatchHints;
        dispatchFlags.epilogueRequired = true;
    }

    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    if (kernel->requiresMemoryMigration()) {
        for (auto &arg : kernel->getMemObjectsToMigrate()) {
            MigrationController::handleMigration(commandQueue.getContext(), commandStreamReceiver, arg.second);
        }
    }

    completionStamp = commandStreamReceiver.flushTask(*kernelOperation->commandStream,
                                                      0,
                                                      dsh,
                                                      ioh,
                                                      ssh,
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    if (isHandlingBarrier) {
        commandQueue.clearLastBcsPackets();
        commandQueue.setStallingCommandsOnNextFlush(false);
    }

    if (kernelOperation->blitPropertiesContainer.size() > 0) {
        const auto newTaskCount = bcsCsrForAuxTranslation->flushBcsTask(kernelOperation->blitPropertiesContainer, false, commandQueue.getDevice());
        if (newTaskCount <= CompletionStamp::notReady) {
            commandQueue.updateBcsTaskCount(bcsCsrForAuxTranslation->getOsContext().getEngineType(), newTaskCount);
        } else {
            completionStamp.taskCount = newTaskCount;
        }
    }
    commandQueue.updateLatestSentEnqueueType(EnqueueProperties::Operation::gpuKernel);

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyFlushTask(completionStamp.taskCount);
    }

    if (printfHandler) {
        const auto waitStatus = commandQueue.waitUntilComplete(completionStamp.taskCount, {}, completionStamp.flushStamp, false);
        if (waitStatus == WaitStatus::gpuHang) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
        }

        if (!printfHandler->printEnqueueOutput()) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
        }
    }

    for (auto surface : surfaces) {
        delete surface;
    }
    surfaces.clear();

    return completionStamp;
}

TaskCountType CommandWithoutKernel::dispatchBlitOperation() {
    auto bcsCsr = kernelOperation->bcsCsr;
    UNRECOVERABLE_IF(bcsCsr == nullptr);

    UNRECOVERABLE_IF(kernelOperation->blitPropertiesContainer.size() != 1);
    auto &blitProperties = *kernelOperation->blitPropertiesContainer.begin();
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(blitProperties.csrDependencies, *bcsCsr, CsrDependencies::DependenciesType::all);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->cacheFlushNodes);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->previousEnqueueNodes);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->barrierNodes);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->multiCsrDependencies);
    blitProperties.blitSyncProperties.outputTimestampPacket = currentTimestampPacketNodes->peekNodes()[0];
    blitProperties.blitSyncProperties.syncMode = (eventsRequest.outEvent && commandQueue.isProfilingEnabled()) ? BlitSyncMode::timestamp : BlitSyncMode::immediate;

    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForRootDevices(blitProperties.csrDependencies, *bcsCsr);
    }

    const auto newTaskCount = bcsCsr->flushBcsTask(kernelOperation->blitPropertiesContainer, false, commandQueue.getDevice());
    if (newTaskCount > CompletionStamp::notReady) {
        return newTaskCount;
    }

    commandQueue.updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), newTaskCount);
    commandQueue.setLastBcsPacket(bcsCsr->getOsContext().getEngineType());

    return newTaskCount;
}

CompletionStamp &CommandWithoutKernel::submit(TaskCountType taskLevel, bool terminated) {
    if (terminated) {
        this->terminated = true;
        return completionStamp;
    }
    for (auto &tagCsrPair : csrDependencies) {
        bool submitStatus = tagCsrPair.first->submitDependencyUpdate(tagCsrPair.second);
        if (!submitStatus) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
            return completionStamp;
        }
    }
    auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();

    if (!kernelOperation) {
        completionStamp.taskCount = commandStreamReceiver.peekTaskCount();
        completionStamp.taskLevel = commandStreamReceiver.peekTaskLevel();
        completionStamp.flushStamp = commandStreamReceiver.obtainCurrentFlushStamp();

        return completionStamp;
    }

    auto barrierNodes = timestampPacketDependencies ? &timestampPacketDependencies->barrierNodes : nullptr;
    auto lockCSR = commandStreamReceiver.obtainUniqueOwnership();

    auto enqueueOperationType = EnqueueProperties::Operation::dependencyResolveOnGpu;

    if (kernelOperation->blitEnqueue) {
        enqueueOperationType = EnqueueProperties::Operation::blit;

        UNRECOVERABLE_IF(!barrierNodes);
        if (commandQueue.isStallingCommandsOnNextFlushRequired()) {
            barrierNodes->add(commandStreamReceiver.getTimestampPacketAllocator()->getTag());
        }
    }

    if (timestampPacketDependencies && commandQueue.isOOQEnabled()) {
        commandQueue.setupBarrierTimestampForBcsEngines(commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getEngineType(), *timestampPacketDependencies);
    }

    auto rootDeviceIndex = commandStreamReceiver.getRootDeviceIndex();
    DispatchFlags dispatchFlags(
        barrierNodes,                                                          // barrierTimestampPacketNodes
        {},                                                                    // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                          // flushStampReference
        commandQueue.getThrottle(),                                            // throttle
        commandQueue.getDevice().getPreemptionMode(),                          // preemptionMode
        GrfConfig::notApplicable,                                              // numGrfRequired
        L3CachingSettings::notApplicable,                                      // l3CacheSettings
        ThreadArbitrationPolicy::NotPresent,                                   // threadArbitrationPolicy
        AdditionalKernelExecInfo::notApplicable,                               // additionalKernelExecInfo
        KernelExecutionType::notApplicable,                                    // kernelExecutionType
        MemoryCompressionState::notApplicable,                                 // memoryCompressionState
        commandQueue.getSliceCount(),                                          // sliceCount
        true,                                                                  // blocking
        false,                                                                 // dcFlush
        false,                                                                 // useSLM
        !commandStreamReceiver.isUpdateTagFromWaitEnabled(),                   // guardCommandBufferWithPipeControl
        false,                                                                 // GSBA32BitRequired
        commandQueue.getPriority() == QueuePriority::low,                      // lowPriority
        false,                                                                 // implicitFlush
        commandStreamReceiver.isNTo1SubmissionModelEnabled(),                  // outOfOrderExecutionAllowed
        false,                                                                 // epilogueRequired
        false,                                                                 // usePerDssBackedBuffer
        commandQueue.getContext().containsMultipleSubDevices(rootDeviceIndex), // areMultipleSubDevicesInContext
        false,                                                                 // memoryMigrationRequired
        false,                                                                 // textureCacheFlush
        false,                                                                 // hasStallingCmds
        false,                                                                 // hasRelaxedOrderingDependencies
        false,                                                                 // stateCacheInvalidation
        commandQueue.isStallingCommandsOnNextFlushRequired(),                  // isStallingCommandsOnNextFlushRequired
        commandQueue.isDcFlushRequiredOnStallingCommandsOnNextFlush()          // isDcFlushRequiredOnStallingCommandsOnNextFlush
    );

    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForRootDevices(dispatchFlags.csrDependencies, commandStreamReceiver);
    }

    const bool isHandlingBarrier = commandQueue.isStallingCommandsOnNextFlushRequired();

    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        if (isHandlingBarrier) {
            commandQueue.fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
        }
        makeTimestampPacketsResident(commandStreamReceiver);
    }

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(*kernelOperation->commandStream,
                                                      0,
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::surfaceState, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    if (isHandlingBarrier) {
        commandQueue.clearLastBcsPackets();
        commandQueue.setStallingCommandsOnNextFlush(false);
    }

    if (kernelOperation->blitEnqueue) {
        auto taskCount = dispatchBlitOperation();
        if (taskCount > CompletionStamp::notReady) {
            completionStamp.taskCount = taskCount;
        }
    }

    commandQueue.updateLatestSentEnqueueType(enqueueOperationType);

    return completionStamp;
}

void Command::setEventsRequest(EventsRequest &eventsRequest) {
    this->eventsRequest = eventsRequest;
    if (eventsRequest.numEventsInWaitList > 0) {
        eventsWaitlist.resize(eventsRequest.numEventsInWaitList);
        auto size = eventsRequest.numEventsInWaitList * sizeof(cl_event);
        memcpy_s(&eventsWaitlist[0], size, eventsRequest.eventWaitList, size);
        this->eventsRequest.eventWaitList = &eventsWaitlist[0];
        for (cl_uint i = 0; i < eventsRequest.numEventsInWaitList; i++) {
            auto event = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
            event->incRefInternal();
        }
    }
}

void Command::setTimestampPacketNode(TimestampPacketContainer &current, TimestampPacketDependencies &&dependencies) {
    currentTimestampPacketNodes = std::make_unique<TimestampPacketContainer>();
    currentTimestampPacketNodes->assignAndIncrementNodesRefCounts(current);

    timestampPacketDependencies = std::make_unique<TimestampPacketDependencies>();
    *timestampPacketDependencies = std::move(dependencies);
}

Command::~Command() {
    if (terminated) {
        if (commandQueue.getTimestampPacketContainer()) {
            std::array<uint32_t, 8u> timestampData;
            timestampData.fill(std::numeric_limits<uint32_t>::max());
            if (currentTimestampPacketNodes.get()) {
                for (auto &node : currentTimestampPacketNodes->peekNodes()) {
                    for (const auto &cmdQueueNode : commandQueue.getTimestampPacketContainer()->peekNodes()) {
                        if (node == cmdQueueNode) {
                            for (uint32_t i = 0; i < node->getPacketsUsed(); i++) {
                                node->assignDataToAllTimestamps(i, timestampData.data());
                            }
                        }
                    }
                }
            }
        }
    } else {
        if (commandQueue.getDeferredTimestampPackets() && timestampPacketDependencies.get()) {
            timestampPacketDependencies->moveNodesToNewContainer(*commandQueue.getDeferredTimestampPackets());
        }
    }

    for (cl_event &eventFromWaitList : eventsWaitlist) {
        auto event = castToObjectOrAbort<Event>(eventFromWaitList);
        event->decRefInternal();
    }
}

void Command::makeTimestampPacketsResident(CommandStreamReceiver &commandStreamReceiver) {
    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        for (cl_event &eventFromWaitList : eventsWaitlist) {
            auto event = castToObjectOrAbort<Event>(eventFromWaitList);
            if (event->getTimestampPacketNodes() && event->getCommandQueue()->getClDevice().getRootDeviceIndex() == commandStreamReceiver.getRootDeviceIndex()) {
                event->getTimestampPacketNodes()->makeResident(commandStreamReceiver);
            }
        }
    }

    if (currentTimestampPacketNodes) {
        currentTimestampPacketNodes->makeResident(commandStreamReceiver);
    }
    if (timestampPacketDependencies) {
        timestampPacketDependencies->cacheFlushNodes.makeResident(commandStreamReceiver);
        timestampPacketDependencies->previousEnqueueNodes.makeResident(commandStreamReceiver);
    }
}

Command::Command(CommandQueue &commandQueue) : commandQueue(commandQueue) {}

Command::Command(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, CsrDependencyContainer *csrDependencies)
    : commandQueue(commandQueue), kernelOperation(std::move(kernelOperation)) {
    if (csrDependencies) {
        this->csrDependencies = *csrDependencies;
    }
}
} // namespace NEO
