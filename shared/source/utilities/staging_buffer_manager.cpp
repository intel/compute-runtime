/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/staging_buffer_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

StagingBuffer::StagingBuffer(void *baseAddress, size_t size) : baseAddress(baseAddress) {
    this->allocator = std::make_unique<HeapAllocator>(castToUint64(baseAddress), size, MemoryConstants::pageSize, 0u);
}

StagingBuffer::StagingBuffer(StagingBuffer &&other) noexcept : baseAddress(other.baseAddress) {
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
    chunkSize = getDefaultStagingBufferSize();
    if (debugManager.flags.StagingBufferSize.get() != -1) {
        chunkSize = debugManager.flags.StagingBufferSize.get() * MemoryConstants::kiloByte;
    }
}

StagingBufferManager::~StagingBufferManager() {
    freeAllocations();
}

/*
 * This method performs single chunk transfer. If transfer is a read operation, it will fetch oldest staging
 * buffer from the queue, otherwise it allocates or reuses buffer from the pool.
 * After transfer is submitted to GPU, it stores used buffer to either queue in case of reads,
 * or tracking container for further reusage.
 */
template <class Func, class... Args>
StagingTransferStatus StagingBufferManager::performChunkTransfer(size_t chunkTransferId, bool isRead, const UserData &userData, StagingQueue &currentStagingBuffers, CommandStreamReceiver *csr, Func &func, Args... args) {
    StagingTransferStatus result{};
    StagingBufferTracker tracker{};
    auto stagingBufferIndex = chunkTransferId % maxInFlightReads;
    if (isRead && chunkTransferId >= maxInFlightReads) {
        if (copyStagingToHost(currentStagingBuffers[stagingBufferIndex], tracker) == WaitStatus::gpuHang) {
            result.waitStatus = WaitStatus::gpuHang;
            return result;
        }
    } else {
        auto allocatedSize = userData.size;
        auto [allocator, stagingBuffer] = requestStagingBuffer(allocatedSize);
        tracker = StagingBufferTracker{allocator, stagingBuffer, allocatedSize, csr};
    }

    auto stagingBuffer = addrToPtr(tracker.chunkAddress);
    if (!isRead) {
        memcpy(stagingBuffer, userData.ptr, userData.size);
    }

    result.chunkCopyStatus = func(stagingBuffer, args...);

    tracker.taskCountToWait = csr->peekTaskCount();
    if (isRead) {
        currentStagingBuffers[stagingBufferIndex] = {userData, tracker};
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
        UserData userData{chunkSrc, chunkSize};
        result = performChunkTransfer(i, false, userData, stagingQueue, csr, chunkCopyFunc, chunkDst, chunkSize);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
    }

    if (remainder != 0) {
        auto chunkDst = ptrOffset(dstPtr, copiesNum * chunkSize);
        auto chunkSrc = ptrOffset(srcPtr, copiesNum * chunkSize);
        UserData userData{chunkSrc, remainder};
        auto result = performChunkTransfer(copiesNum, false, userData, stagingQueue, csr, chunkCopyFunc, chunkDst, remainder);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
    }
    return result;
}

size_t calculateSizeForRegion(size_t region[3], const ImageMetadata &imageMetadata) {
    if (region[2] > 1) {
        return (region[2] - 1) * imageMetadata.slicePitch + (region[1] - 1) * imageMetadata.rowPitch + region[0] * imageMetadata.bytesPerPixel;
    } else if (region[1] > 1) {
        return (region[1] - 1) * imageMetadata.rowPitch + region[0] * imageMetadata.bytesPerPixel;
    }
    return region[0] * imageMetadata.bytesPerPixel;
}

