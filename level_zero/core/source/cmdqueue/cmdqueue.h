/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"

#include <level_zero/ze_api.h>

#include <atomic>

struct _ze_command_queue_handle_t {};

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct Device;

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
                                            ze_fence_handle_t hFence, bool performMigration) = 0;
    virtual ze_result_t executeCommands(uint32_t numCommands,
                                        void *phCommands,
                                        ze_fence_handle_t hFence) = 0;
    virtual ze_result_t synchronize(uint64_t timeout) = 0;

    static CommandQueue *create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, ze_result_t &resultValue);

    static CommandQueue *fromHandle(ze_command_queue_handle_t handle) {
        return static_cast<CommandQueue *>(handle);
    }

    ze_command_queue_handle_t toHandle() { return this; }

    void setCommandQueuePreemptionMode(NEO::PreemptionMode newPreemptionMode) {
        commandQueuePreemptionMode = newPreemptionMode;
    }

    bool peekIsCopyOnlyCommandQueue() const { return this->isCopyOnlyCommandQueue; }

  protected:
    NEO::PreemptionMode commandQueuePreemptionMode = NEO::PreemptionMode::Initial;
    uint32_t partitionCount = 1;
    uint32_t activeSubDevices = 1;
    bool preemptionCmdSyncProgramming = true;
    bool commandQueueDebugCmdsProgrammed = false;
    bool isCopyOnlyCommandQueue = false;
    bool internalUsage = false;
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
