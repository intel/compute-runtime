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
#include "shared/source/utilities/wait_util.h"

namespace L0 {

Fence *Fence::create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc) {
    auto fence = new FenceImp(cmdQueue);
    UNRECOVERABLE_IF(fence == nullptr);
    fence->reset();
    return fence;
}

ze_result_t FenceImp::queryStatus() {
    auto csr = cmdQueue->getCsr();
    csr->downloadAllocations();

    auto *hostAddr = csr->getTagAddress();

    return csr->testTaskCountReady(hostAddr, taskCount) ? ZE_RESULT_SUCCESS : ZE_RESULT_NOT_READY;
}

ze_result_t FenceImp::assignTaskCountFromCsr() {
    auto csr = cmdQueue->getCsr();
    taskCount = csr->peekTaskCount() + 1;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FenceImp::reset() {
    taskCount = std::numeric_limits<uint32_t>::max();
    return ZE_RESULT_SUCCESS;
}

ze_result_t FenceImp::hostSynchronize(uint64_t timeout) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    uint64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (cmdQueue->getCsr()->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    if (std::numeric_limits<uint32_t>::max() == taskCount) {
        return ZE_RESULT_NOT_READY;
    }

    if (timeout == 0) {
        return queryStatus();
    }

    time1 = std::chrono::high_resolution_clock::now();
    while (timeDiff < timeout) {
        ret = queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            return ZE_RESULT_SUCCESS;
        }

        NEO::WaitUtils::waitFunction(nullptr, 0u);

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        }

        time2 = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1).count();
    }

    return ret;
}

} // namespace L0
