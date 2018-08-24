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

#pragma once

#include "runtime/helpers/address_patch.h"
#include "runtime/command_stream/submissions_aggregator.h"
#include <map>
#include <vector>

namespace OCLRT {

enum class DispatchMode;
class MemoryManager;

class FlatBatchBufferHelper {
  public:
    FlatBatchBufferHelper(MemoryManager *memoryManager) : memoryManager(memoryManager) {}
    virtual ~FlatBatchBufferHelper(){};
    MOCKABLE_VIRTUAL bool setPatchInfoData(const PatchInfoData &data);
    MOCKABLE_VIRTUAL bool removePatchInfoData(uint64_t targetLocation);
    MOCKABLE_VIRTUAL bool registerCommandChunk(uint64_t baseCpu, uint64_t baseGpu, uint64_t startOffset, uint64_t endOffset);
    MOCKABLE_VIRTUAL bool registerCommandChunk(CommandChunk &commandChunk);
    MOCKABLE_VIRTUAL bool registerCommandChunk(BatchBuffer &batchBuffer, size_t batchBufferStartCommandSize);
    MOCKABLE_VIRTUAL bool registerBatchBufferStartAddress(uint64_t commandAddress, uint64_t startAddress);
    virtual GraphicsAllocation *flattenBatchBuffer(BatchBuffer &batchBuffer, size_t &sizeBatchBuffer, DispatchMode dispatchMode) = 0;
    virtual char *getIndirectPatchCommands(size_t &indirectPatchCommandsSize, std::vector<PatchInfoData> &indirectPatchInfo) = 0;
    virtual void removePipeControlData(size_t pipeControlLocationSize, void *pipeControlForNooping) = 0;
    void setMemoryManager(MemoryManager *memoryManager) { this->memoryManager = memoryManager; }
    static void fixCrossThreadDataInfo(std::vector<PatchInfoData> &data, size_t offsetCrossThreadData, uint64_t gpuAddress);

    std::vector<CommandChunk> &getCommandChunkList() { return commandChunkList; }
    std::vector<PatchInfoData> &getPatchInfoCollection() { return patchInfoCollection; }
    std::map<uint64_t, uint64_t> &getBatchBufferStartAddressSequence() { return batchBufferStartAddressSequence; }

  protected:
    MemoryManager *memoryManager = nullptr;

    std::vector<PatchInfoData> patchInfoCollection;
    std::vector<CommandChunk> commandChunkList;
    std::map<uint64_t, uint64_t> batchBufferStartAddressSequence;
};

} // namespace OCLRT