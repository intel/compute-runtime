/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_common.h>
#include <level_zero/ze_fence.h>

#include <atomic>

struct _ze_command_queue_handle_t {};

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {

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
    virtual ze_result_t synchronize(uint32_t timeout) = 0;

    static CommandQueue *create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                const ze_command_queue_desc_t *desc);

    static CommandQueue *fromHandle(ze_command_queue_handle_t handle) {
        return static_cast<CommandQueue *>(handle);
    }

    ze_command_queue_handle_t toHandle() { return this; }

    void setCommandQueuePreemptionMode(NEO::PreemptionMode newPreemptionMode) {
        commandQueuePreemptionMode = newPreemptionMode;
    }

  protected:
    std::atomic<uint32_t> commandQueuePerThreadScratchSize;
    NEO::PreemptionMode commandQueuePreemptionMode = NEO::PreemptionMode::Initial;
    bool commandQueueDebugCmdsProgrammed = false;
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
