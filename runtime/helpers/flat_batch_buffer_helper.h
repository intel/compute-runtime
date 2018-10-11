/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/address_patch.h"
#include "runtime/command_stream/submissions_aggregator.h"
#include <map>
#include <vector>

namespace OCLRT {

enum class DispatchMode;
class MemoryManager;
class ExecutionEnvironment;

class FlatBatchBufferHelper {
  public:
    FlatBatchBufferHelper(ExecutionEnvironment &executionEnvironemnt) : executionEnvironemnt(executionEnvironemnt) {}
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
    static void fixCrossThreadDataInfo(std::vector<PatchInfoData> &data, size_t offsetCrossThreadData, uint64_t gpuAddress);

    std::vector<CommandChunk> &getCommandChunkList() { return commandChunkList; }
    std::vector<PatchInfoData> &getPatchInfoCollection() { return patchInfoCollection; }
    std::map<uint64_t, uint64_t> &getBatchBufferStartAddressSequence() { return batchBufferStartAddressSequence; }

  protected:
    MemoryManager *getMemoryManager() const;
    ExecutionEnvironment &executionEnvironemnt;

    std::vector<PatchInfoData> patchInfoCollection;
    std::vector<CommandChunk> commandChunkList;
    std::map<uint64_t, uint64_t> batchBufferStartAddressSequence;
};

} // namespace OCLRT