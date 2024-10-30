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

StagingBufferManager::StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields) : svmAllocsManager(svmAllocsManager), rootDeviceIndices(rootDeviceIndices), deviceBitfields(deviceBitfields) {
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
 * This method performs 4 steps for single chunk copy
 * 1. Get existing chunk of staging buffer, if can't - allocate new one,
 * 2. Perform actual copy,
 * 3. Store used buffer to tracking container (with current task count)
 * 4. Update tag if required to reuse this buffer in next chunk copies
 */
int32_t StagingBufferManager::performChunkCopy(void *chunkDst, const void *chunkSrc, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr) {
    auto allocatedSize = size;
    auto [allocator, chunkBuffer] = requestStagingBuffer(allocatedSize, csr);
    auto ret = chunkCopyFunc(chunkDst, addrToPtr(chunkBuffer), chunkSrc, size);
    trackChunk({allocator, chunkBuffer, allocatedSize, csr->peekTaskCount()});
    if (csr->isAnyDirectSubmissionEnabled()) {
        csr->flushTagUpdate();
    }
    return ret;
}

/*
 * This method copies data between non-USM and USM allocations by splitting transfers into chunks.
 * Each chunk copy contains staging buffer which should be used instead of non-usm memory during transfers on GPU.
 * Caller provides actual function to transfer data for single chunk.
 */
int32_t StagingBufferManager::performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction &chunkCopyFunc, CommandStreamReceiver *csr) {
    auto copiesNum = size / chunkSize;
    auto remainder = size % chunkSize;

    for (auto i = 0u; i < copiesNum; i++) {
        auto chunkDst = ptrOffset(dstPtr, i * chunkSize);
        auto chunkSrc = ptrOffset(srcPtr, i * chunkSize);
        auto ret = performChunkCopy(chunkDst, chunkSrc, chunkSize, chunkCopyFunc, csr);
        if (ret) {
            return ret;
        }
    }

    if (remainder != 0) {
        auto chunkDst = ptrOffset(dstPtr, copiesNum * chunkSize);
        auto chunkSrc = ptrOffset(srcPtr, copiesNum * chunkSize);
        auto ret = performChunkCopy(chunkDst, chunkSrc, remainder, chunkCopyFunc, csr);
        if (ret) {
            return ret;
        }
    }
    return 0;
}

/*
 * This method returns allocator and chunk from staging buffer.
 * Creates new staging buffer if it failed to allocate chunk from existing buffers.
 */
std::pair<HeapAllocator *, uint64_t> StagingBufferManager::requestStagingBuffer(size_t &size, CommandStreamReceiver *csr) {
    auto lock = std::lock_guard<std::mutex>(mtx);

    auto [allocator, chunkBuffer] = getExistingBuffer(size);
    if (chunkBuffer != 0) {
        return {allocator, chunkBuffer};
    }

    clearTrackedChunks(csr);

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

bool StagingBufferManager::isValidForStagingWriteImage(const Device &device, size_t size) const {
    auto thresholdSizeForImages = 32 * MemoryConstants::megaByte;
    auto stagingCopyEnabled = true;
    if (debugManager.flags.EnableCopyWithStagingBuffers.get() != -1) {
        stagingCopyEnabled = debugManager.flags.EnableCopyWithStagingBuffers.get();
    }
    return stagingCopyEnabled && (0 < size && size <= thresholdSizeForImages);
}

void StagingBufferManager::clearTrackedChunks(CommandStreamReceiver *csr) {
    for (auto iterator = trackers.begin(); iterator != trackers.end();) {
        if (csr->testTaskCountReady(csr->getTagAddress(), iterator->taskCountToWait)) {
            iterator->allocator->free(iterator->chunkAddress, iterator->size);
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
