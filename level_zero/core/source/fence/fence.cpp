/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence/fence.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "hw_helpers.h"

namespace L0 {

Fence *Fence::create(CommandQueueImp *cmdQueue, const ze_fence_desc_t *desc) {
    auto fence = new FenceImp(cmdQueue);
    UNRECOVERABLE_IF(fence == nullptr);

    fence->initialize();

    return fence;
}

FenceImp::~FenceImp() {
    cmdQueue->getDevice()->getDriverHandle()->getMemoryManager()->freeGraphicsMemory(allocation);
    allocation = nullptr;
}

ze_result_t FenceImp::queryStatus() {
    auto csr = cmdQueue->getCsr();
    if (csr) {
        csr->downloadAllocations();
    }

    auto hostAddr = static_cast<uint64_t *>(allocation->getUnderlyingBuffer());
    return *hostAddr == Fence::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
}

bool FenceImp::initialize() {
    NEO::AllocationProperties properties(
        cmdQueue->getDevice()->getRootDeviceIndex(), MemoryConstants::cacheLineSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, cmdQueue->getDevice()->getNEODevice()->getDeviceBitfield());
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

ze_result_t FenceImp::hostSynchronize(uint64_t timeout) {
    std::chrono::high_resolution_clock::time_point time1, time2;
    uint64_t timeDiff = 0;
    ze_result_t ret = ZE_RESULT_NOT_READY;

    if (cmdQueue->getCsr()->getType() == NEO::CommandStreamReceiverType::CSR_AUB) {
        return ZE_RESULT_SUCCESS;
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

        std::this_thread::yield();
        NEO::CpuIntrinsics::pause();

        if (timeout == std::numeric_limits<uint64_t>::max()) {
            continue;
        }

        time2 = std::chrono::high_resolution_clock::now();
        timeDiff = std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1).count();
    }

    return ret;
}

} // namespace L0
