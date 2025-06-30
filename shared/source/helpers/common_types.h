/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/sku_info/sku_info_base.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace NEO {
class GraphicsAllocation;
struct EngineControl;
using EngineControlContainer = std::vector<EngineControl>;
using MultiDeviceEngineControlContainer = StackVec<EngineControlContainer, 6u>;
class Device;
using DeviceVector = std::vector<std::unique_ptr<Device>>;
using PrivateAllocsToReuseContainer = StackVec<std::pair<uint32_t, GraphicsAllocation *>, 8>;

// std::to_underlying is C++23 feature
template <typename EnumT>
constexpr auto toUnderlying(EnumT scopedEnumValue) {
    static_assert(std::is_enum_v<EnumT>);
    static_assert(!std::is_convertible_v<EnumT, std::underlying_type_t<EnumT>>);

    return static_cast<std::underlying_type_t<EnumT>>(scopedEnumValue);
}

template <typename EnumT>
    requires(std::is_enum_v<EnumT> && !std::is_convertible_v<EnumT, std::underlying_type_t<EnumT>>)
constexpr auto toEnum(std::underlying_type_t<EnumT> region) {
    return static_cast<EnumT>(region);
}

enum class DebugPauseState : uint32_t {
    disabled,
    waitingForFirstSemaphore,
    waitingForUserStartConfirmation,
    hasUserStartConfirmation,
    waitingForUserEndConfirmation,
    hasUserEndConfirmation,
    terminate
};

enum class ColouringPolicy : uint32_t {
    deviceCountBased,
    chunkSizeBased,
    mappingBased
};

class TagTypeBase {
};

enum class TagNodeType {
    timestampPacket,
    hwTimeStamps,
    hwPerfCounter,
    counter64b,
    fillPattern
};

enum class CacheRegion : uint16_t {
    defaultRegion = 0,
    region1,
    region2,
    region3,
    count,
    none = 0xFFFF
};
constexpr inline auto toCacheRegion(std::underlying_type_t<CacheRegion> region) {
    DEBUG_BREAK_IF(region >= toUnderlying(CacheRegion::count));
    return toEnum<CacheRegion>(region);
}

enum class CacheLevel : uint16_t {
    defaultLevel = 0,
    level2 = 2,
    level3 = 3
};
constexpr inline auto toCacheLevel(std::underlying_type_t<CacheRegion> level) {
    DEBUG_BREAK_IF(level == 1U);
    DEBUG_BREAK_IF(level > 3U);
    return toEnum<CacheLevel>(level);
}

enum class CachePolicy : uint32_t {
    uncached = 0,
    writeCombined = 1,
    writeThrough = 2,
    writeBack = 3,
};

enum class PreferredLocation : int16_t {
    clear = -1,
    system = 0,
    device = 1,
    none = 2,
    defaultLocation = device
};

enum class PostSyncMode : uint32_t {
    noWrite = 0,
    timestamp = 1,
    immediateData = 2,
};

enum class AtomicAccessMode : uint32_t {
    none = 1,
    host = 2,
    device = 3,
    system = 4,
    invalid = 5
};

enum class SynchronizedDispatchMode : uint32_t {
    disabled = 0,
    full = 1,
    limited = 2
};

enum class LocalMemAllocationMode : uint32_t {
    hwDefault = 0U,
    localOnly = 1U,
    localPreferred = 2U,
    count = 3U
};
constexpr inline auto toLocalMemAllocationMode(std::underlying_type_t<LocalMemAllocationMode> modeFlag) {
    DEBUG_BREAK_IF(modeFlag >= toUnderlying(LocalMemAllocationMode::count));
    return toEnum<LocalMemAllocationMode>(modeFlag);
}

namespace InterruptId {
static constexpr uint32_t notUsed = std::numeric_limits<uint32_t>::max();
}

namespace TypeTraits {
template <typename T>
constexpr bool isPodV = std::is_standard_layout_v<T> && std::is_trivial_v<T> && std::is_trivially_copyable_v<T>;
} // namespace TypeTraits

struct BcsSplitSettings {
    BcsInfoMask allEngines = {};
    BcsInfoMask h2dEngines = {};
    BcsInfoMask d2hEngines = {};
    uint32_t minRequiredTotalCsrCount = 0;
    uint32_t requiredTileCount = 0;
    bool enabled = false;
};
} // namespace NEO