StagingTransferStatus StagingBufferManager::performImageSlicesTransfer(StagingQueue &stagingQueue, size_t &submittedChunks, const void *ptr, auto sliceOffset,
                                                                       size_t baseRowOffset, size_t rowsToCopy, size_t origin[4], size_t region[3], ImageMetadata &imageMetadata,
                                                                       ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead) {
    auto rowPitch = imageMetadata.rowPitch;
    auto rowsPerChunk = std::max<size_t>(1ul, chunkSize / rowPitch);
    rowsPerChunk = std::min<size_t>(rowsPerChunk, rowsToCopy);
    auto slicePitch = imageMetadata.slicePitch;
    auto numOfChunksInYDim = rowsToCopy / rowsPerChunk;
    auto remainder = rowsToCopy % (rowsPerChunk * numOfChunksInYDim);
    StagingTransferStatus result{};
    imageMetadata.slicesInChunk = region[2];

    // Split (X, Y, Z') region into several (X, Y', Z') chunks.
    for (auto rowId = 0u; rowId < numOfChunksInYDim; rowId++) {
        origin[1] = baseRowOffset + rowId * rowsPerChunk;
        region[1] = rowsPerChunk;

        auto size = calculateSizeForRegion(region, imageMetadata);
        auto chunkPtr = ptrOffset(ptr, sliceOffset * slicePitch + rowId * rowsPerChunk * rowPitch);

        imageMetadata.rowsInChunk = rowsPerChunk;
        UserData userData{chunkPtr, size, imageMetadata, true};

        result = performChunkTransfer(submittedChunks++, isRead, userData, stagingQueue, csr, chunkTransferImageFunc, origin, region);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }

    if (remainder != 0) {
        origin[1] = baseRowOffset + numOfChunksInYDim * rowsPerChunk;
        region[1] = remainder;

        auto size = calculateSizeForRegion(region, imageMetadata);
        auto chunkPtr = ptrOffset(ptr, sliceOffset * slicePitch + numOfChunksInYDim * rowsPerChunk * rowPitch);

        imageMetadata.rowsInChunk = remainder;
        UserData userData{chunkPtr, size, imageMetadata, true};

        result = performChunkTransfer(submittedChunks++, isRead, userData, stagingQueue, csr, chunkTransferImageFunc, origin, region);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }
    return result;
}

/*
 * This method orchestrates transfer operation for images with given origin and region.
 * Transfer is split into chunks, each chunk represents sub-region to transfer.
 * Each chunk contains staging buffer which should be used instead of non-usm memory during transfers on GPU.
 * Several slices and rows can be packed into single chunk if size of such chunk does not exceeds maximum chunk size (2MB).
 * Caller provides actual function to enqueue read/write operation for single chunk.
 */
StagingTransferStatus StagingBufferManager::performImageTransfer(const void *ptr, const size_t *globalOrigin, const size_t *globalRegion, size_t rowPitch, size_t slicePitch, size_t bytesPerPixel, bool isMipMapped3DImage, ChunkTransferImageFunc &chunkTransferImageFunc, CommandStreamReceiver *csr, bool isRead) {
    StagingQueue stagingQueue;
    size_t origin[4] = {};
    size_t region[3] = {};
    origin[0] = globalOrigin[0];
    region[0] = globalRegion[0];
    if (isMipMapped3DImage) {
        origin[3] = globalOrigin[3];
    }

    StagingTransferStatus result{};
    size_t submittedChunks = 0;

    // Calculate number of rows that can be packed into single chunk.
    auto rowsPerChunk = std::max<size_t>(1ul, chunkSize / rowPitch);
    rowsPerChunk = std::min<size_t>(rowsPerChunk, globalRegion[1]);
    auto numOfChunksInYDim = globalRegion[1] / rowsPerChunk;

    // If single chunk is enough to transfer whole slice, we can try to pack several slices into single chunk.
    size_t slicesPerStep = 1;
    if (numOfChunksInYDim == 1) {
        slicesPerStep = std::max<size_t>(1ul, chunkSize / slicePitch);
        slicesPerStep = std::min<size_t>(slicesPerStep, globalRegion[2]);
    }
    auto remainderSlices = globalRegion[2] % slicesPerStep;

    ImageMetadata imageMetadata{bytesPerPixel, globalRegion[0] * bytesPerPixel, rowPitch, slicePitch};

    // Split (X, Y, Z) region into several (X, Y, Z') chunks.
    for (auto sliceId = 0u; sliceId < globalRegion[2] / slicesPerStep; sliceId++) {
        auto sliceOffset = sliceId * slicesPerStep;
        origin[2] = globalOrigin[2] + sliceOffset;
        region[2] = slicesPerStep;
        result = performImageSlicesTransfer(stagingQueue, submittedChunks, ptr, sliceOffset, globalOrigin[1], globalRegion[1], origin, region, imageMetadata, chunkTransferImageFunc, csr, isRead);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }

    if (remainderSlices != 0) {
        auto sliceOffset = globalRegion[2] - remainderSlices;
        origin[2] = globalOrigin[2] + sliceOffset;
        region[2] = remainderSlices;
        result = performImageSlicesTransfer(stagingQueue, submittedChunks, ptr, sliceOffset, globalOrigin[1], globalRegion[1], origin, region, imageMetadata, chunkTransferImageFunc, csr, isRead);
        if (result.chunkCopyStatus != 0 || result.waitStatus == WaitStatus::gpuHang) {
            return result;
        }
    }

    result.waitStatus = drainAndReleaseStagingQueue(isRead, stagingQueue, submittedChunks);
    return result;
}

