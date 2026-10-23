/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_allocation_cache.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_reuse.h"
#include "shared/source/memory_manager/unified_memory_reuse_cleaner.h"

namespace L0 {

bool ImageAllocationCacheKey::matches(const ImageAllocationCacheKey &other) const {
    const auto &desc = this->imgInfo.imgDesc;
    const auto &otherDesc = other.imgInfo.imgDesc;
    return this->rootDeviceIndex == other.rootDeviceIndex &&
           this->deviceBitfield == other.deviceBitfield &&
           this->allocationType == other.allocationType &&
           this->preferCompressed == other.preferCompressed &&
           desc.imageType == otherDesc.imageType &&
           desc.imageWidth == otherDesc.imageWidth &&
           desc.imageHeight == otherDesc.imageHeight &&
           desc.imageDepth == otherDesc.imageDepth &&
           desc.imageArraySize == otherDesc.imageArraySize &&
           desc.imageRowPitch == otherDesc.imageRowPitch &&
           desc.imageSlicePitch == otherDesc.imageSlicePitch &&
           desc.numMipLevels == otherDesc.numMipLevels &&
           desc.numSamples == otherDesc.numSamples &&
           desc.fromParent == otherDesc.fromParent &&
           this->imgInfo.surfaceFormat == other.imgInfo.surfaceFormat &&
           this->imgInfo.plane == other.imgInfo.plane &&
           this->imgInfo.mipCount == other.imgInfo.mipCount &&
           this->imgInfo.linearStorage == other.imgInfo.linearStorage &&
           this->imgInfo.useLocalMemory == other.imgInfo.useLocalMemory &&
           this->imgInfo.isDisplayable == other.imgInfo.isDisplayable &&
           this->imgInfo.forceTiling == other.imgInfo.forceTiling;
}

std::unique_ptr<ImageAllocationCache> ImageAllocationCache::create(NEO::Device &device) {
    if (NEO::debugManager.flags.ExperimentalEnableImageAllocationCache.get() != 1 ||
        device.usmReuseInfo.getMaxAllocationsSavedForReuseSize() == 0u) {
        return nullptr;
    }

    auto executionEnvironment = device.getExecutionEnvironment();
    executionEnvironment->initializeUnifiedMemoryReuseCleaner(device.isAnyDirectSubmissionLightEnabled());
    auto reuseCleaner = executionEnvironment->unifiedMemoryReuseCleaner.get();

    auto cache = std::make_unique<ImageAllocationCache>(device, reuseCleaner);
    if (reuseCleaner) {
        reuseCleaner->registerAllocationCache(cache.get());
    }
    return cache;
}

ImageAllocationCache::ImageAllocationCache(NEO::Device &device, NEO::UnifiedMemoryReuseCleaner *reuseCleaner)
    : memoryManager(device.getMemoryManager()), usmReuseInfo(device.usmReuseInfo), reuseCleaner(reuseCleaner), device(device) {
}

ImageAllocationCache::~ImageAllocationCache() {
    this->cleanup();
}

bool ImageAllocationCache::insert(const ImageAllocationCacheKey &key, const NEO::ImageInfo &imgInfo, NEO::GraphicsAllocation *allocation) {
    if (allocation == nullptr) {
        return false;
    }
    if (this->device.shouldLimitAllocationsReuse()) {
        return false;
    }
    const auto allocationSize = allocation->getUnderlyingBufferSize();
    std::unique_lock<std::mutex> lock(this->mtx);
    if (!this->acceptsAllocations) {
        return false;
    }
    {
        auto reuseLock = this->usmReuseInfo.obtainAllocationsReuseLock();
        if (allocationSize + this->usmReuseInfo.getAllocationsSavedForReuseSize() > this->usmReuseInfo.getMaxAllocationsSavedForReuseSize()) {
            return false;
        }
        this->usmReuseInfo.recordAllocationSaveForReuse(allocationSize);
    }
    this->allocations.push_back({key, imgInfo, allocation, std::chrono::high_resolution_clock::now()});
    this->empty = false;
    lock.unlock();

    if (this->reuseCleaner) {
        this->reuseCleaner->startThread();
        this->reuseCleaner->notifyAllocationCacheUpdate();
    }
    return true;
}

NEO::GraphicsAllocation *ImageAllocationCache::get(const ImageAllocationCacheKey &key, NEO::ImageInfo &imgInfo) {
    if (this->empty) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto it = this->allocations.begin(); it != this->allocations.end(); ++it) {
        if (!it->key.matches(key) || this->memoryManager->allocInUse(*it->allocation)) {
            continue;
        }
        auto allocation = it->allocation;
        {
            auto reuseLock = this->usmReuseInfo.obtainAllocationsReuseLock();
            this->usmReuseInfo.recordAllocationGetFromReuse(allocation->getUnderlyingBufferSize());
        }
        this->releaseBindlessSlot(*allocation);
        imgInfo = it->imgInfo;
        this->allocations.erase(it);
        this->empty = this->allocations.empty();
        return allocation;
    }
    return nullptr;
}

void ImageAllocationCache::releaseBindlessSlot(NEO::GraphicsAllocation &allocation) {
    const auto &bindlessInfo = allocation.getBindlessInfo();
    if (bindlessInfo.heapAllocation == nullptr) {
        return;
    }
    auto bindlessHeapsHelper = this->memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getBindlessHeapsHelper();
    if (bindlessHeapsHelper) {
        bindlessHeapsHelper->releaseSSToReusePool(bindlessInfo);
    }
    allocation.setBindlessInfo({});
}

void ImageAllocationCache::freeCachedAllocation(const CachedAllocation &cachedAllocation) {
    {
        auto reuseLock = this->usmReuseInfo.obtainAllocationsReuseLock();
        this->usmReuseInfo.recordAllocationGetFromReuse(cachedAllocation.allocation->getUnderlyingBufferSize());
    }
    this->memoryManager->freeGraphicsMemory(cachedAllocation.allocation);
}

void ImageAllocationCache::trim() {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (const auto &cachedAllocation : this->allocations) {
        this->freeCachedAllocation(cachedAllocation);
    }
    this->allocations.clear();
    this->empty = true;
}

void ImageAllocationCache::cleanup() {
    {
        std::lock_guard<std::mutex> lock(this->mtx);
        if (!this->acceptsAllocations) {
            return;
        }
        this->acceptsAllocations = false;
    }
    if (this->reuseCleaner) {
        this->reuseCleaner->unregisterAllocationCache(this);
    }
    this->trim();
}

void ImageAllocationCache::trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll) {
    std::lock_guard<std::mutex> lock(this->mtx);
    for (auto it = this->allocations.begin(); it != this->allocations.end();) {
        if (it->saveTime > trimTimePoint) {
            break;
        }
        if (this->memoryManager->allocInUse(*it->allocation)) {
            ++it;
            continue;
        }
        this->freeCachedAllocation(*it);
        it = this->allocations.erase(it);
        if (!trimAll) {
            break;
        }
    }
    this->empty = this->allocations.empty();
}

} // namespace L0
