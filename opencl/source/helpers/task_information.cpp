/*
 * Copyright (C) 2018-2022 Intel Corporation
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

CommandMapUnmap::CommandMapUnmap(MapOperationType operationType, MemObj &memObj, MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset, bool readOnly,
                                 CommandQueue &commandQueue)
    : Command(commandQueue), memObj(memObj), copySize(copySize), copyOffset(copyOffset), readOnly(readOnly), operationType(operationType) {
    memObj.incRefInternal();
}

CompletionStamp &CommandMapUnmap::submit(uint32_t taskLevel, bool terminated) {
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
        {},                                                                          // csrDependencies
        nullptr,                                                                     // barrierTimestampPacketNodes
        {},                                                                          // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                // flushStampReference
        commandQueue.getThrottle(),                                                  // throttle
        ClPreemptionHelper::taskPreemptionMode(device, multiDispatch),               // preemptionMode
        GrfConfig::NotApplicable,                                                    // numGrfRequired
        L3CachingSettings::NotApplicable,                                            // l3CacheSettings
        ThreadArbitrationPolicy::NotPresent,                                         // threadArbitrationPolicy
        AdditionalKernelExecInfo::NotApplicable,                                     // additionalKernelExecInfo
        KernelExecutionType::NotApplicable,                                          // kernelExecutionType
        MemoryCompressionState::NotApplicable,                                       // memoryCompressionState
        commandQueue.getSliceCount(),                                                // sliceCount
        true,                                                                        // blocking
        true,                                                                        // dcFlush
        false,                                                                       // useSLM
        !commandQueue.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled(),  // guardCommandBufferWithPipeControl
        false,                                                                       // GSBA32BitRequired
        false,                                                                       // requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,                            // lowPriority
        false,                                                                       // implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(), // outOfOrderExecutionAllowed
        false,                                                                       // epilogueRequired
        false,                                                                       // usePerDssBackedBuffer
        false,                                                                       // useSingleSubdevice
        false,                                                                       // useGlobalAtomics
        false,                                                                       // areMultipleSubDevicesInContext
        false,                                                                       // memoryMigrationRequired
        false);                                                                      // textureCacheFlush

    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(queueCommandStream,
                                                      offset,
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    commandQueue.updateLatestSentEnqueueType(EnqueueProperties::Operation::DependencyResolveOnGpu);

    if (!memObj.isMemObjZeroCopy()) {
        const auto waitStatus = commandQueue.waitUntilComplete(completionStamp.taskCount, {}, completionStamp.flushStamp, false);
        if (waitStatus == WaitStatus::GpuHang) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
            return completionStamp;
        }

        if (operationType == MAP) {
            memObj.transferDataToHostPtr(copySize, copyOffset);
        } else if (!readOnly) {
            DEBUG_BREAK_IF(operationType != UNMAP);
            memObj.transferDataFromHostPtr(copySize, copyOffset);
        }
    }

    return completionStamp;
}

CommandComputeKernel::CommandComputeKernel(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation, std::vector<Surface *> surfaces,
                                           bool flushDC, bool usesSLM, uint32_t commandType, std::unique_ptr<PrintfHandler> &&printfHandler,
                                           PreemptionMode preemptionMode, Kernel *kernel, uint32_t kernelCount)
    : Command(commandQueue, kernelOperation), surfaces(std::move(surfaces)), flushDC(flushDC), slmUsed(usesSLM),
      commandType(commandType), printfHandler(std::move(printfHandler)), kernel(kernel),
      kernelCount(kernelCount), preemptionMode(preemptionMode) {
    UNRECOVERABLE_IF(nullptr == this->kernel);
    kernel->incRefInternal();
}

CommandComputeKernel::~CommandComputeKernel() {
    kernel->decRefInternal();
}

CompletionStamp &CommandComputeKernel::submit(uint32_t taskLevel, bool terminated) {
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
        printfHandler->makeResident(commandStreamReceiver);
    }
    makeTimestampPacketsResident(commandStreamReceiver);

    if (kernelOperation->blitPropertiesContainer.size() > 0) {
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(csrDeps, *bcsCsrForAuxTranslation, CsrDependencies::DependenciesType::All);

        BlitProperties::setupDependenciesForAuxTranslation(kernelOperation->blitPropertiesContainer, *timestampPacketDependencies,
                                                           *currentTimestampPacketNodes, csrDeps,
                                                           commandQueue.getGpgpuCommandStreamReceiver(), *bcsCsrForAuxTranslation);
    }

    if (timestampPacketDependencies && commandQueue.isOOQEnabled()) {
        commandQueue.setupBarrierTimestampForBcsEngines(commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getEngineType(), *timestampPacketDependencies);
    }

    const auto &kernelDescriptor = kernel->getKernelInfo().kernelDescriptor;

    auto memoryCompressionState = commandStreamReceiver.getMemoryCompressionState(kernel->isAuxTranslationRequired(), commandQueue.getDevice().getHardwareInfo());

    DispatchFlags dispatchFlags(
        {},                                                                               // csrDependencies
        nullptr,                                                                          // barrierTimestampPacketNodes
        {false, kernel->isVmeKernel()},                                                   // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                                     // flushStampReference
        commandQueue.getThrottle(),                                                       // throttle
        preemptionMode,                                                                   // preemptionMode
        kernelDescriptor.kernelAttributes.numGrfRequired,                                 // numGrfRequired
        L3CachingSettings::l3CacheOn,                                                     // l3CacheSettings
        kernel->getThreadArbitrationPolicy(),                                             // threadArbitrationPolicy
        kernel->getAdditionalKernelExecInfo(),                                            // additionalKernelExecInfo
        kernel->getExecutionType(),                                                       // kernelExecutionType
        memoryCompressionState,                                                           // memoryCompressionState
        commandQueue.getSliceCount(),                                                     // sliceCount
        true,                                                                             // blocking
        flushDC,                                                                          // dcFlush
        slmUsed,                                                                          // useSLM
        !commandQueue.getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled(),       // guardCommandBufferWithPipeControl
        commandType == CL_COMMAND_NDRANGE_KERNEL,                                         // GSBA32BitRequired
        requiresCoherency,                                                                // requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,                                 // lowPriority
        false,                                                                            // implicitFlush
        commandQueue.getGpgpuCommandStreamReceiver().isNTo1SubmissionModelEnabled(),      // outOfOrderExecutionAllowed
        false,                                                                            // epilogueRequired
        false,                                                                            // usePerDssBackedBuffer
        kernel->isSingleSubdevicePreferred(),                                             // useSingleSubdevice
        kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, // useGlobalAtomics
        kernel->areMultipleSubDevicesInContext(),                                         // areMultipleSubDevicesInContext
        kernel->requiresMemoryMigration(),                                                // memoryMigrationRequired
        commandQueue.isTextureCacheFlushNeeded(this->commandType));                       // textureCacheFlush

    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForTaskCountContainer(dispatchFlags.csrDependencies, commandStreamReceiver);
    }

    const bool isHandlingBarrier = commandQueue.getGpgpuCommandStreamReceiver().isStallingCommandsOnNextFlushRequired();

    if (timestampPacketDependencies) {
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(dispatchFlags.csrDependencies, commandStreamReceiver, CsrDependencies::DependenciesType::OutOfCsr);
        if (isHandlingBarrier) {
            commandQueue.fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
        }
        dispatchFlags.barrierTimestampPacketNodes = &timestampPacketDependencies->barrierNodes;
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
    }

    bool isGpuHangDetected{false};

    if (kernelOperation->blitPropertiesContainer.size() > 0) {
        const auto newTaskCount = bcsCsrForAuxTranslation->flushBcsTask(kernelOperation->blitPropertiesContainer, false, commandQueue.isProfilingEnabled(), commandQueue.getDevice());
        if (newTaskCount) {
            commandQueue.updateBcsTaskCount(bcsCsrForAuxTranslation->getOsContext().getEngineType(), *newTaskCount);
        } else {
            isGpuHangDetected = true;
        }
    }
    commandQueue.updateLatestSentEnqueueType(EnqueueProperties::Operation::GpuKernel);

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyFlushTask(completionStamp.taskCount);
    }

    if (printfHandler) {
        const auto waitStatus = commandQueue.waitUntilComplete(completionStamp.taskCount, {}, completionStamp.flushStamp, false);
        if (waitStatus == WaitStatus::GpuHang) {
            isGpuHangDetected = true;
        }

        if (!printfHandler->printEnqueueOutput()) {
            isGpuHangDetected = true;
        }
    }

    for (auto surface : surfaces) {
        delete surface;
    }
    surfaces.clear();

    if (isGpuHangDetected) {
        completionStamp.taskCount = CompletionStamp::gpuHang;
    }

    return completionStamp;
}

bool CommandWithoutKernel::dispatchBlitOperation() {
    auto bcsCsr = kernelOperation->bcsCsr;
    UNRECOVERABLE_IF(bcsCsr == nullptr);

    UNRECOVERABLE_IF(kernelOperation->blitPropertiesContainer.size() != 1);
    auto &blitProperties = *kernelOperation->blitPropertiesContainer.begin();
    eventsRequest.fillCsrDependenciesForTimestampPacketContainer(blitProperties.csrDependencies, *bcsCsr, CsrDependencies::DependenciesType::All);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->cacheFlushNodes);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->previousEnqueueNodes);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestampPacketDependencies->barrierNodes);
    blitProperties.outputTimestampPacket = currentTimestampPacketNodes->peekNodes()[0];

    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForTaskCountContainer(blitProperties.csrDependencies, *bcsCsr);
    }

    const auto newTaskCount = bcsCsr->flushBcsTask(kernelOperation->blitPropertiesContainer, false, commandQueue.isProfilingEnabled(), commandQueue.getDevice());
    if (!newTaskCount) {
        return false;
    }

    commandQueue.updateBcsTaskCount(bcsCsr->getOsContext().getEngineType(), *newTaskCount);
    commandQueue.setLastBcsPacket(bcsCsr->getOsContext().getEngineType());

    return true;
}

CompletionStamp &CommandWithoutKernel::submit(uint32_t taskLevel, bool terminated) {
    if (terminated) {
        this->terminated = true;
        return completionStamp;
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

    auto enqueueOperationType = EnqueueProperties::Operation::DependencyResolveOnGpu;

    if (kernelOperation->blitEnqueue) {
        enqueueOperationType = EnqueueProperties::Operation::Blit;

        UNRECOVERABLE_IF(!barrierNodes);
        if (commandStreamReceiver.isStallingCommandsOnNextFlushRequired()) {
            barrierNodes->add(commandStreamReceiver.getTimestampPacketAllocator()->getTag());
        }
    }

    if (timestampPacketDependencies && commandQueue.isOOQEnabled()) {
        commandQueue.setupBarrierTimestampForBcsEngines(commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getEngineType(), *timestampPacketDependencies);
    }

    auto rootDeviceIndex = commandStreamReceiver.getRootDeviceIndex();
    DispatchFlags dispatchFlags(
        {},                                                                    // csrDependencies
        barrierNodes,                                                          // barrierTimestampPacketNodes
        {},                                                                    // pipelineSelectArgs
        commandQueue.flushStamp->getStampReference(),                          // flushStampReference
        commandQueue.getThrottle(),                                            // throttle
        commandQueue.getDevice().getPreemptionMode(),                          // preemptionMode
        GrfConfig::NotApplicable,                                              // numGrfRequired
        L3CachingSettings::NotApplicable,                                      // l3CacheSettings
        ThreadArbitrationPolicy::NotPresent,                                   // threadArbitrationPolicy
        AdditionalKernelExecInfo::NotApplicable,                               // additionalKernelExecInfo
        KernelExecutionType::NotApplicable,                                    // kernelExecutionType
        MemoryCompressionState::NotApplicable,                                 // memoryCompressionState
        commandQueue.getSliceCount(),                                          // sliceCount
        true,                                                                  // blocking
        false,                                                                 // dcFlush
        false,                                                                 // useSLM
        !commandStreamReceiver.isUpdateTagFromWaitEnabled(),                   // guardCommandBufferWithPipeControl
        false,                                                                 // GSBA32BitRequired
        false,                                                                 // requiresCoherency
        commandQueue.getPriority() == QueuePriority::LOW,                      // lowPriority
        false,                                                                 // implicitFlush
        commandStreamReceiver.isNTo1SubmissionModelEnabled(),                  // outOfOrderExecutionAllowed
        false,                                                                 // epilogueRequired
        false,                                                                 // usePerDssBackedBuffer
        false,                                                                 // useSingleSubdevice
        false,                                                                 // useGlobalAtomics
        commandQueue.getContext().containsMultipleSubDevices(rootDeviceIndex), // areMultipleSubDevicesInContext
        false,                                                                 // memoryMigrationRequired
        false);                                                                // textureCacheFlush

    if (commandQueue.getContext().getRootDeviceIndices().size() > 1) {
        eventsRequest.fillCsrDependenciesForTaskCountContainer(dispatchFlags.csrDependencies, commandStreamReceiver);
    }

    const bool isHandlingBarrier = commandQueue.getGpgpuCommandStreamReceiver().isStallingCommandsOnNextFlushRequired();

    if (commandStreamReceiver.peekTimestampPacketWriteEnabled()) {
        eventsRequest.fillCsrDependenciesForTimestampPacketContainer(dispatchFlags.csrDependencies, commandStreamReceiver, CsrDependencies::DependenciesType::OutOfCsr);
        if (isHandlingBarrier) {
            commandQueue.fillCsrDependenciesWithLastBcsPackets(dispatchFlags.csrDependencies);
        }
        makeTimestampPacketsResident(commandStreamReceiver);
    }

    gtpinNotifyPreFlushTask(&commandQueue);

    completionStamp = commandStreamReceiver.flushTask(*kernelOperation->commandStream,
                                                      0,
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 0u),
                                                      &commandQueue.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 0u),
                                                      taskLevel,
                                                      dispatchFlags,
                                                      commandQueue.getDevice());

    if (isHandlingBarrier) {
        commandQueue.clearLastBcsPackets();
    }

    if (kernelOperation->blitEnqueue) {
        if (!dispatchBlitOperation()) {
            completionStamp.taskCount = CompletionStamp::gpuHang;
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

Command::Command(CommandQueue &commandQueue, std::unique_ptr<KernelOperation> &kernelOperation)
    : commandQueue(commandQueue), kernelOperation(std::move(kernelOperation)) {}
} // namespace NEO
