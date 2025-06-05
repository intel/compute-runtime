/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>

namespace NEO {
class SVMAllocsManager;
class CommandStreamReceiver;
class Device;
class HeapAllocator;

using ChunkCopyFunction = std::function<int32_t(void *, void *, size_t)>;
using ChunkTransferImageFunc = std::function<int32_t(void *, const size_t *, const size_t *)>;
using ChunkTransferBufferFunc = std::function<int32_t(void *, size_t, size_t)>;
class StagingBuffer : NEO::NonCopyableClass {
  public:
    StagingBuffer(void *baseAddress, size_t size);
    StagingBuffer(StagingBuffer &&other) noexcept;
    StagingBuffer &operator=(StagingBuffer &&other) noexcept = delete;

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

static_assert(NEO::NonCopyable<StagingBuffer>);

struct StagingBufferTracker {
    HeapAllocator *allocator = nullptr;
    uint64_t chunkAddress = 0;
    size_t size = 0;
    CommandStreamReceiver *csr = nullptr;
    uint64_t taskCountToWait = 0;

    bool isReady() const;
    void freeChunk() const;
};

struct ImageMetadata {
    size_t bytesPerPixel = 0;
    size_t rowSize = 0;
    size_t rowPitch = 0;
    size_t slicePitch = 0;

    size_t rowsInChunk = 0;
    size_t slicesInChunk = 0;
};

struct UserData {
    const void *ptr = nullptr;
    size_t size = 0;
    ImageMetadata imageMetadata{};
    bool isImageOperation = false;
};

struct StagingTransferStatus {
    int32_t chunkCopyStatus = 0; // status from L0/OCL chunk copy
    WaitStatus waitStatus = WaitStatus::ready;
};

constexpr size_t maxInFlightReads = 2u;
using StagingQueue = StackVec<std::pair<UserData, StagingBufferTracker>, maxInFlightReads>;

class StagingBufferManager : NEO::NonCopyableAndNonMovableClass {
  public:
    StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields, bool requiresWritable);
    ~StagingBufferManager();

    bool isValidForCopy(const Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId);
    bool isValidForStagingTransfer(const Device &device, const void *ptr, size_t size, bool hasDependencies);

    StagingTransferStatus performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr);
    StagingTransferStatus performImageTransfer(const void *ptr, const size_t *globalOrigin, const size_t *globalRegion, size_t rowPitch, size_t slicePitch, size_t bytesPerPixel, bool isMipMapped3DImage, ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead);
    StagingTransferStatus performBufferTransfer(const void *ptr, size_t globalOffset, size_t globalSize, ChunkTransferBufferFunc &chunkTransferBufferFunc, CommandStreamReceiver *csr, bool isRead);

    std::pair<HeapAllocator *, uint64_t> requestStagingBuffer(size_t &size);
    void trackChunk(const StagingBufferTracker &tracker);

    bool registerHostPtr(const void *ptr);
    void resetDetectedPtrs();

  private:
    std::pair<HeapAllocator *, uint64_t> getExistingBuffer(size_t &size);
    void *allocateStagingBuffer(size_t size);
    void clearTrackedChunks();

    template <class Func, class... Args>
    StagingTransferStatus performChunkTransfer(size_t chunkTransferId, bool isRead, const UserData &userData, StagingQueue &currentStagingBuffers, CommandStreamReceiver *csr, Func &func, Args... args);
    StagingTransferStatus performImageSlicesTransfer(StagingQueue &stagingQueue, size_t &submittedChunks, const void *ptr, auto sliceOffset,
                                                     size_t baseRowOffset, size_t rowsToCopy, size_t origin[4], size_t region[3], ImageMetadata &imageMetadata,
                                                     ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead);

    WaitStatus copyStagingToHost(const std::pair<UserData, StagingBufferTracker> &transfer, StagingBufferTracker &tracker) const;
    WaitStatus drainAndReleaseStagingQueue(bool isRead, const StagingQueue &stagingQueue, size_t numOfSubmittedTransfers) const;
    void copyImageToHost(void *dst, const void *stagingBuffer, size_t size, const ImageMetadata &imageData) const;

    bool isValidForStaging(const Device &device, const void *ptr, size_t size, bool hasDependencies);

    size_t chunkSize = 0;
    std::mutex mtx;
    std::vector<StagingBuffer> stagingBuffers;
    std::vector<StagingBufferTracker> trackers;

    SVMAllocsManager *svmAllocsManager;
    const RootDeviceIndicesContainer rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> deviceBitfields;
    const bool requiresWritable = false;

    std::set<const void *> detectedHostPtrs;
};

static_assert(NEO::NonCopyableAndNonMovable<StagingBufferManager>);

size_t getDefaultStagingBufferSize();

} // namespace NEO
