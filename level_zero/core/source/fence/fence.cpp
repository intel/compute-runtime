/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence/fence.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace L0 {

Fence *Fence::create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc) {
    auto fence = new Fence(cmdQueue);
    UNRECOVERABLE_IF(fence == nullptr);
    fence->reset(!!(desc->flags & ZE_FENCE_FLAG_SIGNALED));
    return fence;
}

ze_result_t Fence::queryStatus() {
    auto csr = cmdQueue->getCsr();
    csr->downloadAllocations();

    auto *hostAddr = csr->getTagAddress();

    return csr->testTaskCountReady(hostAddr, taskCount) ? ZE_RESULT_SUCCESS : ZE_RESULT_NOT_READY;
}

ze_result_t Fence::assignTaskCountFromCsr() {
    auto csr = cmdQueue->getCsr();
    taskCount = csr->peekTaskCount() + 1;
    return ZE_RESULT_SUCCESS;
}

ze_result_t Fence::reset(bool signaled) {
    if (signaled) {
        taskCount = 0;
    } else {
        taskCount = std::numeric_limits<uint32_t>::max();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t Fence::hostSynchronize(uint64_t timeout) {
    std::chrono::microseconds elapsedTimeSinceGpuHangCheck{0};
    std::chrono::high_resolution_clock::time_point waitStartTime, lastHangCheckTime, currentTime;
    uint64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;
    const auto csr = cmdQueue->getCsr();

    if (csr->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    if (std::numeric_limits<uint32_t>::max() == taskCount) {
        return ZE_RESULT_NOT_READY;
    }

    if (timeout == 0) {
        return queryStatus();
    }

    waitStartTime = std::chrono::high_resolution_clock::now();
    lastHangCheckTime = waitStartTime;
    while (timeDiff < timeout) {
        ret = queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            return ZE_RESULT_SUCCESS;
        }

        currentTime = std::chrono::high_resolution_clock::now();
        elapsedTimeSinceGpuHangCheck = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastHangCheckTime);

        if (elapsedTimeSinceGpuHangCheck.count() >= gpuHangCheckPeriod.count()) {
            lastHangCheckTime = currentTime;
            if (csr->isGpuHangDetected()) {
                return ZE_RESULT_ERROR_DEVICE_LOST;
            }
        }

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        }

        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - waitStartTime).count();
    }

    return ret;
}

} // namespace L0
