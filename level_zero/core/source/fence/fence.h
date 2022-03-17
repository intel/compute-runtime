/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include <level_zero/ze_api.h>

#include <chrono>
#include <limits>

struct _ze_fence_handle_t {};

namespace L0 {

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

    std::chrono::microseconds gpuHangCheckPeriod{500'000};
    CommandQueueImp *cmdQueue;
    uint32_t taskCount = 0;
};

} // namespace L0
