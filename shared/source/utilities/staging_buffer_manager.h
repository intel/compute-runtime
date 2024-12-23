/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>

namespace NEO {
class SVMAllocsManager;
class CommandStreamReceiver;
class Device;
class HeapAllocator;

using ChunkCopyFunction = std::function<int32_t(void *, void *, size_t)>;
using ChunkTransferImageFunc = std::function<int32_t(void *, const size_t *, const size_t *)>;

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
    CommandStreamReceiver *csr = nullptr;
    uint64_t taskCountToWait = 0;

    bool isReady() const;
    void freeChunk() const;
};

struct UserDstData {
    void *ptr;
    size_t size;
};

struct StagingTransferStatus {
    int32_t chunkCopyStatus = 0; // status from L0/OCL chunk copy
    WaitStatus waitStatus = WaitStatus::ready;
};

using StagingQueue = std::queue<std::pair<UserDstData, StagingBufferTracker>>;

class StagingBufferManager {
  public:
    StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields, bool requiresWritable);
    ~StagingBufferManager();
    StagingBufferManager(StagingBufferManager &&other) noexcept = delete;
    StagingBufferManager(const StagingBufferManager &other) = delete;
    StagingBufferManager &operator=(StagingBufferManager &&other) noexcept = delete;
    StagingBufferManager &operator=(const StagingBufferManager &other) = delete;

    bool isValidForCopy(const Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId) const;
    bool isValidForStagingTransferImage(const Device &device, const void *ptr, bool hasDependencies) const;

    StagingTransferStatus performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr);
    StagingTransferStatus performImageTransfer(const void *ptr, const size_t *globalOrigin, const size_t *globalRegion, size_t rowPitch, ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead);

    std::pair<HeapAllocator *, uint64_t> requestStagingBuffer(size_t &size);
    void trackChunk(const StagingBufferTracker &tracker);

  private:
    std::pair<HeapAllocator *, uint64_t> getExistingBuffer(size_t &size);
    void *allocateStagingBuffer(size_t size);
    void clearTrackedChunks();

    template <class Func, class... Args>
    StagingTransferStatus performChunkTransfer(bool isRead, void *userPtr, size_t size, StagingQueue &currentStagingBuffers, CommandStreamReceiver *csr, Func &func, Args... args);

    WaitStatus fetchHead(StagingQueue &stagingQueue, StagingBufferTracker &tracker) const;
    WaitStatus drainAndReleaseStagingQueue(StagingQueue &stagingQueue) const;

    size_t chunkSize = MemoryConstants::pageSize2M;
    std::mutex mtx;
    std::vector<StagingBuffer> stagingBuffers;
    std::vector<StagingBufferTracker> trackers;

    SVMAllocsManager *svmAllocsManager;
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    const bool requiresWritable = false;
};

} // namespace NEO
