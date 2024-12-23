/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/staging_buffer_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/heap_allocator.h"
namespace NEO {

StagingBuffer::StagingBuffer(void *baseAddress, size_t size) : baseAddress(baseAddress) {
    this->allocator = std::make_unique<HeapAllocator>(castToUint64(baseAddress), size, MemoryConstants::pageSize, 0u);
}

StagingBuffer::StagingBuffer(StagingBuffer &&other) : baseAddress(other.baseAddress) {
    this->allocator.reset(other.allocator.release());
}

bool StagingBufferTracker::isReady() const {
    return csr->testTaskCountReady(csr->getTagAddress(), taskCountToWait);
}

void StagingBufferTracker::freeChunk() const {
    allocator->free(chunkAddress, size);
}

StagingBufferManager::StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields, bool requiresWritable)
    : svmAllocsManager(svmAllocsManager), rootDeviceIndices(rootDeviceIndices), deviceBitfields(deviceBitfields), requiresWritable(requiresWritable) {
    if (debugManager.flags.StagingBufferSize.get() != -1) {
        chunkSize = debugManager.flags.StagingBufferSize.get() * MemoryConstants::kiloByte;
    }
}

StagingBufferManager::~StagingBufferManager() {
    for (auto &stagingBuffer : stagingBuffers) {
        svmAllocsManager->freeSVMAlloc(stagingBuffer.getBaseAddress());
    }
}

/*
 * This method performs single chunk transfer. If transfer is a read operation, it will fetch oldest staging
 * buffer from the queue, otherwise it allocates or reuses buffer from the pool.
 * After transfer is submitted to GPU, it stores used buffer to either queue in case of reads,
 * or tracking container for further reusage.
 */
template <class Func, class... Args>
StagingTransferStatus StagingBufferManager::performChunkTransfer(bool isRead, void *userPtr, size_t size, StagingQueue &currentStagingBuffers, CommandStreamReceiver *csr, Func &func, Args... args) {
    StagingTransferStatus result{};
    StagingBufferTracker tracker{};
    if (currentStagingBuffers.size() > 1) {
        if (fetchHead(currentStagingBuffers, tracker) == WaitStatus::gpuHang) {
            result.waitStatus = WaitStatus::gpuHang;
            return result;
        }
    } else {
        auto allocatedSize = size;
        auto [allocator, stagingBuffer] = requestStagingBuffer(allocatedSize);
        tracker = StagingBufferTracker{allocator, stagingBuffer, allocatedSize, csr};
    }

    auto stagingBuffer = addrToPtr(tracker.chunkAddress);
    if (!isRead) {
        memcpy(stagingBuffer, userPtr, size);
    }

    result.chunkCopyStatus = func(stagingBuffer, args...);

    tracker.taskCountToWait = csr->peekTaskCount();
    if (isRead) {
        UserDstData dstData{userPtr, size};
        currentStagingBuffers.push({dstData, tracker});
    } else {
        trackChunk(tracker);
    }

    if (csr->isAnyDirectSubmissionEnabled()) {
        csr->flushTagUpdate();
    }
    return result;
}

/*
 * This method copies data between non-USM and USM allocations by splitting transfers into chunks.
 * Each chunk copy contains staging buffer which should be used instead of non-usm memory during transfers on GPU.
 * Caller provides actual function to transfer data for single chunk.
 */
StagingTransferStatus StagingBufferManager::performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr) {
    StagingQueue stagingQueue;
    auto copiesNum = size / chunkSize;
    auto remainder = size % chunkSize;
    StagingTransferStatus result{};
    for (auto i = 0u; i < copiesNum; i++) {
        auto chunkDst = ptrOffset(dstPtr, i * chunkSize);
        auto chunkSrc = ptrOffset(srcPtr, i * chunkSize);
        result = performChunkTransfer(false, const_cast<void *>(chunkSrc), chunkSize, stagingQueue, csr, chunkCopyFunc, chunkDst, chunkSize);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
    }

    if (remainder != 0) {
        auto chunkDst = ptrOffset(dstPtr, copiesNum * chunkSize);
        auto chunkSrc = ptrOffset(srcPtr, copiesNum * chunkSize);
        auto result = performChunkTransfer(false, const_cast<void *>(chunkSrc), remainder, stagingQueue, csr, chunkCopyFunc, chunkDst, remainder);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
    }
    return result;
}

