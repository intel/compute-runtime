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
#include <memory>
#include <mutex>

namespace NEO {
class SVMAllocsManager;
class CommandStreamReceiver;
class Device;
class HeapAllocator;

using ChunkCopyFunction = std::function<int32_t(void *, void *, const void *, size_t)>;

class StagingBuffer {
  public:
    StagingBuffer(void *baseAddress, size_t size);
    StagingBuffer(StagingBuffer &&other);
    StagingBuffer(const StagingBuffer &other) = delete;
    StagingBuffer &operator=(StagingBuffer &&other) noexcept = delete;
    StagingBuffer &operator=(const StagingBuffer &other) = delete;

    void *getBaseAddress() const {
        return baseAddress;
    }
    HeapAllocator *getAllocator() const {
        return allocator.get();
    }

  private:
    void *baseAddress;
    std::unique_ptr<HeapAllocator> allocator;
};

struct StagingBufferTracker {
    HeapAllocator *allocator = nullptr;
    uint64_t chunkAddress = 0;
    size_t size = 0;
    uint64_t taskCountToWait = 0;
};

class StagingBufferManager {
  public:
    StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields);
    ~StagingBufferManager();
    StagingBufferManager(StagingBufferManager &&other) noexcept = delete;
    StagingBufferManager(const StagingBufferManager &other) = delete;
    StagingBufferManager &operator=(StagingBufferManager &&other) noexcept = delete;
    StagingBufferManager &operator=(const StagingBufferManager &other) = delete;

    bool isValidForCopy(const Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId) const;
    bool isValidForStagingWriteImage(const Device &device, size_t size) const;

    int32_t performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr);
    std::pair<HeapAllocator *, uint64_t> requestStagingBuffer(size_t &size, CommandStreamReceiver *csr);
    void trackChunk(const StagingBufferTracker &tracker);

  private:
    std::pair<HeapAllocator *, uint64_t> getExistingBuffer(size_t &size);
    void *allocateStagingBuffer(size_t size);
    void clearTrackedChunks(CommandStreamReceiver *csr);

    int32_t performChunkCopy(void *chunkDst, const void *chunkSrc, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr);

    size_t chunkSize = MemoryConstants::pageSize2M;
    std::mutex mtx;
    std::vector<StagingBuffer> stagingBuffers;
    std::vector<StagingBufferTracker> trackers;

    SVMAllocsManager *svmAllocsManager;
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
};

} // namespace NEO
