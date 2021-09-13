/*
 * Copyright (C) 2020-2021 Intel Corporation
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

    volatile uint32_t *hostAddr = static_cast<uint32_t *>(allocation->getUnderlyingBuffer());
    uint32_t queryVal = Fence::STATE_CLEARED;
    for (uint32_t i = 0; i < partitionCount; i++) {
        queryVal = *hostAddr;
        if (queryVal == Fence::STATE_CLEARED) {
            break;
        }
        hostAddr = ptrOffset(hostAddr, CommandQueueImp::addressOffset);
    }
    return queryVal == Fence::STATE_CLEARED ? ZE_RESULT_NOT_READY : ZE_RESULT_SUCCESS;
}

void FenceImp::initialize() {
    NEO::AllocationProperties properties(
        cmdQueue->getDevice()->getRootDeviceIndex(), MemoryConstants::cacheLineSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, cmdQueue->getDevice()->getNEODevice()->getDeviceBitfield());
    properties.alignment = MemoryConstants::cacheLineSize;
    allocation = cmdQueue->getDevice()->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    UNRECOVERABLE_IF(allocation == nullptr);
    reset();
}

ze_result_t FenceImp::reset() {
    volatile uint32_t *hostAddress = static_cast<uint32_t *>(allocation->getUnderlyingBuffer());
    for (uint32_t i = 0; i < partitionCount; i++) {
        *hostAddress = Fence::STATE_CLEARED;
        NEO::CpuIntrinsics::clFlush(const_cast<uint32_t *>(hostAddress));
        hostAddress = ptrOffset(hostAddress, CommandQueueImp::addressOffset);
    }
    partitionCount = 1;
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
