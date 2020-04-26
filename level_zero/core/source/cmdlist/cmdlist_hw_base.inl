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

#include "pipe_control_args.h"

#include <algorithm>

namespace L0 {
struct DeviceImp;

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelWithParams(ze_kernel_handle_t hKernel,
                                                                               const ze_group_count_t *pThreadGroupDimensions,
                                                                               ze_event_handle_t hEvent, uint32_t numWaitEvents,
                                                                               ze_event_handle_t *phWaitEvents, bool isIndirect, bool isPredicate) {
    const auto kernel = Kernel::fromHandle(hKernel);
    UNRECOVERABLE_IF(kernel == nullptr);
    const auto functionImmutableData = kernel->getImmutableData();
    commandListPerThreadScratchSize = std::max<std::uint32_t>(commandListPerThreadScratchSize, kernel->getImmutableData()->getDescriptor().kernelAttributes.perThreadScratchSize[0]);

    auto kernelPreemptionMode = obtainFunctionPreemptionMode(kernel);
    commandListPreemptionMode = std::min(commandListPreemptionMode, kernelPreemptionMode);

    if (!isIndirect) {
        kernel->setGroupCount(pThreadGroupDimensions->groupCountX,
                              pThreadGroupDimensions->groupCountY,
                              pThreadGroupDimensions->groupCountZ);
    }

    if (isIndirect && pThreadGroupDimensions) {
        prepareIndirectParams(pThreadGroupDimensions);
    }

    if (kernel->hasIndirectAllocationsAllowed()) {
        UnifiedMemoryControls unifiedMemoryControls = kernel->getUnifiedMemoryControls();
        auto svmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
        auto &residencyContainer = commandContainer.getResidencyContainer();

        svmAllocsManager->addInternalAllocationsToResidencyContainer(residencyContainer, unifiedMemoryControls.generateMask());
    }

    NEO::EncodeDispatchKernel<GfxFamily>::encode(commandContainer,
                                                 reinterpret_cast<const void *>(pThreadGroupDimensions), isIndirect, isPredicate, kernel,
                                                 0, device->getNEODevice(), commandListPreemptionMode);

    if (hEvent) {
        appendSignalEventPostWalker(hEvent);
    }

    commandContainer.addToResidencyContainer(functionImmutableData->getIsaGraphicsAllocation());
    auto &residencyContainer = kernel->getResidencyContainer();
    for (auto resource : residencyContainer) {
        commandContainer.addToResidencyContainer(resource);
    }

    if (functionImmutableData->getDescriptor().kernelAttributes.flags.usesPrintf) {
        storePrintfFunction(kernel);
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
        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::GLOBAL_START);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, REG_GLOBAL_TIMESTAMP_LDW, timeStampAddress);

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::CONTEXT_START);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timeStampAddress);
    } else {

        timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::GLOBAL_END);
        NEO::PipeControlArgs args;
        args.dcFlushEnable = (event->signalScope == ZE_EVENT_SCOPE_FLAG_NONE) ? false : true;

        if (isCopyOnlyCmdList) {
            NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), timeStampAddress, 0llu, true, true);
        } else {
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
                *(commandContainer.getCommandStream()), POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
                timeStampAddress,
                0llu,
                device->getHwInfo(),
                args);

            timeStampAddress = event->getGpuAddress() + event->getOffsetOfEventTimestampRegister(Event::CONTEXT_END);
            NEO::EncodeStoreMMIO<GfxFamily>::encode(commandContainer, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, timeStampAddress);

            if (args.dcFlushEnable) {
                NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), args);
            }
        }
    }
}
} // namespace L0