StagingTransferStatus StagingBufferManager::performBufferTransfer(const void *ptr, size_t globalOffset, size_t globalSize, ChunkTransferBufferFunc &chunkTransferBufferFunc, CommandStreamReceiver *csr, bool isRead) {
    StagingQueue stagingQueue;
    auto copiesNum = globalSize / chunkSize;
    auto remainder = globalSize % chunkSize;
    auto chunkOffset = globalOffset;
    StagingTransferStatus result{};
    for (auto i = 0u; i < copiesNum; i++) {
        auto chunkPtr = ptrOffset(ptr, i * chunkSize);
        UserData userData{chunkPtr, chunkSize};
        result = performChunkTransfer(i, isRead, userData, stagingQueue, csr, chunkTransferBufferFunc, chunkOffset, chunkSize);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
        chunkOffset += chunkSize;
    }

    if (remainder != 0) {
        auto chunkPtr = ptrOffset(ptr, copiesNum * chunkSize);
        UserData userData{chunkPtr, remainder};
        result = performChunkTransfer(copiesNum, isRead, userData, stagingQueue, csr, chunkTransferBufferFunc, chunkOffset, remainder);
        if (result.chunkCopyStatus != 0) {
            return result;
        }
    }

    result.waitStatus = drainAndReleaseStagingQueue(isRead, stagingQueue, copiesNum + (remainder != 0 ? 1 : 0));
    return result;
}

void StagingBufferManager::copyImageToHost(void *dst, const void *stagingBuffer, size_t size, const ImageMetadata &imageData) const {
    auto sliceSize = imageData.rowSize * imageData.rowsInChunk;

    if (imageData.rowSize < imageData.rowPitch || (sliceSize < imageData.slicePitch && imageData.slicesInChunk > 1)) {
        for (auto sliceId = 0u; sliceId < imageData.slicesInChunk; sliceId++) {
            auto sliceOffset = sliceId * imageData.slicePitch;
            for (auto rowId = 0u; rowId < imageData.rowsInChunk; rowId++) {
                auto rowOffset = rowId * imageData.rowPitch;
                auto offset = sliceOffset + rowOffset;
                memcpy(ptrOffset(dst, offset), ptrOffset(stagingBuffer, offset), imageData.rowSize);
            }
        }
    } else {
        memcpy(dst, stagingBuffer, size);
    }
}

/*
 * This method is used for read transfers. It waits for transfer to finish
 * and copies data associated with that transfer to host allocation.
 * Returned tracker contains staging buffer ready for reuse.
 */
