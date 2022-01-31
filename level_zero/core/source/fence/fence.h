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

#include <limits>

struct _ze_fence_handle_t {};

namespace L0 {

struct Fence : _ze_fence_handle_t {
    static Fence *create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc);
    virtual ~Fence() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t hostSynchronize(uint64_t timeout) = 0;
    virtual ze_result_t queryStatus() = 0;
    virtual ze_result_t assignTaskCountFromCsr() = 0;
    virtual ze_result_t reset() = 0;

    static Fence *fromHandle(ze_fence_handle_t handle) { return static_cast<Fence *>(handle); }

    inline ze_fence_handle_t toHandle() { return this; }

    void setPartitionCount(uint32_t newPartitionCount) {
        partitionCount = newPartitionCount;
    }

  protected:
    uint32_t partitionCount = 1;
    uint32_t taskCount = 0;
};

struct FenceImp : public Fence {
    FenceImp(CommandQueueImp *cmdQueueImp) : cmdQueue(cmdQueueImp) {}

    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t hostSynchronize(uint64_t timeout) override;

    ze_result_t queryStatus() override;

    ze_result_t assignTaskCountFromCsr() override;

    ze_result_t reset() override;

  protected:
    CommandQueueImp *cmdQueue;
};
} // namespace L0
