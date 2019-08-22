/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/experimental_command_buffer.h"

#include "core/command_stream/linear_stream.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/memory_manager/memory_manager.h"

#include <cstring>
#include <type_traits>

namespace NEO {

ExperimentalCommandBuffer::ExperimentalCommandBuffer(CommandStreamReceiver *csr, double profilingTimerResolution) : commandStreamReceiver(csr),
                                                                                                                    currentStream(nullptr),
                                                                                                                    timestampsOffset(0),
                                                                                                                    experimentalAllocationOffset(0),
                                                                                                                    defaultPrint(true),
                                                                                                                    timerResolution(profilingTimerResolution) {
    timestamps = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY});
    memset(timestamps->getUnderlyingBuffer(), 0, timestamps->getUnderlyingBufferSize());
    experimentalAllocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY});
    memset(experimentalAllocation->getUnderlyingBuffer(), 0, experimentalAllocation->getUnderlyingBufferSize());
}

ExperimentalCommandBuffer::~ExperimentalCommandBuffer() {
    auto timestamp = static_cast<uint64_t *>(timestamps->getUnderlyingBuffer());
    for (uint32_t i = 0; i < timestampsOffset / (2 * sizeof(uint64_t)); i++) {
        auto stop = static_cast<uint64_t>(*(timestamp + 1) * timerResolution);
        auto start = static_cast<uint64_t>(*timestamp * timerResolution);
        auto delta = stop - start;
        printDebugString(defaultPrint, stdout, "#%u: delta %llu start %llu stop %llu\n", i, delta, start, stop);
        timestamp += 2;
    }
    MemoryManager *memoryManager = commandStreamReceiver->getMemoryManager();
    memoryManager->freeGraphicsMemory(timestamps);
    memoryManager->freeGraphicsMemory(experimentalAllocation);

    if (currentStream.get()) {
        memoryManager->freeGraphicsMemory(currentStream->getGraphicsAllocation());
        currentStream->replaceGraphicsAllocation(nullptr);
    }
}

void ExperimentalCommandBuffer::getCS(size_t minRequiredSize) {
    if (!currentStream) {
        currentStream.reset(new LinearStream(nullptr));
    }
    minRequiredSize += CSRequirements::minCommandQueueCommandStreamSize;
    constexpr static auto additionalAllocationSize = CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize;
    commandStreamReceiver->ensureCommandBufferAllocation(*currentStream, minRequiredSize, additionalAllocationSize);
}

void ExperimentalCommandBuffer::makeResidentAllocations() {
    commandStreamReceiver->makeResident(*currentStream->getGraphicsAllocation());
    commandStreamReceiver->makeResident(*timestamps);
    commandStreamReceiver->makeResident(*experimentalAllocation);
}

} // namespace NEO
