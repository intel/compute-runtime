/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence/fence.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "hw_helpers.h"

namespace L0 {

struct FenceImp : public Fence {
    FenceImp(CommandQueueImp *cmdQueueImp) : cmdQueue(cmdQueueImp) {}

    ~FenceImp() override {
        cmdQueue->getDevice()->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(allocation);
        allocation = nullptr;
    }

    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t hostSynchronize(uint32_t timeout) override;

    ze_result_t queryStatus() override {
        auto csr = cmdQueue->getCsr();
        if (csr) {
            csr->downloadAllocation(*allocation);
        }

        auto hostAddr = static_cast<uint64_t *>(allocation->getUnderlyingBuffer());
        return *hostAddr == Fence::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
    }

    ze_result_t reset() override;

    static Fence *fromHandle(ze_fence_handle_t handle) { return static_cast<Fence *>(handle); }

    inline ze_fence_handle_t toHandle() { return this; }

    bool initialize();

  protected:
    CommandQueueImp *cmdQueue;
};

Fence *Fence::create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc) {
    auto fence = new FenceImp(cmdQueue);
    UNRECOVERABLE_IF(fence == nullptr);

    fence->initialize();

    return fence;
}

bool FenceImp::initialize() {
    NEO::AllocationProperties properties(
        cmdQueue->getDevice()->getRootDeviceIndex(), MemoryConstants::cacheLineSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    properties.alignment = MemoryConstants::cacheLineSize;
    allocation = cmdQueue->getDevice()->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    UNRECOVERABLE_IF(allocation == nullptr);

    reset();

    return true;
}

ze_result_t FenceImp::reset() {
    auto hostAddress = static_cast<uint64_t *>(allocation->getUnderlyingBuffer());
    *(hostAddress) = Fence::STATE_CLEARED;

    NEO::CpuIntrinsics::clFlush(hostAddress);

    return ZE_RESULT_SUCCESS;
}

ze_result_t FenceImp::hostSynchronize(uint32_t timeout) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    int64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (cmdQueue->getCsr()->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
    }

    waitForTaskCountWithKmdNotifyFallbackHelper(cmdQueue->getCsr(), cmdQueue->getTaskCount(), 0, false, false);

    if (timeout == 0) {
        return queryStatus();
    }

    time1 = std::chrono::high_resolution_clock::now();
    while (timeDiff < timeout) {
        ret = queryStatus();
        if (ret == ZE_RESULT_SUCCESS) {
            return ZE_RESULT_SUCCESS;
        }

        std::this_thread::yield();
        NEO::CpuIntrinsics::pause();

        if (timeout == std::numeric_limits<uint32_t>::max()) {
            continue;
        }

        time2 = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1).count();
    }

    return ret;
}

} // namespace L0