/*
 * This method orchestrates transfer operation for images with given origin and region.
 * Transfer is splitted into chunks, each chunk represents sub-region to transfer.
 * Each chunk contains staging buffer which should be used instead of non-usm memory during transfers on GPU.
 * Several rows are packed into single chunk unless size of single row exceeds maximum chunk size (2MB).
 * Caller provides actual function to enqueue read/write operation for single chunk.
 */
StagingTransferStatus StagingBufferManager::performImageTransfer(const void *ptr, const size_t *globalOrigin, const size_t *globalRegion, size_t rowPitch, ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead) {
    StagingQueue stagingQueue;
    size_t origin[3] = {};
    size_t region[3] = {};
    origin[0] = globalOrigin[0];
    origin[2] = globalOrigin[2];
    region[0] = globalRegion[0];
    region[2] = globalRegion[2];
    auto rowsPerChunk = std::max<size_t>(1ul, chunkSize / rowPitch);
    rowsPerChunk = std::min<size_t>(rowsPerChunk, globalRegion[1]);
    auto numOfChunks = globalRegion[1] / rowsPerChunk;
    auto remainder = globalRegion[1] % (rowsPerChunk * numOfChunks);
    StagingTransferStatus result{};

    for (auto i = 0u; i < numOfChunks; i++) {
        origin[1] = globalOrigin[1] + i * rowsPerChunk;
        region[1] = rowsPerChunk;
        auto size = region[1] * rowPitch;
        auto chunkPtr = ptrOffset(ptr, i * rowsPerChunk * rowPitch);
        result = performChunkTransfer(isRead, const_cast<void *>(chunkPtr), size, stagingQueue, csr, chunkTransferImageFunc, origin, region);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }

    if (remainder != 0) {
        origin[1] = globalOrigin[1] + numOfChunks * rowsPerChunk;
        region[1] = remainder;
        auto size = region[1] * rowPitch;
        auto chunkPtr = ptrOffset(ptr, numOfChunks * rowsPerChunk * rowPitch);
        result = performChunkTransfer(isRead, const_cast<void *>(chunkPtr), size, stagingQueue, csr, chunkTransferImageFunc, origin, region);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }

    result.waitStatus = drainAndReleaseStagingQueue(stagingQueue);
    return result;
}

/*
 * This method is used for read transfers. It waits for oldest transfer to finish
 * and copies data associated with that transfer to host allocation.
 * Returned tracker contains staging buffer ready for reuse.
 */
WaitStatus StagingBufferManager::fetchHead(StagingQueue &stagingQueue, StagingBufferTracker &tracker) const {
    auto &head = stagingQueue.front();
    auto status = head.second.csr->waitForTaskCount(head.second.taskCountToWait);
    if (status == WaitStatus::gpuHang) {
        return status;
    }

    auto &userData = head.first;
    tracker = head.second;
    auto stagingBuffer = addrToPtr(tracker.chunkAddress);
    memcpy(userData.ptr, stagingBuffer, userData.size);
    stagingQueue.pop();
    return WaitStatus::ready;
}

/*
 * Waits for all pending transfers to finish.
 * Releases staging buffers back to pool for reuse.
 */
WaitStatus StagingBufferManager::drainAndReleaseStagingQueue(StagingQueue &stagingQueue) const {
    StagingBufferTracker tracker{};
    while (!stagingQueue.empty()) {
        auto status = fetchHead(stagingQueue, tracker);
        if (status == WaitStatus::gpuHang) {
            return status;
        }
        tracker.freeChunk();
    }
    return WaitStatus::ready;
}

/*
 * This method returns allocator and chunk from staging buffer.
 * Creates new staging buffer if it failed to allocate chunk from existing buffers.
 */
