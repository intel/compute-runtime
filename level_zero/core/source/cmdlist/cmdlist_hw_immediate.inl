/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::checkAvailableSpace() {
    if (this->commandContainer.getCommandStream()->getAvailableSpace() < maxImmediateCommandSize) {
        this->commandContainer.allocateNextCommandBuffer();
        cmdListBBEndOffset = 0;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::executeCommandListImmediateWithFlushTask(bool performMigration) {

    NEO::DispatchFlags dispatchFlags(
        {},                                                          //csrDependencies
        nullptr,                                                     //barrierTimestampPacketNodes
        {},                                                          //pipelineSelectArgs
        nullptr,                                                     //flushStampReference
        NEO::QueueThrottle::MEDIUM,                                  //throttle
        this->getCommandListPreemptionMode(),                        //preemptionMode
        this->commandContainer.lastSentNumGrfRequired,               //numGrfRequired
        NEO::L3CachingSettings::l3CacheOn,                           //l3CacheSettings
        this->getThreadArbitrationPolicy(),                          //threadArbitrationPolicy
        NEO::AdditionalKernelExecInfo::NotApplicable,                //additionalKernelExecInfo
        NEO::KernelExecutionType::NotApplicable,                     //kernelExecutionType
        NEO::MemoryCompressionState::NotApplicable,                  //memoryCompressionState
        NEO::QueueSliceCount::defaultSliceCount,                     //sliceCount
        this->isSyncModeQueue,                                       //blocking
        this->isSyncModeQueue,                                       //dcFlush
        this->getCommandListSLMEnable(),                             //useSLM
        this->isSyncModeQueue,                                       //guardCommandBufferWithPipeControl
        false,                                                       //GSBA32BitRequired
        false,                                                       //requiresCoherency
        false,                                                       //lowPriority
        true,                                                        //implicitFlush
        this->csr->isNTo1SubmissionModelEnabled(),                   //outOfOrderExecutionAllowed
        false,                                                       //epilogueRequired
        false,                                                       //usePerDssBackedBuffer
        false,                                                       //useSingleSubdevice
        false,                                                       //useGlobalAtomics
        this->device->getNEODevice()->getNumGenericSubDevices() > 1, //areMultipleSubDevicesInContext
        false                                                        //memoryMigrationRequired
    );

    this->commandContainer.removeDuplicatesFromResidencyContainer();

    auto commandStream = this->commandContainer.getCommandStream();
    size_t commandStreamStart = cmdListBBEndOffset;

    auto lockCSR = this->csr->obtainUniqueOwnership();

    this->csr->setRequiredScratchSizes(this->getCommandListPerThreadScratchSize(), this->getCommandListPerThreadScratchSize());

    if (performMigration) {
        auto deviceImp = static_cast<DeviceImp *>(this->device);
        auto pageFaultManager = deviceImp->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        if (pageFaultManager == nullptr) {
            performMigration = false;
        }
    }

    this->makeResidentAndMigrate(performMigration);

    if (performMigration) {
        this->migrateSharedAllocations();
    }

    auto completionStamp = this->csr->flushTask(
        *commandStream,
        commandStreamStart,
        *(this->commandContainer.getIndirectHeap(NEO::IndirectHeap::DYNAMIC_STATE)),
        *(this->commandContainer.getIndirectHeap(NEO::IndirectHeap::INDIRECT_OBJECT)),
        *(this->commandContainer.getIndirectHeap(NEO::IndirectHeap::SURFACE_STATE)),
        this->csr->peekTaskLevel(),
        dispatchFlags,
        *(this->device->getNEODevice()));

    if (this->isSyncModeQueue) {
        auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
        this->csr->waitForCompletionWithTimeout(false, timeoutMicroseconds, completionStamp.taskCount);
        this->removeHostPtrAllocations();
    }

    cmdListBBEndOffset = commandStream->getUsed();

    this->commandContainer.getResidencyContainer().clear();

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernel(
    ze_kernel_handle_t hKernel, const ze_group_count_t *pThreadGroupDimensions,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(hKernel, pThreadGroupDimensions,
                                                                        hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernelIndirect(
    ze_kernel_handle_t hKernel, const ze_group_count_t *pDispatchArgumentsBuffer,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(hKernel, pDispatchArgumentsBuffer,
                                                                                hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendBarrier(
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    ze_result_t ret = ZE_RESULT_SUCCESS;
    bool isTimestampEvent = false;
    for (uint32_t i = 0; i < numWaitEvents; i++) {
        auto event = Event::fromHandle(phWaitEvents[i]);
        isTimestampEvent |= (event->isEventTimestampFlagSet()) ? true : false;
    }
    if (hSignalEvent) {
        auto signalEvent = Event::fromHandle(hSignalEvent);
        isTimestampEvent |= signalEvent->isEventTimestampFlagSet();
    }

    if (isTimestampEvent) {
        if (this->isFlushTaskSubmissionEnabled) {
            checkAvailableSpace();
        }
        ret = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
        if (ret == ZE_RESULT_SUCCESS) {
            if (this->isFlushTaskSubmissionEnabled) {
                executeCommandListImmediateWithFlushTask(true);
            } else {
                executeCommandListImmediate(true);
            }
        }
    } else {
        ret = CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnEvents(numWaitEvents, phWaitEvents);
        if (!hSignalEvent) {
            NEO::PipeControlArgs args;
            this->csr->flushNonKernelTask(nullptr, 0, 0, args, false, false, false);
            if (this->isSyncModeQueue) {
                this->csr->flushTagUpdate();
                auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
                this->csr->waitForCompletionWithTimeout(false, timeoutMicroseconds, this->csr->peekTaskCount());
            }
        } else {
            ret = CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(hSignalEvent);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent,
                                                                      numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
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
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch,
                                                                            srcPtr, srcRegion, srcPitch, srcSlicePitch,
                                                                            hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                            size_t patternSize, size_t size,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hSignalEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t ret = ZE_RESULT_SUCCESS;
    auto event = Event::fromHandle(hSignalEvent);
    bool isTimestampEvent = event->isEventTimestampFlagSet();

    if (isTimestampEvent) {
        if (this->isFlushTaskSubmissionEnabled) {
            checkAvailableSpace();
        }
        ret = CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hSignalEvent);
        if (ret == ZE_RESULT_SUCCESS) {
            if (this->isFlushTaskSubmissionEnabled) {
                executeCommandListImmediateWithFlushTask(true);
            } else {
                executeCommandListImmediate(true);
            }
        }
    } else {
        NEO::PipeControlArgs args;
        if (NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed()) {
            args.dcFlushEnable = (!event->signalScope) ? false : true;
        }
        this->csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_SIGNALED, args, false, false, false);
        if (this->isSyncModeQueue) {
            this->csr->flushTagUpdate();
            auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
            this->csr->waitForCompletionWithTimeout(false, timeoutMicroseconds, this->csr->peekTaskCount());
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendEventReset(ze_event_handle_t hSignalEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t ret = ZE_RESULT_SUCCESS;
    auto event = Event::fromHandle(hSignalEvent);
    bool isTimestampEvent = event->isEventTimestampFlagSet();

    if (isTimestampEvent) {
        if (this->isFlushTaskSubmissionEnabled) {
            checkAvailableSpace();
        }
        ret = CommandListCoreFamily<gfxCoreFamily>::appendEventReset(hSignalEvent);
        if (ret == ZE_RESULT_SUCCESS) {
            if (this->isFlushTaskSubmissionEnabled) {
                executeCommandListImmediateWithFlushTask(true);
            } else {
                executeCommandListImmediate(true);
            }
        }
    } else {
        NEO::PipeControlArgs args;
        if (NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed()) {
            args.dcFlushEnable = (!event->signalScope) ? false : true;
        }
        this->csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_CLEARED, args, false, false, false);
        if (this->isSyncModeQueue) {
            this->csr->flushTagUpdate();
            auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
            this->csr->waitForCompletionWithTimeout(false, timeoutMicroseconds, this->csr->peekTaskCount());
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                                                               NEO::GraphicsAllocation *srcAllocation,
                                                                               size_t size, bool flushHost) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(dstAllocation, srcAllocation, size, flushHost);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(false);
        } else {
            executeCommandListImmediate(false);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phWaitEvents) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t ret = ZE_RESULT_SUCCESS;
    bool isTimestampEvent = false;

    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phWaitEvents[i]);
        isTimestampEvent |= (event->isEventTimestampFlagSet()) ? true : false;
    }

    if (isTimestampEvent) {
        if (this->isFlushTaskSubmissionEnabled) {
            checkAvailableSpace();
        }
        ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phWaitEvents);
        if (ret == ZE_RESULT_SUCCESS) {
            if (this->isFlushTaskSubmissionEnabled) {
                executeCommandListImmediateWithFlushTask(true);
            } else {
                executeCommandListImmediate(true);
            }
        }
    } else {
        bool dcFlushRequired = false;
        if (NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed()) {
            for (uint32_t i = 0; i < numEvents; i++) {
                auto event = Event::fromHandle(phWaitEvents[i]);
                dcFlushRequired |= (!event->waitScope) ? false : true;
            }
        }

        NEO::PipeControlArgs args;
        args.dcFlushEnable = dcFlushRequired;
        for (uint32_t i = 0; i < numEvents; i++) {
            auto event = Event::fromHandle(phWaitEvents[i]);
            bool isStartOfDispatch = (i == 0);
            bool isEndOfDispatch = (i == numEvents - 1);
            this->csr->flushNonKernelTask(&event->getAllocation(this->device), event->getGpuAddress(this->device), Event::STATE_CLEARED, args, true, isStartOfDispatch, isEndOfDispatch);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopy(
    ze_image_handle_t dst, ze_image_handle_t src,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(dst, src, nullptr, nullptr, hSignalEvent,
                                                                                numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                                 ze_image_handle_t hSrcImage,
                                                                                 const ze_image_region_t *pDstRegion,
                                                                                 const ze_image_region_t *pSrcRegion,
                                                                                 ze_event_handle_t hSignalEvent,
                                                                                 uint32_t numWaitEvents,
                                                                                 ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent,
                                                                           numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyFromMemory(
    ze_image_handle_t hDstImage,
    const void *srcPtr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(hDstImage, srcPtr, pDstRegion, hSignalEvent,
                                                                               numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyToMemory(
    void *dstPtr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(dstPtr, hSrcImage, pSrcRegion, hSignalEvent,
                                                                             numWaitEvents, phWaitEvents);
    if (ret == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            executeCommandListImmediateWithFlushTask(true);
        } else {
            executeCommandListImmediate(true);
        }
    }
    return ret;
}

} // namespace L0
