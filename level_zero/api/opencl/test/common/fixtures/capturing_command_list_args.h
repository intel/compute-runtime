/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/zex_cmdlist.h"
#include <level_zero/ze_api.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

using EventHandles = std::vector<ze_event_handle_t>;

// Level Zero APIs often take a pointer as an argument. These objects are already destroyed by the
// time we leave the LEO API call, so we need to deep copy them if we want to compare them in ULTS.
inline EventHandles copyWaitEvents(uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if ((phWaitEvents == nullptr) || (numWaitEvents == 0u)) {
        return {};
    }
    return {phWaitEvents, phWaitEvents + numWaitEvents};
}

inline std::vector<uint8_t> copyPattern(const void *pattern, size_t patternSize) {
    if ((pattern == nullptr) || (patternSize == 0u)) {
        return {};
    }
    auto bytes = reinterpret_cast<const uint8_t *>(pattern);
    return {bytes, bytes + patternSize};
}

template <typename T>
std::optional<T> copyValue(const T *value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    return *value;
}

struct AppendMemoryCopyArgs {
    void *dstptr;
    const void *srcptr;
    size_t size;
    ze_event_handle_t signalEvent;
    EventHandles waitEvents;

    AppendMemoryCopyArgs(void *dstptr, const void *srcptr, size_t size, ze_event_handle_t signalEvent,
                         uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents)
        : dstptr(dstptr), srcptr(srcptr), size(size), signalEvent(signalEvent),
          waitEvents(copyWaitEvents(numWaitEvents, phWaitEvents)) {}
};

struct AppendMemoryCopyRegionArgs {
    void *dstptr;
    std::optional<ze_copy_region_t> dstRegion;
    uint32_t dstPitch;
    uint32_t dstSlicePitch;
    const void *srcptr;
    std::optional<ze_copy_region_t> srcRegion;
    uint32_t srcPitch;
    uint32_t srcSlicePitch;
    ze_event_handle_t signalEvent;
    EventHandles waitEvents;

    AppendMemoryCopyRegionArgs(void *dstptr, const ze_copy_region_t *dstRegion, uint32_t dstPitch, uint32_t dstSlicePitch,
                               const void *srcptr, const ze_copy_region_t *srcRegion, uint32_t srcPitch, uint32_t srcSlicePitch,
                               ze_event_handle_t signalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents)
        : dstptr(dstptr), dstRegion(copyValue(dstRegion)), dstPitch(dstPitch), dstSlicePitch(dstSlicePitch),
          srcptr(srcptr), srcRegion(copyValue(srcRegion)), srcPitch(srcPitch), srcSlicePitch(srcSlicePitch),
          signalEvent(signalEvent), waitEvents(copyWaitEvents(numWaitEvents, phWaitEvents)) {}
};

struct AppendMemoryFillArgs {
    void *ptr;
    std::vector<uint8_t> pattern;
    size_t patternSize;
    size_t size;
    ze_event_handle_t signalEvent;
    EventHandles waitEvents;

    AppendMemoryFillArgs(void *ptr, const void *pattern, size_t patternSize, size_t size,
                         ze_event_handle_t signalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents)
        : ptr(ptr), pattern(copyPattern(pattern, patternSize)), patternSize(patternSize), size(size),
          signalEvent(signalEvent), waitEvents(copyWaitEvents(numWaitEvents, phWaitEvents)) {}
};

struct AppendBarrierArgs {
    ze_event_handle_t signalEvent;
    EventHandles waitEvents;
    bool relaxedOrderingDispatch;

    AppendBarrierArgs(ze_event_handle_t signalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                      bool relaxedOrderingDispatch)
        : signalEvent(signalEvent), waitEvents(copyWaitEvents(numWaitEvents, phWaitEvents)),
          relaxedOrderingDispatch(relaxedOrderingDispatch) {}
};

struct HostSynchronizeArgs {
    uint64_t timeout = 0u;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
