/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO::Zebin::ZeInfo::Types::L1CachePolicy {
enum L1CachePolicy : uint8_t {
    L1CachePolicyUnknown = 0,
    L1CachePolicyWriteBypass = 1,
    L1CachePolicyUncached,
    L1CachePolicyWriteBack,
    L1CachePolicyWriteThrough,
    L1CachePolicyWriteStreaming,
    L1CachePolicyMax
};

namespace Defaults {
inline constexpr L1CachePolicy l1CachePolicy = L1CachePolicyUnknown;
} // namespace Defaults
} // namespace NEO::Zebin::ZeInfo::Types::L1CachePolicy
