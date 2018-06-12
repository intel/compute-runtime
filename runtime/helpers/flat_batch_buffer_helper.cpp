/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/helpers/flat_batch_buffer_helper.h"

namespace OCLRT {

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
}; // namespace OCLRT