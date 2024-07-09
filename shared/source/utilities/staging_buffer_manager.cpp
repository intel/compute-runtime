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
#include "shared/source/memory_manager/unified_memory_manager.h"

namespace NEO {

StagingBufferManager::StagingBufferManager(SVMAllocsManager *svmAllocsManager, const RootDeviceIndicesContainer &rootDeviceIndices, const std::map<uint32_t, DeviceBitfield> &deviceBitfields) : svmAllocsManager(svmAllocsManager), rootDeviceIndices(rootDeviceIndices), deviceBitfields(deviceBitfields) {
    if (debugManager.flags.StagingBufferSize.get() != -1) {
        chunkSize = debugManager.flags.StagingBufferSize.get() * MemoryConstants::kiloByte;
    }
}

StagingBufferManager::~StagingBufferManager() {
    for (auto &stagingBuffer : stagingBuffers) {
        svmAllocsManager->freeSVMAlloc(stagingBuffer.first->gpuAllocations.getDefaultGraphicsAllocation()->getUnderlyingBuffer());
    }
}

/*
 * This method performs 4 steps for single chunk copy
 * 1. Get existing staging buffer, if can't - allocate new one,
 * 2. Perform actual copy,
 * 3. Store used buffer back to the container (with current task count)
 * 4. Update tag to reuse previous buffers within same API call
 */
int32_t StagingBufferManager::performChunkCopy(void *chunkDst, const void *chunkSrc, size_t size, ChunkCopyFunction chunkCopyFunc, CommandStreamReceiver *csr) {
    auto rootDeviceIndex = csr->getRootDeviceIndex();
    auto taskCount = *csr->getTagAddress();
    auto stagingBuffer = getExistingBuffer(taskCount, rootDeviceIndex);
    if (stagingBuffer == nullptr) {
        stagingBuffer = allocateStagingBuffer();
    }
    auto ret = chunkCopyFunc(chunkDst, stagingBuffer, chunkSrc, size);
    storeBuffer(stagingBuffer, csr->peekTaskCount());
    csr->flushTagUpdate();
    return ret;
}

/*
 * This method copies data between non-USM and USM allocations by splitting transfers into chunks.
 * Each chunk copy contains staging buffer which should be used instead of non-usm memory during transfers on GPU.
 * Caller provides actual function to transfer data for single chunk.
 */
int32_t StagingBufferManager::performCopy(void *dstPtr, const void *srcPtr, size_t size, ChunkCopyFunction chunkCopyFunc, CommandStreamReceiver *csr) {
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
 * This method will try to return existing staging buffer from the container.
 * It's checking only "oldest" allocation.
 * Returns nullptr if no staging buffer available.
 */
void *StagingBufferManager::getExistingBuffer(uint64_t taskCount, uint32_t rootDeviceIndex) {
    auto lock = std::lock_guard<std::mutex>(mtx);
    if (stagingBuffers.empty()) {
        return nullptr;
    }
    void *buffer = nullptr;
    auto iterator = stagingBuffers.begin();
    UNRECOVERABLE_IF(iterator == stagingBuffers.end());

    if (taskCount > iterator->second) {
        auto allocation = iterator->first->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
        buffer = allocation->getUnderlyingBuffer();
        stagingBuffers.erase(iterator);
    }
    return buffer;
}

void *StagingBufferManager::allocateStagingBuffer() {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 0u, rootDeviceIndices, deviceBitfields);
    auto hostPtr = svmAllocsManager->createHostUnifiedMemoryAllocation(chunkSize, unifiedMemoryProperties);
    return hostPtr;
}

void StagingBufferManager::storeBuffer(void *stagingBuffer, uint64_t taskCount) {
    auto lock = std::lock_guard<std::mutex>(mtx);
    auto svmData = svmAllocsManager->getSVMAlloc(stagingBuffer);
    stagingBuffers.push_back({svmData, taskCount});
}

bool StagingBufferManager::isValidForCopy(Device &device, void *dstPtr, const void *srcPtr, size_t size, bool hasDependencies, uint32_t osContextId) const {
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

} // namespace NEO