std::pair<HeapAllocator *, uint64_t> StagingBufferManager::requestStagingBuffer(size_t &size) {
    auto lock = std::lock_guard<std::mutex>(mtx);

    auto [allocator, chunkBuffer] = getExistingBuffer(size);
    if (chunkBuffer != 0) {
        return {allocator, chunkBuffer};
    }

    clearTrackedChunks();

    auto [retriedAllocator, retriedChunkBuffer] = getExistingBuffer(size);
    if (retriedChunkBuffer != 0) {
        return {retriedAllocator, retriedChunkBuffer};
    }

    auto stagingBufferSize = alignUp(std::max(chunkSize, size), MemoryConstants::pageSize2M);
    auto usmHost = allocateStagingBuffer(stagingBufferSize);
    if (usmHost != nullptr) {
        StagingBuffer stagingBuffer{usmHost, stagingBufferSize};
        allocator = stagingBuffer.getAllocator();
        chunkBuffer = allocator->allocate(size);
        stagingBuffers.push_back(std::move(stagingBuffer));
    }
    return {allocator, chunkBuffer};
}

/*
 * This method will try to allocate chunk from existing staging buffer.
 * Returns allocator and chunk from consumed staging buffer.
 */
std::pair<HeapAllocator *, uint64_t> StagingBufferManager::getExistingBuffer(size_t &size) {
    uint64_t buffer = 0;
    HeapAllocator *allocator = nullptr;

    for (auto &stagingBuffer : stagingBuffers) {
        allocator = stagingBuffer.getAllocator();
        buffer = allocator->allocate(size);
        if (buffer != 0) {
            if (requiresWritable) {
                auto alloc = svmAllocsManager->getSVMAlloc(stagingBuffer.getBaseAddress())->gpuAllocations.getDefaultGraphicsAllocation();
                alloc->setTbxWritable(true, std::numeric_limits<uint32_t>::max());
                alloc->setAubWritable(true, std::numeric_limits<uint32_t>::max());
            }
            break;
        }
    }
    return {allocator, buffer};
}

void *StagingBufferManager::allocateStagingBuffer(size_t size) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
    auto hostPtr = svmAllocsManager->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    return hostPtr;
}

bool StagingBufferManager::isValidForCopy(const Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId) const {
    auto stagingCopyEnabled = device.getProductHelper().isStagingBuffersEnabled();
    if (debugManager.flags.EnableCopyWithStagingBuffers.get() != -1) {
        stagingCopyEnabled = debugManager.flags.EnableCopyWithStagingBuffers.get();
    }
    auto usmDstData = svmAllocsManager->getSVMAlloc(dstPtr);
    auto usmSrcData = svmAllocsManager->getSVMAlloc(srcPtr);
    bool hostToUsmCopy = usmSrcData == nullptr && usmDstData != nullptr;
    bool isUsedByOsContext = false;
    if (usmDstData) {
        isUsedByOsContext = usmDstData->gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex())->isUsedByOsContext(osContextId);
    }
    return stagingCopyEnabled && hostToUsmCopy && !hasDependencies && (isUsedByOsContext || size <= chunkSize);
}

bool StagingBufferManager::isValidForStagingTransferImage(const Device &device, const void *ptr, bool hasDependencies) const {
    auto stagingCopyEnabled = device.getProductHelper().isStagingBuffersEnabled();
    if (debugManager.flags.EnableCopyWithStagingBuffers.get() != -1) {
        stagingCopyEnabled = debugManager.flags.EnableCopyWithStagingBuffers.get();
    }
    auto nonUsmPtr = ptr != nullptr && svmAllocsManager->getSVMAlloc(ptr) == nullptr;
    return stagingCopyEnabled && !hasDependencies && nonUsmPtr;
}

void StagingBufferManager::clearTrackedChunks() {
    for (auto iterator = trackers.begin(); iterator != trackers.end();) {
        if (iterator->isReady()) {
            iterator->freeChunk();
            iterator = trackers.erase(iterator);
        } else {
            break;
        }
    }
}

void StagingBufferManager::trackChunk(const StagingBufferTracker &tracker) {
    auto lock = std::lock_guard<std::mutex>(mtx);
    trackers.push_back(tracker);
}

} // namespace NEO
