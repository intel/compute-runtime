/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/constants.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <chrono>
#include <limits>

struct _ze_fence_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_fence_handle_t>);

namespace L0 {

struct CommandQueueImp;

struct Fence : _ze_fence_handle_t {
    static Fence *create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc);
    virtual ~Fence() = default;
    MOCKABLE_VIRTUAL ze_result_t destroy() {
        delete this;
        return ZE_RESULT_SUCCESS;
    }
    MOCKABLE_VIRTUAL ze_result_t hostSynchronize(uint64_t timeout);
    MOCKABLE_VIRTUAL ze_result_t queryStatus();
    MOCKABLE_VIRTUAL ze_result_t assignTaskCountFromCsr();
    MOCKABLE_VIRTUAL ze_result_t reset(bool signaled);

    static Fence *fromHandle(ze_fence_handle_t handle) { return static_cast<Fence *>(handle); }

    inline ze_fence_handle_t toHandle() { return this; }

  protected:
    Fence(CommandQueueImp *cmdQueueImp) : cmdQueue(cmdQueueImp) {}

    std::chrono::microseconds gpuHangCheckPeriod{CommonConstants::gpuHangCheckTimeInUS};
    CommandQueueImp *cmdQueue;
    TaskCountType taskCount = 0;
};

} // namespace L0