WaitStatus StagingBufferManager::copyStagingToHost(const std::pair<UserData, StagingBufferTracker> &transfer, StagingBufferTracker &tracker) const {
    auto status = transfer.second.csr->waitForTaskCount(transfer.second.taskCountToWait);
    if (status == WaitStatus::gpuHang) {
        return status;
    }
    transfer.second.csr->downloadAllocations(true);
    auto &userData = transfer.first;
    tracker = transfer.second;
    auto stagingBuffer = addrToPtr(tracker.chunkAddress);
    auto userDst = const_cast<void *>(userData.ptr);
    if (userData.isImageOperation) {
        copyImageToHost(userDst, stagingBuffer, userData.size, userData.imageMetadata);
        return WaitStatus::ready;
    }

    memcpy(userDst, stagingBuffer, userData.size);
    return WaitStatus::ready;
}

/*
 * Waits for all pending transfers to finish.
 * Releases staging buffers back to pool for reuse.
 */
WaitStatus StagingBufferManager::drainAndReleaseStagingQueue(bool isRead, const StagingQueue &stagingQueue, size_t numOfSubmittedTransfers) const {
    if (isRead) {
        StagingBufferTracker tracker{};
        for (auto i = 0u; i < std::min(numOfSubmittedTransfers, maxInFlightReads); i++) {
            auto status = copyStagingToHost(stagingQueue[i], tracker);
            if (status == WaitStatus::gpuHang) {
                return status;
            }
            tracker.freeChunk();
        }
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

    auto stagingBufferSize = alignUp(std::max(chunkSize, size), getDefaultStagingBufferSize());
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

bool StagingBufferManager::isValidForCopy(const Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId) {
    auto usmDstData = svmAllocsManager->getSVMAlloc(dstPtr);
    auto usmSrcData = svmAllocsManager->getSVMAlloc(srcPtr);
    bool hostToUsmCopy = usmSrcData == nullptr && usmDstData != nullptr;
    bool isUsedByOsContext = false;
    if (usmDstData) {
        isUsedByOsContext = usmDstData->gpuAllocations.getGraphicsAllocation(device.getRootDeviceIndex())->isUsedByOsContext(osContextId);
    }
    return hostToUsmCopy && (isUsedByOsContext || size <= chunkSize) && this->isValidForStaging(device, srcPtr, size, hasDependencies);
}

bool StagingBufferManager::isValidForStagingTransfer(const Device &device, const void *ptr, size_t size, bool hasDependencies) {
    auto nonUsmPtr = ptr != nullptr && svmAllocsManager->getSVMAlloc(ptr) == nullptr;
    return nonUsmPtr && this->isValidForStaging(device, ptr, size, hasDependencies);
}

// Common checks for usm, buffers and images
bool StagingBufferManager::isValidForStaging(const Device &device, const void *ptr, size_t size, bool hasDependencies) {
    auto stagingCopyEnabled = device.getProductHelper().isStagingBuffersEnabled();
    if (debugManager.flags.EnableCopyWithStagingBuffers.get() != -1) {
        stagingCopyEnabled = debugManager.flags.EnableCopyWithStagingBuffers.get();
    }
    auto osInterface = device.getRootDeviceEnvironment().osInterface.get();
    bool sizeWithinThreshold = osInterface ? osInterface->isSizeWithinThresholdForStaging(ptr, size) : true;
    return stagingCopyEnabled && !hasDependencies && sizeWithinThreshold && !this->registerHostPtr(ptr);
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

bool StagingBufferManager::registerHostPtr(const void *ptr) {
    auto lock = std::lock_guard<std::mutex>(mtx);
    auto isHostPtrDetected = detectedHostPtrs.find(ptr) != detectedHostPtrs.end();
    detectedHostPtrs.insert(ptr);
    return isHostPtrDetected;
}

void StagingBufferManager::resetDetectedPtrs() {
    auto lock = std::lock_guard<std::mutex>(mtx);
    detectedHostPtrs.clear();
}

void StagingBufferManager::freeAllocations() {
    auto lock = std::lock_guard<std::mutex>(mtx);
    for (auto &stagingBuffer : stagingBuffers) {
        svmAllocsManager->freeSVMAlloc(stagingBuffer.getBaseAddress());
    }
    stagingBuffers.clear();
    trackers.clear();
}

} // namespace NEO
