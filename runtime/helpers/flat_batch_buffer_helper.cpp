/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/flat_batch_buffer_helper.h"

#include "core/memory_manager/graphics_allocation.h"
#include "runtime/execution_environment/execution_environment.h"

namespace NEO {

bool FlatBatchBufferHelper::setPatchInfoData(const PatchInfoData &data) {
    patchInfoCollection.push_back(data);
    return true;
}
bool FlatBatchBufferHelper::removePatchInfoData(uint64_t targetLocation) {
    for (auto it = patchInfoCollection.begin(); it != patchInfoCollection.end(); ++it) {
        if (it->targetAllocation + it->targetAllocationOffset == targetLocation) {
            patchInfoCollection.erase(it);
            break;
        }
    }
    return true;
}

bool FlatBatchBufferHelper::registerCommandChunk(uint64_t baseCpu, uint64_t baseGpu, uint64_t startOffset, uint64_t endOffset) {

    CommandChunk commandChunk;
    commandChunk.baseAddressGpu = baseGpu;
    commandChunk.baseAddressCpu = baseCpu;
    commandChunk.startOffset = startOffset;
    commandChunk.endOffset = endOffset;
    return registerCommandChunk(commandChunk);
}

bool FlatBatchBufferHelper::registerCommandChunk(BatchBuffer &batchBuffer, size_t batchBufferStartCommandSize) {
    CommandChunk commandChunk;
    commandChunk.baseAddressGpu = batchBuffer.stream->getGraphicsAllocation()->getGpuAddress();
    commandChunk.baseAddressCpu = reinterpret_cast<uint64_t>(batchBuffer.stream->getCpuBase());
    commandChunk.startOffset = batchBuffer.startOffset;
    commandChunk.endOffset = batchBuffer.chainedBatchBufferStartOffset + batchBufferStartCommandSize;
    return registerCommandChunk(commandChunk);
}

bool FlatBatchBufferHelper::registerCommandChunk(CommandChunk &commandChunk) {
    commandChunkList.push_back(commandChunk);
    return true;
}

bool FlatBatchBufferHelper::registerBatchBufferStartAddress(uint64_t commandAddress, uint64_t startAddress) {
    batchBufferStartAddressSequence.insert(std::pair<uint64_t, uint64_t>(commandAddress, startAddress));
    return true;
}

void FlatBatchBufferHelper::fixCrossThreadDataInfo(std::vector<PatchInfoData> &data, size_t offsetCrossThreadData, uint64_t gpuAddress) {
    for (auto &patchInfoData : data) {
        if (patchInfoData.sourceType == PatchInfoAllocationType::KernelArg) {
            patchInfoData.targetAllocation = gpuAddress;
            patchInfoData.targetAllocationOffset += offsetCrossThreadData;
        }
    }
}

MemoryManager *FlatBatchBufferHelper::getMemoryManager() const {
    return executionEnvironemnt.memoryManager.get();
}
}; // namespace NEO