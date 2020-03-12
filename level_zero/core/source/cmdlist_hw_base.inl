/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/unified_memory/unified_memory.h"

#include <algorithm>

namespace L0 {
struct DeviceImp;

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunctionWithParams(ze_kernel_handle_t hFunction,
                                                                                 const ze_group_count_t *pThreadGroupDimensions,
                                                                                 ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                                                 ze_event_handle_t *phWaitEvents, bool isIndirect, bool isPredicate) {
    const auto function = Kernel::fromHandle(hFunction);
    UNRECOVERABLE_IF(function == nullptr);
    const auto functionImmutableData = function->getImmutableData();
    commandListPerThreadScratchSize = std::max(commandListPerThreadScratchSize, function->getPerThreadScratchSize());

    auto functionPreemptionMode = obtainFunctionPreemptionMode(function);
    commandListPreemptionMode = std::min(commandListPreemptionMode, functionPreemptionMode);

    if (!isIndirect) {
        function->setGroupCount(pThreadGroupDimensions->groupCountX,
                                pThreadGroupDimensions->groupCountY,
                                pThreadGroupDimensions->groupCountZ);
    }

    if (isIndirect && pThreadGroupDimensions) {
        prepareIndirectParams(pThreadGroupDimensions);
    }

    if (function->hasIndirectAllocationsAllowed()) {
        UnifiedMemoryControls unifiedMemoryControls = function->getUnifiedMemoryControls();
        auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
        auto &residencyContainer = commandContainer.getResidencyContainer();

        svmAllocsManager->addInternalAllocationsToResidencyContainer(residencyContainer, unifiedMemoryControls.generateMask());
    }

    NEO::EncodeDispatchKernel<GfxFamily>::encode(commandContainer,
                                                 reinterpret_cast<const void *>(pThreadGroupDimensions), isIndirect, isPredicate, function,
                                                 0, device->getNEODevice(), commandListPreemptionMode);

    if (hEvent) {
        appendSignalEventPostWalker(hEvent);
    }

    commandContainer.addToResidencyContainer(functionImmutableData->getIsaGraphicsAllocation());
    auto &residencyContainer = function->getResidencyContainer();
    for (auto resource : residencyContainer) {
        commandContainer.addToResidencyContainer(resource);
    }

    if (functionImmutableData->getDescriptor().kernelAttributes.flags.usesPrintf) {
        storePrintfFunction(function);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfiling(ze_event_handle_t hEvent, bool beforeWalker) {
    if (!hEvent) {
        return;
    }

    auto event = Event::fromHandle(hEvent);

    if (!event->isTimestampEvent) {
        return;
    }

    uint64_t timeStampAddress = 0;

    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    commandContainer.addToResidencyContainer(&event->getAllocation());
    if (beforeWalker) {
        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::GLOBAL_START_LOW);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, REG_GLOBAL_TIMESTAMP_LDW, timeStampAddress);

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::GLOBAL_START_HIGH);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, REG_GLOBAL_TIMESTAMP_UN, timeStampAddress);

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::CONTEXT_START);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timeStampAddress);
    } else {

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::CONTEXT_END);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timeStampAddress);

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::GLOBAL_END);
        bool dcFlushEnable = (event->signalScope == ZE_EVENT_SCOPE_FLAG_NONE) ? false : true;

        NEO::MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
            *(commandContainer.getCommandStream()), POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
            timeStampAddress,
            0llu,
            dcFlushEnable,
            device->getHwInfo());
    }
}
} // namespace L0
