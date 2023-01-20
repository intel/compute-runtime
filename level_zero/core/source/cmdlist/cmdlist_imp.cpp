/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/tools/source/metrics/metric.h"

#include "igfxfmid.h"

#include <algorithm>

namespace L0 {

CommandList::CommandList(uint32_t numIddsPerBlock) : commandContainer(numIddsPerBlock) {
}

CommandListAllocatorFn commandListFactory[IGFX_MAX_PRODUCT] = {};
CommandListAllocatorFn commandListFactoryImmediate[IGFX_MAX_PRODUCT] = {};

ze_result_t CommandListImp::destroy() {
    if (this->isBcsSplitNeeded) {
        static_cast<DeviceImp *>(this->device)->bcsSplit.releaseResources();
    }

    if (this->cmdListType == CommandListType::TYPE_IMMEDIATE && this->isFlushTaskSubmissionEnabled && !this->isSyncModeQueue) {
        auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
        this->csr->waitForCompletionWithTimeout(NEO::WaitParams{false, false, timeoutMicroseconds}, this->csr->peekTaskCount());
    }
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandListImp::appendMetricMemoryBarrier() {

    return device->getMetricDeviceContext().appendMetricMemoryBarrier(*this);
}

ze_result_t CommandListImp::appendMetricStreamerMarker(zet_metric_streamer_handle_t hMetricStreamer,
                                                       uint32_t value) {
    return MetricStreamer::fromHandle(hMetricStreamer)->appendStreamerMarker(*this, value);
}

ze_result_t CommandListImp::appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) {
    if (cmdListType == CommandListType::TYPE_IMMEDIATE && isFlushTaskSubmissionEnabled) {
        this->device->activateMetricGroups();
    }

    return MetricQuery::fromHandle(hMetricQuery)->appendBegin(*this);
}

ze_result_t CommandListImp::appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return MetricQuery::fromHandle(hMetricQuery)->appendEnd(*this, hSignalEvent, numWaitEvents, phWaitEvents);
}

CommandList *CommandList::create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                                 ze_command_list_flags_t flags, ze_result_t &returnValue) {
    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandListFactory[productFamily];
    }

    CommandListImp *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        commandList = static_cast<CommandListImp *>((*allocator)(CommandList::defaultNumIddsPerBlock));
        returnValue = commandList->initialize(device, engineGroupType, flags);
        if (returnValue != ZE_RESULT_SUCCESS) {
            commandList->destroy();
            commandList = nullptr;
        }
    }

    return commandList;
}

CommandList *CommandList::createImmediate(uint32_t productFamily, Device *device,
                                          const ze_command_queue_desc_t *desc,
                                          bool internalUsage, NEO::EngineGroupType engineGroupType,
                                          ze_result_t &returnValue) {

    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandListFactoryImmediate[productFamily];
    }

    CommandListImp *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        NEO::CommandStreamReceiver *csr = nullptr;
        auto deviceImp = static_cast<DeviceImp *>(device);
        const auto &hwInfo = device->getHwInfo();
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        if (internalUsage) {
            if (NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType) && deviceImp->getActiveDevice()->getInternalCopyEngine()) {
                csr = deviceImp->getActiveDevice()->getInternalCopyEngine()->commandStreamReceiver;
            } else {
                auto internalEngine = deviceImp->getActiveDevice()->getInternalEngine();
                csr = internalEngine.commandStreamReceiver;
                auto internalEngineType = internalEngine.getEngineType();
                auto internalEngineUsage = internalEngine.getEngineUsage();
                engineGroupType = gfxCoreHelper.getEngineGroupType(internalEngineType, internalEngineUsage, hwInfo);
            }
        } else {
            returnValue = device->getCsrForOrdinalAndIndex(&csr, desc->ordinal, desc->index);
            if (returnValue != ZE_RESULT_SUCCESS) {
                return commandList;
            }
        }

        UNRECOVERABLE_IF(nullptr == csr);

        commandList = static_cast<CommandListImp *>((*allocator)(CommandList::commandListimmediateIddsPerBlock));
        commandList->csr = csr;
        commandList->internalUsage = internalUsage;
        commandList->cmdListType = CommandListType::TYPE_IMMEDIATE;
        commandList->isSyncModeQueue = (desc->mode == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS);
        if (!internalUsage) {
            commandList->isFlushTaskSubmissionEnabled = gfxCoreHelper.isPlatformFlushTaskEnabled(hwInfo);
            if (NEO::DebugManager.flags.EnableFlushTaskSubmission.get() != -1) {
                commandList->isFlushTaskSubmissionEnabled = !!NEO::DebugManager.flags.EnableFlushTaskSubmission.get();
            }
            PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Flush Task for Immediate command list : %s\n", commandList->isFlushTaskSubmissionEnabled ? "Enabled" : "Disabled");

            auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
            commandList->immediateCmdListHeapSharing = L0GfxCoreHelper::enableImmediateCmdListHeapSharing(rootDeviceEnvironment, commandList->isFlushTaskSubmissionEnabled);
        }
        returnValue = commandList->initialize(device, engineGroupType, desc->flags);
        if (returnValue != ZE_RESULT_SUCCESS) {
            commandList->destroy();
            commandList = nullptr;
            return commandList;
        }

        auto commandQueue = CommandQueue::create(productFamily, device, csr, desc, commandList->isCopyOnly(), internalUsage, returnValue);
        if (!commandQueue) {
            commandList->destroy();
            commandList = nullptr;
            return commandList;
        }

        commandList->cmdQImmediate = commandQueue;
        commandList->isTbxMode = csr->isTbxMode();
        commandList->commandListPreemptionMode = device->getDevicePreemptionMode();

        commandList->isBcsSplitNeeded = deviceImp->bcsSplit.setupDevice(productFamily, internalUsage, desc, csr);
        commandList->commandContainer.setImmediateCmdListCsr(csr);
        commandList->commandContainer.fillReusableAllocationLists();

        if (commandList->isWaitForEventsFromHostEnabled()) {
            commandList->numThreads = NEO::SysCalls::getNumThreads();
        }

        return commandList;
    }

    return commandList;
}

} // namespace L0
