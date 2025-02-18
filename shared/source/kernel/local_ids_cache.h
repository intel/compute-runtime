/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <mutex>

namespace NEO {
struct RootDeviceEnvironment;
class LocalIdsCache : NEO::NonCopyableAndNonMovableClass {
  public:
    struct LocalIdsCacheEntry {
        Vec3<uint16_t> groupSize = {0, 0, 0};
        uint8_t *localIdsData = nullptr;
        size_t localIdsSize = 0U;
        size_t localIdsSizeAllocated = 0U;
        size_t accessCounter = 0;
    };

    LocalIdsCache() = delete;

    LocalIdsCache(size_t cacheSize, std::array<uint8_t, 3> wgDimOrder, uint32_t grfCount, uint8_t simdSize, uint8_t grfSize, bool usesOnlyImages = false);
    ~LocalIdsCache();

    void setLocalIdsForGroup(const Vec3<uint16_t> &group, void *destination, const RootDeviceEnvironment &rootDeviceEnvironment);
    size_t getLocalIdsSizeForGroup(const Vec3<uint16_t> &group, const RootDeviceEnvironment &rootDeviceEnvironment) const;
    size_t getLocalIdsSizePerThread() const;

  protected:
    void setLocalIdsForEntry(LocalIdsCacheEntry &entry, void *destination);
    void commitNewEntry(LocalIdsCacheEntry &entry, const Vec3<uint16_t> &group, const RootDeviceEnvironment &rootDeviceEnvironment);
    std::unique_lock<std::mutex> lock();

    StackVec<LocalIdsCacheEntry, 4> cache;
    std::mutex setLocalIdsMutex;
    const std::array<uint8_t, 3> wgDimOrder;
    const uint32_t localIdsSizePerThread;
    const uint32_t grfCount;
    const uint8_t grfSize;
    const uint8_t simdSize;
    const bool usesOnlyImages;
};

static_assert(NEO::NonCopyableAndNonMovable<LocalIdsCache>);

} // namespace NEO
