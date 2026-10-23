/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_reuse_cache.h"
#include "shared/source/memory_manager/allocation_type.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace NEO {
class Device;
class GraphicsAllocation;
class UnifiedMemoryReuseCleaner;
struct UsmReuseInfo;
} // namespace NEO

namespace L0 {

struct ImageAllocationCacheKey {
    NEO::ImageInfo imgInfo = {};
    NEO::DeviceBitfield deviceBitfield = {};
    NEO::AllocationType allocationType = {};
    uint32_t rootDeviceIndex = 0u;
    bool preferCompressed = false;

    bool matches(const ImageAllocationCacheKey &other) const;
};

class ImageAllocationCache : public NEO::AllocationReuseCache {
  public:
    struct CachedAllocation {
        ImageAllocationCacheKey key;
        NEO::ImageInfo imgInfo;
        NEO::GraphicsAllocation *allocation;
        std::chrono::high_resolution_clock::time_point saveTime;
    };

    static std::unique_ptr<ImageAllocationCache> create(NEO::Device &device);

    ImageAllocationCache(NEO::Device &device, NEO::UnifiedMemoryReuseCleaner *reuseCleaner);
    ~ImageAllocationCache() override;

    bool insert(const ImageAllocationCacheKey &key, const NEO::ImageInfo &imgInfo, NEO::GraphicsAllocation *allocation);
    NEO::GraphicsAllocation *get(const ImageAllocationCacheKey &key, NEO::ImageInfo &imgInfo);
    void trim();
    void cleanup();

    bool isEmpty() override { return this->empty; }
    void trimOldAllocs(std::chrono::high_resolution_clock::time_point trimTimePoint, bool trimAll) override;
    NEO::MemoryManager *getMemoryManager() const override { return this->memoryManager; }

  protected:
    void releaseBindlessSlot(NEO::GraphicsAllocation &allocation);
    void freeCachedAllocation(const CachedAllocation &cachedAllocation);

    std::vector<CachedAllocation> allocations;
    std::mutex mtx;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::UsmReuseInfo &usmReuseInfo;
    NEO::UnifiedMemoryReuseCleaner *reuseCleaner = nullptr;
    NEO::Device &device;
    std::atomic_bool empty = true;
    bool acceptsAllocations = true;
};

} // namespace L0
