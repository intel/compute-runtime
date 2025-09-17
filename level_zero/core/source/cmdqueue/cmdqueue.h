/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/heap_base_address_model.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"
#include <level_zero/ze_api.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

struct _ze_command_queue_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_COMMAND_QUEUE> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_command_queue_handle_t>);

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
using ResidencyContainer = std::vector<GraphicsAllocation *>;

} // namespace NEO

struct UnifiedMemoryControls;

namespace L0 {
struct Device;

struct QueueProperties {
    NEO::SynchronizedDispatchMode synchronizedDispatchMode = NEO::SynchronizedDispatchMode::disabled;
    bool interruptHint = false;
    bool copyOffloadHint = false;
    std::optional<int> priorityLevel = std::nullopt;
};

struct CommandQueue : _ze_command_queue_handle_t {
    template <typename Type>
    struct Allocator {
        static CommandQueue *allocate(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) {
            return new Type(device, csr, desc);
        }
    };

    virtual ~CommandQueue() = default;

    virtual ze_result_t createFence(const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) = 0;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration,
                                            NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                            std::unique_lock<std::mutex> *outerLockForIndirect) = 0;
    virtual ze_result_t synchronize(uint64_t timeout) = 0;
    virtual ze_result_t getOrdinal(uint32_t *pOrdinal) = 0;
    virtual ze_result_t getIndex(uint32_t *pIndex) = 0;

    static CommandQueue *create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, bool immediateCmdListQueue, ze_result_t &resultValue);

    static CommandQueue *fromHandle(ze_command_queue_handle_t handle) {
        return static_cast<CommandQueue *>(handle);
    }

    static QueueProperties extractQueueProperties(const ze_command_queue_desc_t &desc);

    virtual void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) = 0;
    virtual void makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer) = 0;

    ze_command_queue_handle_t toHandle() { return this; }

    bool peekIsCopyOnlyCommandQueue() const { return this->isCopyOnlyCommandQueue; }

    virtual void unregisterCsrClient() = 0;
    virtual void registerCsrClient() = 0;

    TaskCountType getTaskCount() const { return taskCount; }
    void setTaskCount(TaskCountType newTaskCount) { taskCount = newTaskCount; }

    inline bool getAndClearIsWalkerWithProfilingEnqueued() {
        bool retVal = this->isWalkerWithProfilingEnqueued;
        this->isWalkerWithProfilingEnqueued = false;
        return retVal;
    }
    inline void setPatchingPreamble(bool patching, bool saveWait) {
        this->patchingPreamble = patching;
        this->saveWaitForPreamble = saveWait;
    }
    inline bool getPatchingPreamble() const {
        return this->patchingPreamble;
    }
    inline bool getSaveWaitForPreamble() const {
        return this->saveWaitForPreamble;
    }
    void saveTagAndTaskCountForCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *commandListHandles,
                                            NEO::GraphicsAllocation *tagGpuAllocation, TaskCountType submittedTaskCount);

  protected:
    bool frontEndTrackingEnabled() const;

    uint32_t partitionCount = 1;
    uint32_t activeSubDevices = 1;
    std::atomic<TaskCountType> taskCount = 0;
    NEO::HeapAddressModel cmdListHeapAddressModel = NEO::HeapAddressModel::privateHeaps;

    bool preemptionCmdSyncProgramming = true;
    bool commandQueueDebugCmdsProgrammed = false;
    bool isCopyOnlyCommandQueue = false;
    bool internalUsage = false;
    bool frontEndStateTracking = false;
    bool pipelineSelectStateTracking = false;
    bool stateComputeModeTracking = false;
    bool stateBaseAddressTracking = false;
    bool doubleSbaWa = false;
    bool dispatchCmdListBatchBufferAsPrimary = false;
    bool heaplessModeEnabled = false;
    bool heaplessStateInitEnabled = false;
    bool isWalkerWithProfilingEnqueued = false;
    bool patchingPreamble = false;
    bool saveWaitForPreamble = false;
};

using CommandQueueAllocatorFn = CommandQueue *(*)(Device *device, NEO::CommandStreamReceiver *csr,
                                                  const ze_command_queue_desc_t *desc);
extern CommandQueueAllocatorFn commandQueueFactory[];

template <uint32_t productFamily, typename CommandQueueType>
struct CommandQueuePopulateFactory {
    CommandQueuePopulateFactory() {
        commandQueueFactory[productFamily] = CommandQueue::Allocator<CommandQueueType>::allocate;
    }
};

} // namespace L0
