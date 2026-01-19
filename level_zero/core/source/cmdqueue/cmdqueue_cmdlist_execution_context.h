/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/unified_memory/unified_memory.h"

#include <level_zero/ze_api.h>

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace NEO {
class GraphicsAllocation;
class ScratchSpaceController;
} // namespace NEO

namespace L0 {
struct CommandList;
struct Device;

struct CommandListExecutionContext {

    CommandListExecutionContext() {}

    CommandListExecutionContext(ze_command_list_handle_t *commandListHandles,
                                uint32_t numCommandLists,
                                NEO::PreemptionMode contextPreemptionMode,
                                Device *device,
                                NEO::ScratchSpaceController *scratchSpaceController,
                                NEO::GraphicsAllocation *globalStatelessAllocation,
                                bool debugEnabled,
                                bool programActivePartitionConfig,
                                bool performMigration,
                                bool sipSent);

    NEO::StreamProperties cmdListBeginState{};
    uint64_t scratchGsba = 0;
    uint64_t childGpuAddressPositionBeforeDynamicPreamble = 0;
    uint64_t currentGpuAddressForChainedBbStart = 0;

    size_t spaceForResidency = 10;
    size_t bufferSpaceForPatchPreamble = 0;
    size_t totalNoopSpaceForPatchPreamble = 0;
    CommandList *firstCommandList = nullptr;
    CommandList *lastCommandList = nullptr;
    void *currentPatchForChainedBbStart = nullptr;
    void *currentPatchPreambleBuffer = nullptr;
    uintptr_t basePatchPreambleAddress = 0;
    NEO::ScratchSpaceController *scratchSpaceController = nullptr;
    NEO::GraphicsAllocation *globalStatelessAllocation = nullptr;
    std::unique_lock<std::mutex> *outerLockForIndirect = nullptr;
    std::unique_lock<NEO::CommandStreamReceiver::MutexType> *lockCSR = nullptr;

    NEO::PreemptionMode preemptionMode{};
    NEO::PreemptionMode statePreemption{};
    uint32_t perThreadScratchSpaceSlot0Size = 0;
    uint32_t perThreadScratchSpaceSlot1Size = 0;
    uint32_t totalActiveScratchPatchElements = 0;
    UnifiedMemoryControls unifiedMemoryControls{};

    bool anyCommandListWithCooperativeKernels = false;
    bool anyCommandListRequiresDisabledEUFusion = false;
    bool cachedMOCSAllowed = true;
    bool containsAnyRegularCmdList = false;
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    const bool isPreemptionModeInitial{false};
    bool isDevicePreemptionModeMidThread{};
    bool isDebugEnabled{};
    bool isNEODebuggerActive = false;
    bool stateSipRequired{};
    bool isProgramActivePartitionConfigRequired{};
    bool isMigrationRequested{};
    bool isDirectSubmissionEnabled{};
    bool isDispatchTaskCountPostSyncRequired{};
    bool hasIndirectAccess{};
    bool rtDispatchRequired = false;
    bool pipelineCmdsDispatch = false;
    bool lockScratchController = false;
    bool cmdListScratchAddressPatchingEnabled = false;
    bool containsParentImmediateStream = false;
    bool patchPreambleWaitSyncNeeded = false;
    bool regularHeapful = false;
    bool stateCacheFlushRequired = false;
    bool instructionCacheFlushRequired = false;
    bool taskCountUpdateFenceRequired = false;
};

} // namespace L0
