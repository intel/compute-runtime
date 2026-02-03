/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/heap_base_address_model.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_helpers.h"
#include "level_zero/core/source/helpers/api_handle_helper.h"
#include <level_zero/ze_api.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

struct _ze_command_queue_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_COMMAND_QUEUE> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_command_queue_handle_t>);

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
class LinearStream;
class MemoryManager;
using ResidencyContainer = std::vector<GraphicsAllocation *>;

} // namespace NEO

struct UnifiedMemoryControls;

namespace L0 {
struct Kernel;

struct CommandQueue : _ze_command_queue_handle_t {
    static constexpr size_t defaultQueueCmdBufferSize = 128 * MemoryConstants::kiloByte;
    static constexpr size_t minCmdBufferPtrAlign = 8;
    static constexpr size_t totalCmdBufferSize =
        defaultQueueCmdBufferSize +
        MemoryConstants::cacheLineSize +
        NEO::CSRequirements::csOverfetchSize;

    template <typename Type>
    struct Allocator {
        static CommandQueue *allocate(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) {
            return new Type(device, csr, desc);
        }
    };

    CommandQueue() = delete;
    CommandQueue(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc);

    virtual ~CommandQueue() = default;

    virtual ze_result_t createFence(const ze_fence_desc_t *desc, ze_fence_handle_t *phFence) = 0;
    virtual ze_result_t destroy();
    virtual ze_result_t executeCommandLists(uint32_t numCommandLists,
                                            ze_command_list_handle_t *phCommandLists,
                                            ze_fence_handle_t hFence, bool performMigration,
                                            NEO::LinearStream *parentImmediateCommandlistLinearStream,
                                            std::unique_lock<std::mutex> *outerLockForIndirect) = 0;
    virtual ze_result_t synchronize(uint64_t timeout);
    ze_result_t getOrdinal(uint32_t *pOrdinal);
    ze_result_t getIndex(uint32_t *pIndex);

    static CommandQueue *create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, bool immediateCmdListQueue, ze_result_t &resultValue);

    static CommandQueue *fromHandle(ze_command_queue_handle_t handle) {
        return static_cast<CommandQueue *>(handle);
    }

    static QueueProperties extractQueueProperties(const ze_command_queue_desc_t &desc);

    virtual void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration);
    virtual void makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer);

    ze_command_queue_handle_t toHandle() { return this; }

    bool peekIsCopyOnlyCommandQueue() const { return this->isCopyOnlyCommandQueue; }

    void unregisterCsrClient();
    void registerCsrClient();

    TaskCountType getTaskCount() const { return taskCount; }
    void setTaskCount(TaskCountType newTaskCount) { taskCount = newTaskCount; }

    inline bool getAndClearIsWalkerWithProfilingEnqueued() {
        bool retVal = this->isWalkerWithProfilingEnqueued;
        this->isWalkerWithProfilingEnqueued = false;
        return retVal;
    }
    inline void setPatchingPreamble(bool patching) {
        this->patchingPreamble = patching;
    }
    inline bool getPatchingPreamble() const {
        return this->patchingPreamble;
    }
    inline bool getSaveWaitForPreamble() const {
        return this->saveWaitForPreamble;
    }
    void saveTagAndTaskCountForCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *commandListHandles,
                                            NEO::GraphicsAllocation *tagGpuAllocation, TaskCountType submittedTaskCount);

    MOCKABLE_VIRTUAL ze_result_t initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue);

    Device *getDevice() { return device; }

    NEO::CommandStreamReceiver *getCsr() const { return csr; }

    MOCKABLE_VIRTUAL NEO::WaitStatus reserveLinearStreamSize(size_t size);

    ze_command_queue_mode_t getCommandQueueMode() const {
        return desc.mode;
    }

    bool isSynchronousMode() const {
        bool syncMode = getCommandQueueMode() == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        if (NEO::debugManager.flags.MakeEachEnqueueBlocking.get()) {
            syncMode |= true;
        }
        return syncMode;
    }

    bool getUseKmdWaitFunction() const {
        return useKmdWaitFunction;
    }

    virtual bool getPreemptionCmdProgramming() = 0;
    void printKernelsPrintfOutput(bool hangDetected);
    void checkAssert();
    void clearHeapContainer() {
        heapContainer.clear();
    }
    NEO::LinearStream *getStartingCmdBuffer() const {
        return startingCmdBuffer;
    }
    void triggerBbStartJump() {
        forceBbStartJump = true;
    }
    void makeResidentForResidencyContainer(const NEO::ResidencyContainer &residencyContainer);

  protected:
    bool frontEndTrackingEnabled() const;

    MOCKABLE_VIRTUAL NEO::SubmissionStatus submitBatchBuffer(size_t offset, void *endingCmdPtr,
                                                             bool isCooperative);

    ze_result_t synchronizeByPollingForTaskCount(uint64_t timeoutNanoseconds);

    void postSyncOperations(bool hangDetected);

    CommandListStateChangeList stateChanges;
    CommandBufferManager buffers;
    NEO::LinearStream commandStream{};
    NEO::LinearStream firstCmdListStream{};
    NEO::HeapContainer heapContainer;
    ze_command_queue_desc_t desc;
    std::vector<std::weak_ptr<Kernel>> printfKernelContainer;

    Device *device = nullptr;
    NEO::CommandStreamReceiver *csr = nullptr;
    NEO::LinearStream *startingCmdBuffer = nullptr;

    uint32_t partitionCount = 1;
    uint32_t activeSubDevices = 1;
    std::atomic<TaskCountType> taskCount = 0;
    NEO::HeapAddressModel cmdListHeapAddressModel = NEO::HeapAddressModel::privateHeaps;

    uint32_t currentStateChangeIndex = 0;

    std::atomic<bool> cmdListWithAssertExecuted = false;
    bool useKmdWaitFunction = false;
    bool forceBbStartJump = false;
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
