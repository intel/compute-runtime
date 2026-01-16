/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_context.h"

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include <algorithm>
#include <limits>

namespace L0 {

CommandListExecutionContext::CommandListExecutionContext(
    ze_command_list_handle_t *commandListHandles,
    uint32_t numCommandLists,
    NEO::PreemptionMode contextPreemptionMode,
    Device *device,
    NEO::ScratchSpaceController *scratchSpaceController,
    NEO::GraphicsAllocation *globalStatelessAllocation,
    bool debugEnabled,
    bool programActivePartitionConfig,
    bool performMigration,
    bool sipSent) : scratchSpaceController(scratchSpaceController),
                    globalStatelessAllocation(globalStatelessAllocation),
                    preemptionMode{contextPreemptionMode},
                    statePreemption{contextPreemptionMode},
                    isPreemptionModeInitial{contextPreemptionMode == NEO::PreemptionMode::Initial},
                    isDebugEnabled{debugEnabled},
                    isProgramActivePartitionConfigRequired{programActivePartitionConfig},
                    isMigrationRequested{performMigration} {

    constexpr size_t residencyContainerSpaceForPreemption = 2;
    constexpr size_t residencyContainerSpaceForTagWrite = 1;
    constexpr size_t residencyContainerSpaceForBtdAllocation = 1;

    this->firstCommandList = CommandList::fromHandle(commandListHandles[0]);
    this->lastCommandList = CommandList::fromHandle(commandListHandles[numCommandLists - 1]);

    this->isDevicePreemptionModeMidThread = device->getDevicePreemptionMode() == NEO::PreemptionMode::MidThread && !this->isNEODebuggerActive(device);
    this->stateSipRequired = (this->isPreemptionModeInitial && this->isDevicePreemptionModeMidThread) ||
                             (!sipSent && this->isNEODebuggerActive(device));

    if (this->isDevicePreemptionModeMidThread) {
        this->spaceForResidency += residencyContainerSpaceForPreemption;
    }
    this->spaceForResidency += residencyContainerSpaceForTagWrite;
    if (device->getNEODevice()->getRTMemoryBackedBuffer()) {
        this->spaceForResidency += residencyContainerSpaceForBtdAllocation;
    }

    if (this->isMigrationRequested && device->getDriverHandle()->getMemoryManager()->getPageFaultManager() == nullptr) {
        this->isMigrationRequested = false;
    }

    this->globalInit |= (this->isProgramActivePartitionConfigRequired || this->isPreemptionModeInitial || this->stateSipRequired);
}

bool CommandListExecutionContext::isNEODebuggerActive(Device *device) {
    return device->getNEODevice()->getDebugger() && this->isDebugEnabled;
}

} // namespace L0
