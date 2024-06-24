/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>
#include <map>
#include <mutex>

namespace NEO {
class SVMAllocsManager;
class CommandStreamReceiver;
class Device;
struct SvmAllocationData;

using ChunkCopyFunction = std::function<int32_t(void *, void *, const void *, size_t)>;

struct StagingBufferTracker {
    void *stagingBuffer;
    uint64_t taskCount;
};

class StagingBufferManager {
  public:
    StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields);
    ~StagingBufferManager();

    bool isValidForCopy(Device &device, void *dstPtr, const void *srcPtr, bool hasDependencies) const;
    int32_t performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction chunkCopyFunc, CommandStreamReceiver *csr);

  private:
    void *getExistingBuffer(uint64_t taskCount, uint32_t rootDeviceIndex);
    void *allocateStagingBuffer();
    void storeBuffer(void *stagingBuffer, uint64_t taskCount);
    int32_t performChunkCopy(void *chunkDst, const void *chunkSrc, size_t size, ChunkCopyFunction chunkCopyFunc, CommandStreamReceiver *csr);

    size_t chunkSize = MemoryConstants::pageSize2M;

    std::vector<std::pair<SvmAllocationData *, uint64_t>> stagingBuffers;
    std::mutex mtx;

    SVMAllocsManager *svmAllocsManager;
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
};

} // namespace NEO
