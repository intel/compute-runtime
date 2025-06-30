/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/wait_util.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/error_code_helper_l0.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/semaphore/external_semaphore_imp.h"

#include "encode_surface_state_args.h"

#include <cmath>
#include <functional>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
CommandListCoreFamilyImmediate<gfxCoreFamily>::CommandListCoreFamilyImmediate(uint32_t numIddsPerBlock) : BaseClass(numIddsPerBlock) {
    computeFlushMethod = &CommandListCoreFamilyImmediate<gfxCoreFamily>::flushRegularTask;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::checkAvailableSpace(uint32_t numEvents, bool hasRelaxedOrderingDependencies, size_t commandSize, bool requestCommandBufferInLocalMem) {
    this->commandContainer.fillReusableAllocationLists();

    // Command container might have two command buffers - one in local mem (mainly for relaxed ordering and any other specific purposes) and one in system mem for copying into ring buffer.
    // If relaxed ordering is needed in given dispatch or if we need to force Local mem usage, and current command stream is in system memory, swap of command streams is required to ensure local memory.
    // If relaxed ordering is not needed and command buffer is in local mem, then also we need to swap.
    bool swapStreams = false;
    if (hasRelaxedOrderingDependencies) {
        if (NEO::MemoryPoolHelper::isSystemMemoryPool(this->commandContainer.getCommandStream()->getGraphicsAllocation()->getMemoryPool())) {
            swapStreams = true;
        }
    } else {
        if (requestCommandBufferInLocalMem && NEO::MemoryPoolHelper::isSystemMemoryPool(this->commandContainer.getCommandStream()->getGraphicsAllocation()->getMemoryPool())) {
            swapStreams = true;
        } else if (!requestCommandBufferInLocalMem && !NEO::MemoryPoolHelper::isSystemMemoryPool(this->commandContainer.getCommandStream()->getGraphicsAllocation()->getMemoryPool())) {
            swapStreams = true;
        }
    }

    if (swapStreams) {
        if (this->commandContainer.swapStreams()) {
            this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();
        }
    }

    size_t semaphoreSize = NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait() * numEvents;
    if (this->commandContainer.getCommandStream()->getAvailableSpace() < commandSize + semaphoreSize) {
        bool requireSystemMemoryCommandBuffer = !hasRelaxedOrderingDependencies && !requestCommandBufferInLocalMem;

        auto alloc = this->commandContainer.reuseExistingCmdBuffer(requireSystemMemoryCommandBuffer);
        this->commandContainer.addCurrentCommandBufferToReusableAllocationList();

        if (!alloc) {
            alloc = this->commandContainer.allocateCommandBuffer(requireSystemMemoryCommandBuffer);
            this->commandContainer.getCmdBufferAllocations().push_back(alloc);
        }
        this->commandContainer.setCmdBuffer(alloc);
        this->cmdListCurrentStartOffset = 0;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::updateDispatchFlagsWithRequiredStreamState(NEO::DispatchFlags &dispatchFlags) {
    const auto &requiredFrontEndState = this->requiredStreamState.frontEndState;
    dispatchFlags.kernelExecutionType = (requiredFrontEndState.computeDispatchAllWalkerEnable.value == 1)
                                            ? NEO::KernelExecutionType::concurrent
                                            : NEO::KernelExecutionType::defaultType;
    dispatchFlags.disableEUFusion = (requiredFrontEndState.disableEUFusion.value == 1);
    dispatchFlags.additionalKernelExecInfo = (requiredFrontEndState.disableOverdispatch.value == 1)
                                                 ? NEO::AdditionalKernelExecInfo::disableOverdispatch
                                                 : NEO::AdditionalKernelExecInfo::notSet;

    const auto &requiredStateComputeMode = this->requiredStreamState.stateComputeMode;
    dispatchFlags.numGrfRequired = (requiredStateComputeMode.largeGrfMode.value == 1) ? GrfConfig::largeGrfNumber
                                                                                      : GrfConfig::defaultGrfNumber;
    dispatchFlags.threadArbitrationPolicy = requiredStateComputeMode.threadArbitrationPolicy.value;

    const auto &requiredPipelineSelect = this->requiredStreamState.pipelineSelect;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = requiredPipelineSelect.systolicMode.value == 1;
    if (this->containsStatelessUncachedResource) {
        dispatchFlags.l3CacheSettings = NEO::L3CachingSettings::l3CacheOff;
        this->containsStatelessUncachedResource = false;
    } else {
        dispatchFlags.l3CacheSettings = NEO::L3CachingSettings::l3CacheOn;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushBcsTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, bool requireTaskCountUpdate, NEO::AppendOperations appendOperation, NEO::CommandStreamReceiver *csr) {
    NEO::DispatchBcsFlags dispatchBcsFlags(
        this->isSyncModeQueue || requireTaskCountUpdate, // flushTaskCount
        hasStallingCmds,                                 // hasStallingCmds
        hasRelaxedOrderingDependencies                   // hasRelaxedOrderingDependencies
    );
    dispatchBcsFlags.optionalEpilogueCmdStream = getOptionalEpilogueCmdStream(&cmdStreamTask, appendOperation);
    dispatchBcsFlags.dispatchOperation = appendOperation;

    CommandListImp::storeReferenceTsToMappedEvents(true);

    return csr->flushBcsTask(cmdStreamTask, taskStartOffset, dispatchBcsFlags, this->device->getHwInfo());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::handleDebugSurfaceStateUpdate(NEO::IndirectHeap *ssh) {

    NEO::Device *neoDevice = this->device->getNEODevice();
    if (neoDevice->getDebugger() && !neoDevice->getBindlessHeapsHelper()) {
        auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(getCsr(false));
        auto &sshState = csrHw->getSshState();
        bool sshDirty = sshState.updateAndCheck(ssh);

        if (sshDirty) {
            auto surfaceStateSpace = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
            auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

            NEO::EncodeSurfaceStateArgs args;
            args.outMemory = &surfaceState;
            args.graphicsAddress = this->device->getDebugSurface()->getGpuAddress();
            args.size = this->device->getDebugSurface()->getUnderlyingBufferSize();
            args.mocs = this->device->getMOCS(false, false);
            args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
            args.allocation = this->device->getDebugSurface();
            args.gmmHelper = neoDevice->getGmmHelper();
            args.areMultipleSubDevicesInContext = false;
            args.isDebuggerActive = true;
            NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
            *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
template <bool streamStatesSupported>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::handleHeapsAndResidencyForImmediateRegularTask(void *&sshCpuBaseAddress) {
    NEO::IndirectHeap *dsh = nullptr;
    NEO::IndirectHeap *ioh = this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::indirectObject);
    NEO::IndirectHeap *ssh = nullptr;

    const auto bindlessHeapsHelper = this->device->getNEODevice()->getBindlessHeapsHelper();
    const bool useGlobalHeaps = bindlessHeapsHelper != nullptr;
    auto csr = getCsr(false);

    csr->makeResident(*ioh->getGraphicsAllocation());

    if constexpr (streamStatesSupported) {
        if (this->requiredStreamState.stateBaseAddress.indirectObjectBaseAddress.value == NEO::StreamProperty64::initValue) {
            this->requiredStreamState.stateBaseAddress.setPropertiesIndirectState(ioh->getHeapGpuBase(), ioh->getHeapSizeInPages());
        }
    }

    if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        ssh = csr->getGlobalStatelessHeap();
        csr->makeResident(*ssh->getGraphicsAllocation());

        if constexpr (streamStatesSupported) {
            if (this->requiredStreamState.stateBaseAddress.surfaceStateBaseAddress.value == NEO::StreamProperty64::initValue) {
                this->requiredStreamState.stateBaseAddress.setPropertiesSurfaceState(NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps),
                                                                                     NEO::getStateSizeForSsh(*ssh, useGlobalHeaps));
            }
        }
    } else if (this->immediateCmdListHeapSharing) {
        ssh = this->commandContainer.getSurfaceStateHeapReserve().indirectHeapReservation;
        if (ssh->getGraphicsAllocation()) {
            csr->makeResident(*ssh->getGraphicsAllocation());

            if constexpr (streamStatesSupported) {
                this->requiredStreamState.stateBaseAddress.setPropertiesBindingTableSurfaceState(NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateSizeForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateSizeForSsh(*ssh, useGlobalHeaps));
            }
        }
        if (this->dynamicHeapRequired) {
            dsh = this->commandContainer.getDynamicStateHeapReserve().indirectHeapReservation;
            if (dsh->getGraphicsAllocation()) {
                csr->makeResident(*dsh->getGraphicsAllocation());
                if constexpr (streamStatesSupported) {
                    this->requiredStreamState.stateBaseAddress.setPropertiesDynamicState(NEO::getStateBaseAddress(*dsh, useGlobalHeaps),
                                                                                         NEO::getStateSize(*dsh, useGlobalHeaps));
                }
            }
        }
    } else {
        if (this->dynamicHeapRequired) {
            dsh = this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::dynamicState);
            csr->makeResident(*dsh->getGraphicsAllocation());
            if constexpr (streamStatesSupported) {
                this->requiredStreamState.stateBaseAddress.setPropertiesDynamicState(NEO::getStateBaseAddress(*dsh, useGlobalHeaps),
                                                                                     NEO::getStateSize(*dsh, useGlobalHeaps));
            }
        }
        ssh = this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::surfaceState);
        if (ssh) {
            csr->makeResident(*ssh->getGraphicsAllocation());
            if constexpr (streamStatesSupported) {
                this->requiredStreamState.stateBaseAddress.setPropertiesBindingTableSurfaceState(NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateSizeForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps),
                                                                                                 NEO::getStateSizeForSsh(*ssh, useGlobalHeaps));
            }
        }
    }

    if (this->device->getL0Debugger()) {
        csr->makeResident(*this->device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId()));
        csr->makeResident(*this->device->getDebugSurface());
        if (bindlessHeapsHelper) {
            csr->makeResident(*bindlessHeapsHelper->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getGraphicsAllocation());
        }
    }

    if (ssh) {
        sshCpuBaseAddress = ssh->getCpuBase();
        handleDebugSurfaceStateUpdate(ssh);
    }

    csr->setRequiredScratchSizes(this->getCommandListPerThreadScratchSize(0u), this->getCommandListPerThreadScratchSize(1u));
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediateRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds,
                                                                                              bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate) {
    void *sshCpuPointer = nullptr;
    constexpr bool streamStatesSupported = true;

    if (appendOperation == NEO::AppendOperations::kernel) {
        handleHeapsAndResidencyForImmediateRegularTask<streamStatesSupported>(sshCpuPointer);
    }

    NEO::LinearStream *optionalEpilogueCmdStream = getOptionalEpilogueCmdStream(&cmdStreamTask, appendOperation);

    NEO::ImmediateDispatchFlags dispatchFlags{
        &this->requiredStreamState,     // requiredState
        sshCpuPointer,                  // sshCpuBase
        optionalEpilogueCmdStream,      // optionalEpilogueCmdStream
        appendOperation,                // dispatchOperation
        this->isSyncModeQueue,          // blockingAppend
        requireTaskCountUpdate,         // requireTaskCountUpdate
        hasRelaxedOrderingDependencies, // hasRelaxedOrderingDependencies
        hasStallingCmds                 // hasStallingCmds
    };
    CommandListImp::storeReferenceTsToMappedEvents(true);

    return getCsr(false)->flushImmediateTask(cmdStreamTask,
                                             taskStartOffset,
                                             dispatchFlags,
                                             *(this->device->getNEODevice()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediateRegularTaskStateless(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds,
                                                                                                       bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate) {

    void *sshCpuPointer = nullptr;
    constexpr bool streamStatesSupported = false;

    if (appendOperation == NEO::AppendOperations::kernel) {
        handleHeapsAndResidencyForImmediateRegularTask<streamStatesSupported>(sshCpuPointer);
    }

    NEO::LinearStream *optionalEpilogueCmdStream = getOptionalEpilogueCmdStream(&cmdStreamTask, appendOperation);

    NEO::ImmediateDispatchFlags dispatchFlags{
        nullptr,                        // requiredState
        sshCpuPointer,                  // sshCpuBase
        optionalEpilogueCmdStream,      // optionalEpilogueCmdStream
        appendOperation,                // dispatchOperation
        this->isSyncModeQueue,          // blockingAppend
        requireTaskCountUpdate,         // requireTaskCountUpdate
        hasRelaxedOrderingDependencies, // hasRelaxedOrderingDependencies
        hasStallingCmds                 // hasStallingCmds
    };
    CommandListImp::storeReferenceTsToMappedEvents(true);

    return getCsr(false)->flushImmediateTaskStateless(cmdStreamTask,
                                                      taskStartOffset,
                                                      dispatchFlags,
                                                      *(this->device->getNEODevice()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation, bool requireTaskCountUpdate) {
    auto csr = getCsr(false);

    NEO::DispatchFlags dispatchFlags(
        nullptr,                                                          // barrierTimestampPacketNodes
        {},                                                               // pipelineSelectArgs
        nullptr,                                                          // flushStampReference
        NEO::getThrottleFromPowerSavingUint(csr->getUmdPowerHintValue()), // throttle
        this->getCommandListPreemptionMode(),                             // preemptionMode
        GrfConfig::notApplicable,                                         // numGrfRequired
        NEO::L3CachingSettings::l3CacheOn,                                // l3CacheSettings
        NEO::ThreadArbitrationPolicy::NotPresent,                         // threadArbitrationPolicy
        NEO::AdditionalKernelExecInfo::notApplicable,                     // additionalKernelExecInfo
        NEO::KernelExecutionType::notApplicable,                          // kernelExecutionType
        NEO::MemoryCompressionState::notApplicable,                       // memoryCompressionState
        NEO::QueueSliceCount::defaultSliceCount,                          // sliceCount
        this->isSyncModeQueue,                                            // blocking
        this->isSyncModeQueue,                                            // dcFlush
        this->getCommandListSLMEnable(),                                  // useSLM
        this->isSyncModeQueue || requireTaskCountUpdate,                  // guardCommandBufferWithPipeControl
        false,                                                            // gsba32BitRequired
        false,                                                            // lowPriority
        true,                                                             // implicitFlush
        csr->isNTo1SubmissionModelEnabled(),                              // outOfOrderExecutionAllowed
        false,                                                            // epilogueRequired
        false,                                                            // usePerDssBackedBuffer
        this->device->getNEODevice()->getNumGenericSubDevices() > 1,      // areMultipleSubDevicesInContext
        false,                                                            // memoryMigrationRequired
        false,                                                            // textureCacheFlush
        hasStallingCmds,                                                  // hasStallingCmds
        hasRelaxedOrderingDependencies,                                   // hasRelaxedOrderingDependencies
        false,                                                            // stateCacheInvalidation
        false,                                                            // isStallingCommandsOnNextFlushRequired
        false                                                             // isDcFlushRequiredOnStallingCommandsOnNextFlush
    );

    dispatchFlags.optionalEpilogueCmdStream = getOptionalEpilogueCmdStream(&cmdStreamTask, appendOperation);

    auto ioh = (this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::indirectObject));
    NEO::IndirectHeap *dsh = nullptr;
    NEO::IndirectHeap *ssh = nullptr;

    if (appendOperation == NEO::AppendOperations::kernel) {
        this->updateDispatchFlagsWithRequiredStreamState(dispatchFlags);
        csr->setRequiredScratchSizes(this->getCommandListPerThreadScratchSize(0u), this->getCommandListPerThreadScratchSize(1u));

        if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
            ssh = csr->getGlobalStatelessHeap();
        } else if (this->immediateCmdListHeapSharing) {
            auto &sshReserveConfig = this->commandContainer.getSurfaceStateHeapReserve();
            if (sshReserveConfig.indirectHeapReservation->getGraphicsAllocation()) {
                ssh = sshReserveConfig.indirectHeapReservation;
            }
            auto &dshReserveConfig = this->commandContainer.getDynamicStateHeapReserve();
            if (this->dynamicHeapRequired && dshReserveConfig.indirectHeapReservation->getGraphicsAllocation()) {
                dsh = dshReserveConfig.indirectHeapReservation;
            }
        } else {
            dsh = this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::dynamicState);
            ssh = this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::surfaceState);
        }

        if (this->device->getL0Debugger() && NEO::Debugger::isDebugEnabled(this->internalUsage)) {
            csr->makeResident(*this->device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId()));
            csr->makeResident(*this->device->getDebugSurface());
            if (this->device->getNEODevice()->getBindlessHeapsHelper()) {
                csr->makeResident(*this->device->getNEODevice()->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getGraphicsAllocation());
            }
        }

        NEO::Device *neoDevice = this->device->getNEODevice();
        if (neoDevice->getDebugger() && this->immediateCmdListHeapSharing && !neoDevice->getBindlessHeapsHelper()) {
            auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(csr);
            auto sshStateCopy = csrHw->getSshState();
            bool sshDirty = sshStateCopy.updateAndCheck(ssh);

            if (sshDirty) {
                auto surfaceStateSpace = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
                auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

                NEO::EncodeSurfaceStateArgs args;
                args.outMemory = &surfaceState;
                args.graphicsAddress = this->device->getDebugSurface()->getGpuAddress();
                args.size = this->device->getDebugSurface()->getUnderlyingBufferSize();
                args.mocs = this->device->getMOCS(false, false);
                args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
                args.allocation = this->device->getDebugSurface();
                args.gmmHelper = neoDevice->getGmmHelper();
                args.areMultipleSubDevicesInContext = false;
                args.isDebuggerActive = true;
                NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
                *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
            }
        }
    }

    CommandListImp::storeReferenceTsToMappedEvents(true);

    return csr->flushTask(
        cmdStreamTask,
        taskStartOffset,
        dsh,
        ioh,
        ssh,
        csr->peekTaskLevel(),
        dispatchFlags,
        *(this->device->getNEODevice()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation,
                                                                                                    bool copyOffloadSubmission, bool requireTaskCountUpdate,
                                                                                                    MutexLock *outerLock,
                                                                                                    std::unique_lock<std::mutex> *outerLockForIndirect) {
    const auto copyOffloadModeForOperation = getCopyOffloadModeForOperation(copyOffloadSubmission);
    return executeCommandListImmediateWithFlushTaskImpl(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies, appendOperation, requireTaskCountUpdate, getCmdQImmediate(copyOffloadModeForOperation), outerLock, outerLockForIndirect);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::executeCommandListImmediateWithFlushTaskImpl(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::AppendOperations appendOperation,
                                                                                                               bool requireTaskCountUpdate, CommandQueue *cmdQ,
                                                                                                               MutexLock *outerLock,
                                                                                                               std::unique_lock<std::mutex> *outerLockForIndirect) {
    auto cmdQImp = static_cast<CommandQueueImp *>(cmdQ);
    this->commandContainer.removeDuplicatesFromResidencyContainer();

    auto commandStream = this->commandContainer.getCommandStream();
    size_t commandStreamStart = this->cmdListCurrentStartOffset;
    if (appendOperation == NEO::AppendOperations::cmdList && this->dispatchCmdListBatchBufferAsPrimary) {
        auto cmdListStartCmdBufferStream = cmdQImp->getStartingCmdBuffer();
        // check if queue starting stream is the same as immediate,
        // if they are the same - immediate command list buffer has preamble in it including jump from immediate to regular cmdlist - proceed normal
        // if not - regular cmdlist is the starting command buffer - no queue preamble or waiting commands
        if (cmdListStartCmdBufferStream != commandStream) {
            commandStream = cmdListStartCmdBufferStream;
            commandStreamStart = 0u;
        }
    }

    auto csr = cmdQImp->getCsr();
    auto lockCSR = outerLock != nullptr ? std::move(*outerLock) : csr->obtainUniqueOwnership();

    std::unique_lock<std::mutex> lockForIndirect;

    if (appendOperation != NEO::AppendOperations::cmdList) {
        if (NEO::ApiSpecificConfig::isSharedAllocPrefetchEnabled()) {
            auto svmAllocMgr = this->device->getDriverHandle()->getSvmAllocsManager();
            svmAllocMgr->prefetchSVMAllocs(*this->device->getNEODevice(), *csr);
        }

        cmdQ->registerCsrClient();

        if (this->hasIndirectAllocationsAllowed()) {
            cmdQ->handleIndirectAllocationResidency(this->getUnifiedMemoryControls(), lockForIndirect, performMigration);
        }

        if (performMigration) {
            auto deviceImp = static_cast<DeviceImp *>(this->device);
            auto pageFaultManager = deviceImp->getDriverHandle()->getMemoryManager()->getPageFaultManager();
            if (pageFaultManager == nullptr) {
                performMigration = false;
            }
        }

        cmdQ->makeResidentAndMigrate(performMigration, this->commandContainer.getResidencyContainer());

        if (performMigration) {
            this->migrateSharedAllocations();
        }

        if (this->performMemoryPrefetch) {
            auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
            prefetchManager->migrateAllocationsToGpu(this->getPrefetchContext(),
                                                     *this->device->getDriverHandle()->getSvmAllocsManager(),
                                                     *this->device->getNEODevice(),
                                                     *csr);
        }

        static_cast<CommandQueueHw<gfxCoreFamily> *>(this->cmdQImmediate)->patchCommands(*this, 0u, false);
    } else {
        lockForIndirect = std::move(*outerLockForIndirect);
        cmdQImp->makeResidentForResidencyContainer(this->commandContainer.getResidencyContainer());
    }

    for (auto &operation : this->memAdviseOperations) {
        this->executeMemAdvise(operation.hDevice, operation.ptr, operation.size, operation.advice);
    }
    this->memAdviseOperations.clear();

    NEO::CompletionStamp completionStamp;
    if (cmdQ->peekIsCopyOnlyCommandQueue()) {
        completionStamp = flushBcsTask(*commandStream, commandStreamStart, hasStallingCmds, hasRelaxedOrderingDependencies, requireTaskCountUpdate, appendOperation, csr);
    } else {
        completionStamp = (this->*computeFlushMethod)(*commandStream, commandStreamStart, hasStallingCmds, hasRelaxedOrderingDependencies, appendOperation, requireTaskCountUpdate);
    }

    if (completionStamp.taskCount > NEO::CompletionStamp::notReady) {
        if (completionStamp.taskCount == NEO::CompletionStamp::outOfHostMemory) {
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    cmdQImp->clearHeapContainer();

    // save offset from immediate stream - even when not used to dispatch commands, can be used for epilogue
    this->cmdListCurrentStartOffset = this->commandContainer.getCommandStream()->getUsed();
    this->containsAnyKernel = false;
    this->handlePostSubmissionState();

    if (NEO::debugManager.flags.PauseOnEnqueue.get() != -1) {
        this->device->getNEODevice()->debugExecutionCounter++;
    }

    lockCSR.unlock();
    ze_result_t status = ZE_RESULT_SUCCESS;
    cmdQ->setTaskCount(completionStamp.taskCount);

    if (cmdQ == this->cmdQImmediate || cmdQ == this->cmdQImmediateCopyOffload) {
        if (this->isSyncModeQueue) {
            status = hostSynchronize(std::numeric_limits<uint64_t>::max(), true);
        }
    }
    this->kernelWithAssertAppended = false;

    return status;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::hasStallingCmdsForRelaxedOrdering(uint32_t numWaitEvents, bool relaxedOrderingDispatch) const {
    return (!relaxedOrderingDispatch && (numWaitEvents > 0 || this->hasInOrderDependencies()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::skipInOrderNonWalkerSignalingAllowed(ze_event_handle_t signalEvent) const {
    if (!NEO::debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.get()) {
        return false;
    }
    return this->isInOrderNonWalkerSignalingRequired(Event::fromHandle(signalEvent));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernel(
    ze_kernel_handle_t kernelHandle, const ze_group_count_t &threadGroupDimensions,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
    CmdListKernelLaunchParams &launchParams) {

    bool relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);
    bool stallingCmdsForRelaxedOrdering = hasStallingCmdsForRelaxedOrdering(numWaitEvents, relaxedOrderingDispatch);

    checkAvailableSpace(numWaitEvents, relaxedOrderingDispatch, commonImmediateCommandSize, false);
    launchParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernelHandle, threadGroupDimensions,
                                                                        hSignalEvent, numWaitEvents, phWaitEvents,
                                                                        launchParams);

    if (launchParams.skipInOrderNonWalkerSignaling) {
        auto event = Event::fromHandle(hSignalEvent);

        if (isInOrderExecutionEnabled()) {
            // Skip only in base appendLaunchKernel(). Handle remaining operations here.
            handleInOrderNonWalkerSignaling(event, stallingCmdsForRelaxedOrdering, relaxedOrderingDispatch, ret);
        }
        CommandListCoreFamily<gfxCoreFamily>::handleInOrderDependencyCounter(event, true, false);
    }

    return flushImmediate(ret, true, stallingCmdsForRelaxedOrdering, relaxedOrderingDispatch, NEO::AppendOperations::kernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::handleInOrderNonWalkerSignaling(Event *event, bool &hasStallingCmds, bool &relaxedOrderingDispatch, ze_result_t &result) {
    bool nonWalkerSignalingHasRelaxedOrdering = false;

    if (NEO::debugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.get() != 0) {
        auto counterValueBeforeSecondCheck = this->relaxedOrderingCounter;
        nonWalkerSignalingHasRelaxedOrdering = isRelaxedOrderingDispatchAllowed(1, false);
        this->relaxedOrderingCounter = counterValueBeforeSecondCheck; // dont increment twice
    }

    if (nonWalkerSignalingHasRelaxedOrdering) {
        if (event && event->isCounterBased()) {
            event->hostEventSetValue(Event::STATE_INITIAL);
        }
        result = flushImmediate(result, true, hasStallingCmds, relaxedOrderingDispatch, NEO::AppendOperations::kernel, false, nullptr, false, nullptr, nullptr);
        NEO::RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(*this->commandContainer.getCommandStream(), isCopyOnly(false));
        relaxedOrderingDispatch = true;
        hasStallingCmds = hasStallingCmdsForRelaxedOrdering(1, relaxedOrderingDispatch);
    }

    CommandListCoreFamily<gfxCoreFamily>::appendWaitOnSingleEvent(event, nullptr, nonWalkerSignalingHasRelaxedOrdering, false, CommandToPatch::Invalid);
    CommandListCoreFamily<gfxCoreFamily>::appendSignalInOrderDependencyCounter(event, false, false, false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernelIndirect(
    ze_kernel_handle_t kernelHandle, const ze_group_count_t &pDispatchArgumentsBuffer,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {
    relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(kernelHandle, pDispatchArgumentsBuffer,
                                                                                hSignalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, relaxedOrderingDispatch), relaxedOrderingDispatch, NEO::AppendOperations::kernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {
    ze_result_t ret = ZE_RESULT_SUCCESS;

    bool isStallingOperation = true;

    if (isInOrderExecutionEnabled()) {
        if (isSkippingInOrderBarrierAllowed(hSignalEvent, numWaitEvents, phWaitEvents)) {
            if (hSignalEvent) {
                assignInOrderExecInfoToEvent(Event::fromHandle(hSignalEvent));
            }

            return ZE_RESULT_SUCCESS;
        }

        relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);
        isStallingOperation = hasStallingCmdsForRelaxedOrdering(numWaitEvents, relaxedOrderingDispatch);
    }

    checkAvailableSpace(numWaitEvents, false, commonImmediateCommandSize, false);

    ret = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, relaxedOrderingDispatch);

    this->dependenciesPresent = true;
    return flushImmediate(ret, true, isStallingOperation, relaxedOrderingDispatch, NEO::AppendOperations::nonKernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, isCopyOffloadEnabled());

    auto estimatedSize = commonImmediateCommandSize;
    if (isCopyOnly(true)) {
        auto nBlits = size / (NEO::BlitCommandsHelper<GfxFamily>::getMaxBlitWidth(this->device->getNEODevice()->getRootDeviceEnvironment()) *
                              NEO::BlitCommandsHelper<GfxFamily>::getMaxBlitHeight(this->device->getNEODevice()->getRootDeviceEnvironment(), true));
        auto sizePerBlit = sizeof(typename GfxFamily::XY_COPY_BLT) + NEO::BlitCommandsHelper<GfxFamily>::estimatePostBlitCommandSize();
        estimatedSize += nBlits * sizePerBlit;
    }
    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, estimatedSize, false);

    bool hasStallindCmds = hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch);

    ze_result_t ret;
    CpuMemCopyInfo cpuMemCopyInfo(dstptr, const_cast<void *>(srcptr), size);
    this->device->getDriverHandle()->findAllocationDataForRange(const_cast<void *>(srcptr), size, cpuMemCopyInfo.srcAllocData);
    this->device->getDriverHandle()->findAllocationDataForRange(dstptr, size, cpuMemCopyInfo.dstAllocData);
    if (preferCopyThroughLockedPtr(cpuMemCopyInfo, numWaitEvents, phWaitEvents)) {
        ret = performCpuMemcpy(cpuMemCopyInfo, hSignalEvent, numWaitEvents, phWaitEvents);
        if (ret == ZE_RESULT_SUCCESS || ret == ZE_RESULT_ERROR_DEVICE_LOST) {
            return ret;
        }
    }

    NEO::TransferDirection direction;
    auto isSplitNeeded = this->isAppendSplitNeeded(dstptr, srcptr, size, direction);
    if (isSplitNeeded) {
        memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(1, false); // split generates more than 1 event
        memoryCopyParams.forceDisableCopyOnlyInOrderSignaling = true;
        hasStallindCmds = !memoryCopyParams.relaxedOrderingDispatch;

        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, void *, const void *>(this, dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, true, memoryCopyParams.relaxedOrderingDispatch, direction, [&](void *dstptrParam, const void *srcptrParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptrParam, srcptrParam, sizeParam, hSignalEventParam, 0u, nullptr, memoryCopyParams);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent,
                                                                     numWaitEvents, phWaitEvents, memoryCopyParams);
    }

    return flushImmediate(ret, true, hasStallindCmds, memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel, memoryCopyParams.copyOffloadAllowed, hSignalEvent, isSplitNeeded, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyRegion(
    void *dstPtr,
    const ze_copy_region_t *dstRegion,
    uint32_t dstPitch,
    uint32_t dstSlicePitch,
    const void *srcPtr,
    const ze_copy_region_t *srcRegion,
    uint32_t srcPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, isCopyOffloadEnabled());

    auto estimatedSize = commonImmediateCommandSize;
    if (isCopyOnly(true)) {
        auto xBlits = static_cast<size_t>(std::ceil(srcRegion->width / static_cast<double>(BlitterConstants::maxBlitWidth)));
        auto yBlits = static_cast<size_t>(std::ceil(srcRegion->height / static_cast<double>(BlitterConstants::maxBlitHeight)));
        auto zBlits = static_cast<size_t>(srcRegion->depth);
        auto sizePerBlit = sizeof(typename GfxFamily::XY_COPY_BLT) + NEO::BlitCommandsHelper<GfxFamily>::estimatePostBlitCommandSize();
        estimatedSize += xBlits * yBlits * zBlits * sizePerBlit;
    }
    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, estimatedSize, false);

    bool hasStallindCmds = hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch);

    ze_result_t ret;

    NEO::TransferDirection direction;
    auto isSplitNeeded = this->isAppendSplitNeeded(dstPtr, srcPtr, this->getTotalSizeForCopyRegion(dstRegion, dstPitch, dstSlicePitch), direction);
    if (isSplitNeeded) {
        memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(1, false); // split generates more than 1 event
        memoryCopyParams.forceDisableCopyOnlyInOrderSignaling = true;
        hasStallindCmds = !memoryCopyParams.relaxedOrderingDispatch;

        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, uint32_t, uint32_t>(this, dstRegion->originX, srcRegion->originX, dstRegion->width, hSignalEvent, numWaitEvents, phWaitEvents, true, memoryCopyParams.relaxedOrderingDispatch, direction, [&](uint32_t dstOriginXParam, uint32_t srcOriginXParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            ze_copy_region_t dstRegionLocal = {};
            ze_copy_region_t srcRegionLocal = {};
            memcpy(&dstRegionLocal, dstRegion, sizeof(ze_copy_region_t));
            memcpy(&srcRegionLocal, srcRegion, sizeof(ze_copy_region_t));
            dstRegionLocal.originX = dstOriginXParam;
            dstRegionLocal.width = static_cast<uint32_t>(sizeParam);
            srcRegionLocal.originX = srcOriginXParam;
            srcRegionLocal.width = static_cast<uint32_t>(sizeParam);
            return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, &dstRegionLocal, dstPitch, dstSlicePitch,
                                                                                srcPtr, &srcRegionLocal, srcPitch, srcSlicePitch,
                                                                                hSignalEventParam, 0u, nullptr, memoryCopyParams);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch,
                                                                           srcPtr, srcRegion, srcPitch, srcSlicePitch,
                                                                           hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
    }

    return flushImmediate(ret, true, hasStallindCmds, memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel, memoryCopyParams.copyOffloadAllowed, hSignalEvent, isSplitNeeded, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                            size_t patternSize, size_t size,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch,
                          NEO::AppendOperations::kernel, memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hSignalEvent, bool relaxedOrderingDispatch) {
    ze_result_t ret = ZE_RESULT_SUCCESS;

    relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(0, false);
    bool hasStallingCmds = !Event::fromHandle(hSignalEvent)->isCounterBased() || hasStallingCmdsForRelaxedOrdering(0, relaxedOrderingDispatch);

    checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    ret = CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hSignalEvent, relaxedOrderingDispatch);
    return flushImmediate(ret, true, hasStallingCmds, relaxedOrderingDispatch, NEO::AppendOperations::nonKernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendEventReset(ze_event_handle_t hSignalEvent) {
    ze_result_t ret = ZE_RESULT_SUCCESS;

    checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    ret = CommandListCoreFamily<gfxCoreFamily>::appendEventReset(hSignalEvent);
    return flushImmediate(ret, true, true, false, NEO::AppendOperations::nonKernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                                                               NEO::GraphicsAllocation *srcAllocation,
                                                                               size_t size, bool flushHost) {

    checkAvailableSpace(0, false, commonImmediateCommandSize, false);

    ze_result_t ret;

    NEO::TransferDirection direction;
    auto isSplitNeeded = this->isAppendSplitNeeded(dstAllocation->getMemoryPool(), srcAllocation->getMemoryPool(), size, direction);

    bool relaxedOrdering = false;

    if (isSplitNeeded) {
        relaxedOrdering = isRelaxedOrderingDispatchAllowed(1, false); // split generates more than 1 event
        uintptr_t dstAddress = static_cast<uintptr_t>(dstAllocation->getGpuAddress());
        uintptr_t srcAddress = static_cast<uintptr_t>(srcAllocation->getGpuAddress());
        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, uintptr_t, uintptr_t>(this, dstAddress, srcAddress, size, nullptr, 0u, nullptr, false, relaxedOrdering, direction, [&](uintptr_t dstAddressParam, uintptr_t srcAddressParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            this->appendMemoryCopyBlit(dstAddressParam, dstAllocation, 0u,
                                       srcAddressParam, srcAllocation, 0u,
                                       sizeParam, nullptr);
            return CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hSignalEventParam, false);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(dstAllocation, srcAllocation, size, flushHost);
    }
    return flushImmediate(ret, false, false, relaxedOrdering, NEO::AppendOperations::kernel, false, nullptr, isSplitNeeded, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phWaitEvents, CommandToPatchContainer *outWaitCmds,
                                                                              bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) {
    bool allSignaled = true;
    for (auto i = 0u; i < numEvents; i++) {
        allSignaled &= (!this->dcFlushSupport && Event::fromHandle(phWaitEvents[i])->isAlreadyCompleted());
    }
    if (allSignaled) {
        return ZE_RESULT_SUCCESS;
    }

    if (!skipFlush) {
        checkAvailableSpace(numEvents, false, commonImmediateCommandSize, false);
    }

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phWaitEvents, outWaitCmds, relaxedOrderingAllowed, trackDependencies, apiRequest, skipAddingWaitEventsToResidency, false, copyOffloadOperation);
    this->dependenciesPresent = true;

    if (skipFlush) {
        return ret;
    }

    return flushImmediate(ret, true, true, false, NEO::AppendOperations::nonKernel, false, nullptr, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    checkAvailableSpace(numWaitEvents, false, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);

    return flushImmediate(ret, true, true, false, NEO::AppendOperations::nonKernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingDispatch) {
    CmdListMemoryCopyParams memoryCopyParams = {};
    memoryCopyParams.relaxedOrderingDispatch = relaxedOrderingDispatch;
    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopy(
    ze_image_handle_t dst, ze_image_handle_t src,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(dst, src, nullptr, nullptr, hSignalEvent,
                                                                                numWaitEvents, phWaitEvents, memoryCopyParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                                 ze_image_handle_t hSrcImage,
                                                                                 const ze_image_region_t *pDstRegion,
                                                                                 const ze_image_region_t *pSrcRegion,
                                                                                 ze_event_handle_t hSignalEvent,
                                                                                 uint32_t numWaitEvents,
                                                                                 ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    auto estimatedSize = commonImmediateCommandSize;
    if (isCopyOnly(false)) {
        auto imgSize = L0::Image::fromHandle(hSrcImage)->getImageInfo().size;
        auto nBlits = static_cast<size_t>(std::ceil(imgSize / static_cast<double>(BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight)));
        auto sizePerBlit = sizeof(typename GfxFamily::XY_BLOCK_COPY_BLT) + NEO::BlitCommandsHelper<GfxFamily>::estimatePostBlitCommandSize();
        estimatedSize += nBlits * sizePerBlit;
    }
    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, estimatedSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent,
                                                                           numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel,
                          memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyFromMemory(
    ze_image_handle_t hDstImage,
    const void *srcPtr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(hDstImage, srcPtr, pDstRegion, hSignalEvent,
                                                                               numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel,
                          memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyToMemory(
    void *dstPtr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(dstPtr, hSrcImage, pSrcRegion, hSignalEvent,
                                                                             numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel,
                          memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyFromMemoryExt(
    ze_image_handle_t hDstImage,
    const void *srcPtr,
    const ze_image_region_t *pDstRegion,
    uint32_t srcRowPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemoryExt(hDstImage, srcPtr, pDstRegion, srcRowPitch, srcSlicePitch,
                                                                                  hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel,
                          memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyToMemoryExt(
    void *dstPtr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    uint32_t destRowPitch,
    uint32_t destSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents, CmdListMemoryCopyParams &memoryCopyParams) {
    memoryCopyParams.relaxedOrderingDispatch = isRelaxedOrderingDispatchAllowed(numWaitEvents, false);

    checkAvailableSpace(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemoryExt(dstPtr, hSrcImage, pSrcRegion, destRowPitch, destSlicePitch,
                                                                                hSignalEvent, numWaitEvents, phWaitEvents, memoryCopyParams);

    return flushImmediate(ret, true, hasStallingCmdsForRelaxedOrdering(numWaitEvents, memoryCopyParams.relaxedOrderingDispatch), memoryCopyParams.relaxedOrderingDispatch, NEO::AppendOperations::kernel,
                          memoryCopyParams.copyOffloadAllowed, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryRangesBarrier(uint32_t numRanges,
                                                                                     const size_t *pRangeSizes,
                                                                                     const void **pRanges,
                                                                                     ze_event_handle_t hSignalEvent,
                                                                                     uint32_t numWaitEvents,
                                                                                     ze_event_handle_t *phWaitEvents) {
    checkAvailableSpace(numWaitEvents, false, commonImmediateCommandSize, false);

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
    return flushImmediate(ret, true, true, false, NEO::AppendOperations::nonKernel, false, hSignalEvent, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnMemory(void *desc, void *ptr, uint64_t data, ze_event_handle_t signalEventHandle, bool useQwordData) {
    checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnMemory(desc, ptr, data, signalEventHandle, useQwordData);
    return flushImmediate(ret, true, false, false, NEO::AppendOperations::nonKernel, false, signalEventHandle, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWriteToMemory(void *desc, void *ptr, uint64_t data) {
    checkAvailableSpace(0, false, commonImmediateCommandSize, false);
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWriteToMemory(desc, ptr, data);
    return flushImmediate(ret, true, false, false, NEO::AppendOperations::nonKernel, false, nullptr, false, nullptr, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitExternalSemaphores(uint32_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                                                        const ze_external_semaphore_wait_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                                                        uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    checkAvailableSpace(0, false, commonImmediateCommandSize, false);

    auto ret = ZE_RESULT_SUCCESS;
    if (numWaitEvents) {
        ret = this->appendWaitOnEvents(numWaitEvents, phWaitEvents, nullptr, false, false, true, false, true, false);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    auto driverHandleImp = static_cast<DriverHandleImp *>(this->device->getDriverHandle());

    for (uint32_t i = 0; i < numExternalSemaphores; i++) {
        std::lock_guard<std::mutex> lock(driverHandleImp->externalSemaphoreController->semControllerMutex);

        ze_event_handle_t proxyWaitEvent = nullptr;
        ret = driverHandleImp->externalSemaphoreController->allocateProxyEvent(this->device->toHandle(), this->hContext, &proxyWaitEvent);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }

        ret = this->appendWaitOnEvents(1u, &proxyWaitEvent, nullptr, false, false, false, false, false, false);
        if (ret != ZE_RESULT_SUCCESS) {
            auto event = Event::fromHandle(proxyWaitEvent);
            event->destroy();
            return ret;
        }

        driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxyWaitEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphores[i])), params[i].value, ExternalSemaphoreController::SemaphoreOperation::Wait));
    }

    driverHandleImp->externalSemaphoreController->semControllerCv.notify_one();

    bool relaxedOrderingDispatch = false;
    if (hSignalEvent) {
        ret = this->appendSignalEvent(hSignalEvent, relaxedOrderingDispatch);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalExternalSemaphores(size_t numExternalSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores,
                                                                                          const ze_external_semaphore_signal_params_ext_t *params, ze_event_handle_t hSignalEvent,
                                                                                          uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    checkAvailableSpace(0, false, commonImmediateCommandSize, false);

    auto ret = ZE_RESULT_SUCCESS;
    if (numWaitEvents) {
        ret = this->appendWaitOnEvents(numWaitEvents, phWaitEvents, nullptr, false, false, true, false, true, false);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    auto driverHandleImp = static_cast<DriverHandleImp *>(this->device->getDriverHandle());

    for (size_t i = 0; i < numExternalSemaphores; i++) {
        std::lock_guard<std::mutex> lock(driverHandleImp->externalSemaphoreController->semControllerMutex);

        ze_event_handle_t proxySignalEvent = nullptr;
        ret = driverHandleImp->externalSemaphoreController->allocateProxyEvent(this->device->toHandle(), this->hContext, &proxySignalEvent);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }

        ret = this->appendSignalEvent(proxySignalEvent, false);
        if (ret != ZE_RESULT_SUCCESS) {
            auto event = Event::fromHandle(proxySignalEvent);
            event->destroy();
            return ret;
        }

        driverHandleImp->externalSemaphoreController->proxyEvents.push_back(std::make_tuple(Event::fromHandle(proxySignalEvent), static_cast<ExternalSemaphore *>(ExternalSemaphore::fromHandle(hSemaphores[i])), params[i].value, ExternalSemaphoreController::SemaphoreOperation::Signal));
    }

    driverHandleImp->externalSemaphoreController->semControllerCv.notify_one();

    bool relaxedOrderingDispatch = false;
    if (hSignalEvent) {
        ret = this->appendSignalEvent(hSignalEvent, relaxedOrderingDispatch);
    }

    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::hostSynchronize(uint64_t timeout, bool handlePostWaitOperations) {
    ze_result_t status = ZE_RESULT_SUCCESS;

    auto waitQueue = this->cmdQImmediate;

    TaskCountType mainQueueTaskCount = waitQueue->getTaskCount();
    TaskCountType copyOffloadTaskCount = 0;

    NEO::CommandStreamReceiver *mainQueueCsr = getCsr(false);
    NEO::CommandStreamReceiver *copyOffloadCsr = nullptr;

    NEO::InternalAllocationStorage *mainInternalAllocStorage = mainQueueCsr->getInternalAllocationStorage();
    NEO::InternalAllocationStorage *copyOffloadInternalAllocStorage = nullptr;

    bool mainStorageCleanupNeeded = !mainInternalAllocStorage->getTemporaryAllocations().peekIsEmpty();
    bool copyOffloadStorageCleanupNeeded = false;

    const bool dualStreamCopyOffload = isDualStreamCopyOffloadOperation(isCopyOffloadEnabled());

    if (dualStreamCopyOffload) {
        copyOffloadTaskCount = this->cmdQImmediateCopyOffload->getTaskCount();
        copyOffloadCsr = getCsr(true);
        copyOffloadInternalAllocStorage = copyOffloadCsr->getInternalAllocationStorage();
        copyOffloadStorageCleanupNeeded = !copyOffloadInternalAllocStorage->getTemporaryAllocations().peekIsEmpty();

        if (this->latestFlushIsDualCopyOffload) {
            waitQueue = this->cmdQImmediateCopyOffload;
        }
    }

    auto waitTaskCount = waitQueue->getTaskCount();
    auto waitCsr = static_cast<CommandQueueImp *>(waitQueue)->getCsr();

    auto tempAllocsCleanupRequired = handlePostWaitOperations && (mainStorageCleanupNeeded || copyOffloadStorageCleanupNeeded);

    bool inOrderWaitAllowed = (isInOrderExecutionEnabled() && !tempAllocsCleanupRequired && this->latestFlushIsHostVisible && (this->heaplessModeEnabled || !this->latestOperationHasOptimizedCbEvent));

    uint64_t inOrderSyncValue = this->inOrderExecInfo.get() ? inOrderExecInfo->getCounterValue() : 0;

    if (inOrderWaitAllowed) {
        status = synchronizeInOrderExecution(timeout, (waitQueue == this->cmdQImmediateCopyOffload));
    } else {
        const int64_t timeoutInMicroSeconds = timeout / 1000;
        const auto indefinitelyPoll = timeout == std::numeric_limits<uint64_t>::max();
        const auto waitStatus = waitCsr->waitForCompletionWithTimeout(NEO::WaitParams{indefinitelyPoll, !indefinitelyPoll, false, timeoutInMicroSeconds}, waitTaskCount);
        if (waitStatus == NEO::WaitStatus::gpuHang) {
            status = ZE_RESULT_ERROR_DEVICE_LOST;
        } else if (waitStatus == NEO::WaitStatus::notReady) {
            status = ZE_RESULT_NOT_READY;
        }
    }

    if (status != ZE_RESULT_NOT_READY) {
        if (isInOrderExecutionEnabled()) {
            inOrderExecInfo->setLastWaitedCounterValue(inOrderSyncValue);
        }

        if (this->isTbxMode && (status == ZE_RESULT_SUCCESS)) {
            mainQueueCsr->downloadAllocations(true);
            if (dualStreamCopyOffload) {
                copyOffloadCsr->downloadAllocations(true);
            }
        }

        if (handlePostWaitOperations) {
            if (status == ZE_RESULT_SUCCESS) {
                this->cmdQImmediate->unregisterCsrClient();
                if (dualStreamCopyOffload) {
                    this->cmdQImmediateCopyOffload->unregisterCsrClient();
                }

                if (tempAllocsCleanupRequired) {
                    if (mainStorageCleanupNeeded) {
                        mainInternalAllocStorage->cleanAllocationList(mainQueueTaskCount, NEO::AllocationUsage::TEMPORARY_ALLOCATION);
                    }
                    if (copyOffloadStorageCleanupNeeded) {
                        copyOffloadInternalAllocStorage->cleanAllocationList(copyOffloadTaskCount, NEO::AllocationUsage::TEMPORARY_ALLOCATION);
                    }
                }

                if (inOrderExecInfo) {
                    inOrderExecInfo->releaseNotUsedTempTimestampNodes(false);
                }

                this->storeFillPatternResourcesForReuse();
            }

            bool hangDetected = status == ZE_RESULT_ERROR_DEVICE_LOST;
            this->printKernelsPrintfOutput(hangDetected);
            this->checkAssert();
            {
                auto cmdQueueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
                cmdQueueImp->printKernelsPrintfOutput(hangDetected);
                cmdQueueImp->checkAssert();
            }
        }
    }

    return status;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::hostSynchronize(uint64_t timeout) {
    return hostSynchronize(timeout, true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
CommandQueue *CommandListCoreFamilyImmediate<gfxCoreFamily>::getCmdQImmediate(CopyOffloadMode copyOffloadMode) const {
    return (copyOffloadMode == CopyOffloadModes::dualStream) ? this->cmdQImmediateCopyOffload : this->cmdQImmediate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::LinearStream *CommandListCoreFamilyImmediate<gfxCoreFamily>::getOptionalEpilogueCmdStream(NEO::LinearStream *taskCmdStream, NEO::AppendOperations appendOperation) {
    if (appendOperation == NEO::AppendOperations::cmdList && this->dispatchCmdListBatchBufferAsPrimary) {
        auto commandStream = this->commandContainer.getCommandStream();
        // when regular cmd list is present as main command buffer, provide immediate command stream for epilogue
        if (commandStream != taskCmdStream) {
            return commandStream;
        }
    }
    return nullptr;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies,
                                                                          NEO::AppendOperations appendOperation, bool copyOffloadSubmission, ze_event_handle_t hSignalEvent, bool requireTaskCountUpdate,
                                                                          MutexLock *outerLock,
                                                                          std::unique_lock<std::mutex> *outerLockForIndirect) {
    auto signalEvent = Event::fromHandle(hSignalEvent);

    const auto copyOffloadModeForOperation = getCopyOffloadModeForOperation(copyOffloadSubmission);
    auto queue = getCmdQImmediate(copyOffloadModeForOperation);
    this->latestFlushIsDualCopyOffload = (copyOffloadModeForOperation == CopyOffloadModes::dualStream);

    if (NEO::debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.get() == 1) {
        static_cast<CommandQueueImp *>(queue)->getCsr()->ensurePrimaryCsrInitialized(*this->device->getNEODevice());
    }

    if (inputRet == ZE_RESULT_SUCCESS) {
        if (signalEvent && (NEO::debugManager.flags.TrackNumCsrClientsOnSyncPoints.get() != 0)) {
            signalEvent->setLatestUsedCmdQueue(queue);
        }
        inputRet = executeCommandListImmediateWithFlushTask(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies, appendOperation, copyOffloadSubmission, requireTaskCountUpdate,
                                                            outerLock, outerLockForIndirect);
    }

    this->latestFlushIsHostVisible = !this->dcFlushSupport;

    if (signalEvent) {
        signalEvent->setCsr(static_cast<CommandQueueImp *>(queue)->getCsr(), isInOrderExecutionEnabled());
        this->latestFlushIsHostVisible |= signalEvent->isSignalScope(ZE_EVENT_SCOPE_FLAG_HOST) && !this->latestFlushIsDualCopyOffload;
    }

    return inputRet;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::preferCopyThroughLockedPtr(CpuMemCopyInfo &cpuMemCopyInfo, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (NEO::debugManager.flags.ExperimentalForceCopyThroughLock.get() == 1) {
        return true;
    }

    if (!this->copyThroughLockedPtrEnabled) {
        return false;
    }

    if (((cpuMemCopyInfo.srcAllocData != nullptr) && (cpuMemCopyInfo.srcAllocData->isImportedAllocation)) ||
        ((cpuMemCopyInfo.dstAllocData != nullptr) && (cpuMemCopyInfo.dstAllocData->isImportedAllocation))) {
        return false;
    }

    if (cpuMemCopyInfo.srcAllocData == nullptr) {
        auto hostAlloc = this->getDevice()->getDriverHandle()->findHostPointerAllocation(cpuMemCopyInfo.srcPtr, cpuMemCopyInfo.size, this->getDevice()->getRootDeviceIndex());
        cpuMemCopyInfo.srcIsImportedHostPtr = hostAlloc != nullptr;
    }
    if (cpuMemCopyInfo.dstAllocData == nullptr) {
        auto hostAlloc = this->getDevice()->getDriverHandle()->findHostPointerAllocation(cpuMemCopyInfo.dstPtr, cpuMemCopyInfo.size, this->getDevice()->getRootDeviceIndex());
        cpuMemCopyInfo.dstIsImportedHostPtr = hostAlloc != nullptr;
    }

    const TransferType transferType = getTransferType(cpuMemCopyInfo);
    const size_t transferThreshold = getTransferThreshold(transferType);

    bool cpuMemCopyEnabled = false;

    switch (transferType) {
    case TransferType::hostUsmToDeviceUsm:
    case TransferType::deviceUsmToHostUsm: {
        if (this->dependenciesPresent) {
            cpuMemCopyEnabled = false;
            break;
        }
        bool allEventsCompleted = true;
        for (uint32_t i = 0; i < numWaitEvents; i++) {
            if (!Event::fromHandle(phWaitEvents[i])->isAlreadyCompleted()) {
                allEventsCompleted = false;
                break;
            }
        }
        cpuMemCopyEnabled = allEventsCompleted;
        break;
    }
    case TransferType::hostNonUsmToDeviceUsm:
    case TransferType::deviceUsmToHostNonUsm:
        cpuMemCopyEnabled = true;
        break;
    default:
        cpuMemCopyEnabled = false;
        break;
    }

    return cpuMemCopyEnabled && cpuMemCopyInfo.size <= transferThreshold;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::flushInOrderCounterSignal(bool waitOnInOrderCounterRequired) {
    ze_result_t ret = ZE_RESULT_SUCCESS;
    if (waitOnInOrderCounterRequired && !this->isHeaplessModeEnabled() && this->latestOperationHasOptimizedCbEvent) {
        this->appendSignalInOrderDependencyCounter(nullptr, false, true, false);
        this->inOrderExecInfo->addCounterValue(this->getInOrderIncrementValue());
        this->handleInOrderCounterOverflow(false);
        ret = flushImmediate(ret, false, true, false, NEO::AppendOperations::nonKernel, false, nullptr, false, nullptr, nullptr);
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isSuitableUSMHostAlloc(NEO::SvmAllocationData *alloc) {
    return alloc && (alloc->memoryType == InternalMemoryType::hostUnifiedMemory);
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isSuitableUSMDeviceAlloc(NEO::SvmAllocationData *alloc) {
    return alloc && (alloc->memoryType == InternalMemoryType::deviceUnifiedMemory) &&
           alloc->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex()) &&
           alloc->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex())->storageInfo.getNumBanks() == 1;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isSuitableUSMSharedAlloc(NEO::SvmAllocationData *alloc) {
    return alloc && (alloc->memoryType == InternalMemoryType::sharedUnifiedMemory);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::performCpuMemcpy(const CpuMemCopyInfo &cpuMemCopyInfo, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    bool lockingFailed = false;
    auto srcLockPointer = obtainLockedPtrFromDevice(cpuMemCopyInfo.srcAllocData, const_cast<void *>(cpuMemCopyInfo.srcPtr), lockingFailed);
    if (lockingFailed) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto dstLockPointer = obtainLockedPtrFromDevice(cpuMemCopyInfo.dstAllocData, const_cast<void *>(cpuMemCopyInfo.dstPtr), lockingFailed);
    if (lockingFailed) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (isInOrderExecutionEnabled()) {
        this->dependenciesPresent = false; // wait only for waitlist and in-order sync value
    }

    if (numWaitEvents > 0) {
        uint32_t numEventsThreshold = 5;
        if (NEO::debugManager.flags.ExperimentalCopyThroughLockWaitlistSizeThreshold.get() != -1) {
            numEventsThreshold = static_cast<uint32_t>(NEO::debugManager.flags.ExperimentalCopyThroughLockWaitlistSizeThreshold.get());
        }

        bool waitOnHost = !this->dependenciesPresent && (numWaitEvents < numEventsThreshold);

        if (waitOnHost) {
            this->synchronizeEventList(numWaitEvents, phWaitEvents);
        } else {
            this->appendBarrier(nullptr, numWaitEvents, phWaitEvents, false);
        }
    }

    if (this->dependenciesPresent) {
        auto submissionStatus = getCsr(false)->flushTagUpdate();
        if (submissionStatus != NEO::SubmissionStatus::success) {
            return getErrorCodeForSubmissionStatus(submissionStatus);
        }
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (!this->handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const void *cpuMemcpySrcPtr = srcLockPointer ? srcLockPointer : cpuMemCopyInfo.srcPtr;
    void *cpuMemcpyDstPtr = dstLockPointer ? dstLockPointer : cpuMemCopyInfo.dstPtr;

    if (this->dependenciesPresent || isInOrderExecutionEnabled()) {
        auto waitStatus = hostSynchronize(std::numeric_limits<uint64_t>::max(), false);

        if (waitStatus != ZE_RESULT_SUCCESS) {
            return waitStatus;
        }
        this->dependenciesPresent = false;
    }

    if (signalEvent) {
        CommandListImp::addToMappedEventList(signalEvent);
        CommandListImp::storeReferenceTsToMappedEvents(true);
        signalEvent->setGpuStartTimestamp();
    }

    memcpy_s(cpuMemcpyDstPtr, cpuMemCopyInfo.size, cpuMemcpySrcPtr, cpuMemCopyInfo.size);

    if (signalEvent) {
        signalEvent->setGpuEndTimestamp();

        if (signalEvent->isCounterBased()) {
            assignInOrderExecInfoToEvent(signalEvent);
        }

        signalEvent->hostSignal(true);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void *CommandListCoreFamilyImmediate<gfxCoreFamily>::obtainLockedPtrFromDevice(NEO::SvmAllocationData *allocData, void *ptr, bool &lockingFailed) {
    if (!allocData) {
        return nullptr;
    }

    auto alloc = allocData->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex());
    if (alloc->getMemoryPool() != NEO::MemoryPool::localMemory) {
        return nullptr;
    }

    if (!alloc->isLocked()) {
        this->device->getDriverHandle()->getMemoryManager()->lockResource(alloc);
        if (!alloc->isLocked()) {
            lockingFailed = true;
            return nullptr;
        }
    }

    auto gpuAddress = allocData->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex())->getGpuAddress();
    auto offset = ptrDiff(ptr, gpuAddress);
    return ptrOffset(alloc->getLockedPtr(), offset);
}

template <GFXCORE_FAMILY gfxCoreFamily>
TransferType CommandListCoreFamilyImmediate<gfxCoreFamily>::getTransferType(const CpuMemCopyInfo &cpuMemCopyInfo) {
    const bool srcHostUSM = isSuitableUSMHostAlloc(cpuMemCopyInfo.srcAllocData) || cpuMemCopyInfo.srcIsImportedHostPtr;
    const bool srcDeviceUSM = isSuitableUSMDeviceAlloc(cpuMemCopyInfo.srcAllocData);
    const bool srcSharedUSM = isSuitableUSMSharedAlloc(cpuMemCopyInfo.srcAllocData);
    const bool srcHostNonUSM = (cpuMemCopyInfo.srcAllocData == nullptr) && !cpuMemCopyInfo.srcIsImportedHostPtr;

    const bool dstHostUSM = isSuitableUSMHostAlloc(cpuMemCopyInfo.dstAllocData) || cpuMemCopyInfo.dstIsImportedHostPtr;
    const bool dstDeviceUSM = isSuitableUSMDeviceAlloc(cpuMemCopyInfo.dstAllocData);
    const bool dstSharedUSM = isSuitableUSMSharedAlloc(cpuMemCopyInfo.dstAllocData);
    const bool dstHostNonUSM = (cpuMemCopyInfo.dstAllocData == nullptr) && !cpuMemCopyInfo.dstIsImportedHostPtr;

    if (srcHostNonUSM && dstHostUSM) {
        return TransferType::hostNonUsmToHostUsm;
    }
    if (srcHostNonUSM && dstDeviceUSM) {
        return TransferType::hostNonUsmToDeviceUsm;
    }
    if (srcHostNonUSM && dstSharedUSM) {
        return TransferType::hostNonUsmToSharedUsm;
    }
    if (srcHostNonUSM && dstHostNonUSM) {
        return TransferType::hostNonUsmToHostNonUsm;
    }

    if (srcHostUSM && dstHostUSM) {
        return TransferType::hostUsmToHostUsm;
    }
    if (srcHostUSM && dstDeviceUSM) {
        return TransferType::hostUsmToDeviceUsm;
    }
    if (srcHostUSM && dstSharedUSM) {
        return TransferType::hostUsmToSharedUsm;
    }
    if (srcHostUSM && dstHostNonUSM) {
        return TransferType::hostUsmToHostNonUsm;
    }

    if (srcDeviceUSM && dstHostUSM) {
        return TransferType::deviceUsmToHostUsm;
    }
    if (srcDeviceUSM && dstDeviceUSM) {
        return TransferType::deviceUsmToDeviceUsm;
    }
    if (srcDeviceUSM && dstSharedUSM) {
        return TransferType::deviceUsmToSharedUsm;
    }
    if (srcDeviceUSM && dstHostNonUSM) {
        return TransferType::deviceUsmToHostNonUsm;
    }

    if (srcSharedUSM && dstHostUSM) {
        return TransferType::sharedUsmToHostUsm;
    }
    if (srcSharedUSM && dstDeviceUSM) {
        return TransferType::sharedUsmToDeviceUsm;
    }
    if (srcSharedUSM && dstSharedUSM) {
        return TransferType::sharedUsmToSharedUsm;
    }
    if (srcSharedUSM && dstHostNonUSM) {
        return TransferType::sharedUsmToHostNonUsm;
    }

    return TransferType::unknown;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandListCoreFamilyImmediate<gfxCoreFamily>::getTransferThreshold(TransferType transferType) {
    size_t retVal = 0u;

    switch (transferType) {
    case TransferType::hostNonUsmToHostUsm:
        retVal = 1 * MemoryConstants::megaByte;
        break;
    case TransferType::hostNonUsmToDeviceUsm:
        retVal = 4 * MemoryConstants::megaByte;
        if (NEO::debugManager.flags.ExperimentalH2DCpuCopyThreshold.get() != -1) {
            retVal = NEO::debugManager.flags.ExperimentalH2DCpuCopyThreshold.get();
        }
        break;
    case TransferType::hostNonUsmToSharedUsm:
        retVal = 0u;
        break;
    case TransferType::hostNonUsmToHostNonUsm:
        retVal = 1 * MemoryConstants::megaByte;
        break;
    case TransferType::hostUsmToHostUsm:
        retVal = 200 * MemoryConstants::kiloByte;
        break;
    case TransferType::hostUsmToDeviceUsm:
        retVal = 50 * MemoryConstants::kiloByte;
        break;
    case TransferType::hostUsmToSharedUsm:
        retVal = 0u;
        break;
    case TransferType::hostUsmToHostNonUsm:
        retVal = 500 * MemoryConstants::kiloByte;
        break;
    case TransferType::deviceUsmToDeviceUsm:
        retVal = 0u;
        break;
    case TransferType::deviceUsmToSharedUsm:
        retVal = 0u;
        break;
    case TransferType::deviceUsmToHostUsm:
        retVal = 128u;
        break;
    case TransferType::deviceUsmToHostNonUsm:
        retVal = 1 * MemoryConstants::kiloByte;
        if (NEO::debugManager.flags.ExperimentalD2HCpuCopyThreshold.get() != -1) {
            retVal = NEO::debugManager.flags.ExperimentalD2HCpuCopyThreshold.get();
        }
        break;
    case TransferType::sharedUsmToHostUsm:
    case TransferType::sharedUsmToDeviceUsm:
    case TransferType::sharedUsmToSharedUsm:
    case TransferType::sharedUsmToHostNonUsm:
        retVal = 0u;
        break;
    default:
        retVal = 0u;
        break;
    }

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isBarrierRequired() {
    return *getCsr(false)->getBarrierCountTagAddress() < getCsr(false)->peekBarrierCount();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::printKernelsPrintfOutput(bool hangDetected) {
    for (auto &kernelWeakPtr : this->printfKernelContainer) {
        std::lock_guard<std::mutex> lock(static_cast<DeviceImp *>(this->device)->printfKernelMutex);
        if (!kernelWeakPtr.expired()) {
            kernelWeakPtr.lock()->printPrintfOutput(hangDetected);
        }
    }
    this->printfKernelContainer.clear();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::checkAssert() {
    if (this->hasKernelWithAssert()) {
        UNRECOVERABLE_IF(this->device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get() == nullptr);
        this->device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isRelaxedOrderingDispatchAllowed(uint32_t numWaitEvents, bool copyOffload) {
    const auto copyOffloadModeForOperation = getCopyOffloadModeForOperation(copyOffload);

    auto csr = getCsr(copyOffload);
    if (!csr->directSubmissionRelaxedOrderingEnabled()) {
        return false;
    }

    auto numEvents = numWaitEvents + (this->hasInOrderDependencies() ? 1 : 0);

    if (NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.get() != 0) {
        uint32_t relaxedOrderingCounterThreshold = csr->getDirectSubmissionRelaxedOrderingQueueDepth();

        auto queueTaskCount = getCmdQImmediate(copyOffloadModeForOperation)->getTaskCount();
        auto csrTaskCount = csr->peekTaskCount();

        bool skipTaskCountCheck = (csrTaskCount - queueTaskCount == 1) && csr->isLatestFlushIsTaskCountUpdateOnly();

        if (NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicTreshold.get() != -1) {
            relaxedOrderingCounterThreshold = static_cast<uint32_t>(NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicTreshold.get());
        }

        if (queueTaskCount == csrTaskCount || skipTaskCountCheck) {
            relaxedOrderingCounter++;
        } else {
            // Submission from another queue. Reset counter and keep relaxed ordering allowed
            relaxedOrderingCounter = 0;
            this->keepRelaxedOrderingEnabled = true;
        }

        if (relaxedOrderingCounter > static_cast<uint64_t>(relaxedOrderingCounterThreshold)) {
            this->keepRelaxedOrderingEnabled = false;
            return false;
        }

        return (keepRelaxedOrderingEnabled && (numEvents > 0));
    }

    return NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*csr, numEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::synchronizeInOrderExecution(uint64_t timeout, bool copyOffloadSync) const {
    std::chrono::high_resolution_clock::time_point waitStartTime, lastHangCheckTime, now;
    uint64_t timeDiff = 0;

    ze_result_t status = ZE_RESULT_NOT_READY;

    auto waitValue = inOrderExecInfo->getCounterValue();

    lastHangCheckTime = std::chrono::high_resolution_clock::now();
    waitStartTime = lastHangCheckTime;

    auto csr = getCsr(copyOffloadSync);

    do {
        if (inOrderExecInfo->getHostCounterAllocation()) {
            csr->downloadAllocation(*inOrderExecInfo->getHostCounterAllocation());
        } else {
            UNRECOVERABLE_IF(!inOrderExecInfo->getDeviceCounterAllocation());
            csr->downloadAllocation(*inOrderExecInfo->getDeviceCounterAllocation());
        }

        bool signaled = true;

        if (csr->getType() != NEO::CommandStreamReceiverType::aub) {
            const uint64_t *hostAddress = ptrOffset(inOrderExecInfo->getBaseHostAddress(), inOrderExecInfo->getAllocationOffset());

            for (uint32_t i = 0; i < inOrderExecInfo->getNumHostPartitionsToWait(); i++) {
                if (!NEO::WaitUtils::waitFunctionWithPredicate<const uint64_t>(hostAddress, waitValue, std::greater_equal<uint64_t>(), timeDiff / 1000)) {
                    signaled = false;
                    break;
                }

                hostAddress = ptrOffset(hostAddress, this->device->getL0GfxCoreHelper().getImmediateWritePostSyncOffset());
            }
        }

        if (signaled) {
            csr->pollForAubCompletion();
            status = ZE_RESULT_SUCCESS;
            break;
        }

        if (csr->checkGpuHangDetected(std::chrono::high_resolution_clock::now(), lastHangCheckTime)) {
            status = ZE_RESULT_ERROR_DEVICE_LOST;
            break;
        }

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        } else if (timeout == 0) {
            break;
        }

        now = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(now - waitStartTime).count();
    } while (timeDiff < timeout);

    return status;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::setupFlushMethod(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    if (L0GfxCoreHelper::useImmediateComputeFlushTask(rootDeviceEnvironment)) {

        if (this->isHeaplessStateInitEnabled()) {
            this->computeFlushMethod = &CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediateRegularTaskStateless;
        } else {

            this->computeFlushMethod = &CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediateRegularTask;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::allocateOrReuseKernelPrivateMemoryIfNeeded(Kernel *kernel, uint32_t sizePerHwThread) {
    L0::KernelImp *kernelImp = static_cast<KernelImp *>(kernel);
    if (sizePerHwThread != 0U && kernelImp->getParentModule().shouldAllocatePrivateMemoryPerDispatch()) {
        auto ownership = getCsr(false)->obtainUniqueOwnership();
        this->allocateOrReuseKernelPrivateMemory(kernel, sizePerHwThread, getCsr(false)->getOwnedPrivateAllocations());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists,
                                                                              ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    constexpr bool copyOffloadOperation = false;
    constexpr bool relaxedOrderingDispatch = false;
    constexpr bool requireTaskCountUpdate = true;
    constexpr bool hasStallingCmds = true;

    bool copyEngineExecution = isCopyOnly(copyOffloadOperation);

    auto ret = ZE_RESULT_SUCCESS;
    checkAvailableSpace(numWaitEvents,
                        relaxedOrderingDispatch,
                        commonImmediateCommandSize,
                        this->dispatchCmdListBatchBufferAsPrimary);

    ret = CommandListCoreFamily<gfxCoreFamily>::addEventsToCmdList(numWaitEvents, phWaitEvents,
                                                                   nullptr,
                                                                   relaxedOrderingDispatch,
                                                                   false,
                                                                   true,
                                                                   false,
                                                                   copyOffloadOperation);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    Event *signalEvent = nullptr;
    bool dcFlush = false;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
        dcFlush = this->getDcFlushRequired(signalEvent->isSignalScope());
    }

    if (!this->handleCounterBasedEventOperations(signalEvent, false)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    CommandListCoreFamily<gfxCoreFamily>::appendEventForProfiling(signalEvent,
                                                                  nullptr,
                                                                  true,
                                                                  false,
                                                                  false,
                                                                  copyEngineExecution);

    auto queueImp = static_cast<CommandQueueImp *>(this->cmdQImmediate);
    auto mainAppendLock = queueImp->getCsr()->obtainUniqueOwnership();
    std::unique_lock<std::mutex> mainLockForIndirect;

    if (this->dispatchCmdListBatchBufferAsPrimary) {
        // check if wait event preamble or implicit synchronization is present and force bb start jump in queue, even when no preamble is required there
        if (this->commandContainer.getCommandStream()->getUsed() != this->cmdListCurrentStartOffset) {
            queueImp->triggerBbStartJump();
        }
    }
    ret = this->cmdQImmediate->executeCommandLists(numCommandLists, phCommandLists,
                                                   nullptr,
                                                   true,
                                                   this->commandContainer.getCommandStream(),
                                                   &mainLockForIndirect);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    CommandListCoreFamily<gfxCoreFamily>::appendSignalEventPostWalker(signalEvent,
                                                                      nullptr,
                                                                      nullptr,
                                                                      false,
                                                                      false,
                                                                      copyEngineExecution);
    if (isInOrderExecutionEnabled()) {
        CommandListCoreFamily<gfxCoreFamily>::dispatchInOrderPostOperationBarrier(signalEvent,
                                                                                  dcFlush,
                                                                                  copyEngineExecution);
        CommandListCoreFamily<gfxCoreFamily>::appendSignalInOrderDependencyCounter(signalEvent,
                                                                                   copyOffloadOperation,
                                                                                   false,
                                                                                   false);
    }

    CommandListCoreFamily<gfxCoreFamily>::handleInOrderDependencyCounter(signalEvent,
                                                                         false,
                                                                         copyOffloadOperation);

    return flushImmediate(ret,
                          true,
                          hasStallingCmds,
                          relaxedOrderingDispatch,
                          NEO::AppendOperations::cmdList,
                          copyOffloadOperation,
                          hSignalEvent,
                          requireTaskCountUpdate,
                          &mainAppendLock,
                          &mainLockForIndirect);
}

} // namespace L0
