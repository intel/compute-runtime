/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace L0::MCL {

template <typename T>
static T undefined = std::numeric_limits<T>::max();

template <typename T>
constexpr bool isDefined(T value) {
    return value != undefined<T>;
}

template <typename T>
constexpr bool isUndefined(T value) {
    return value == undefined<T>;
}

using OffsetT = size_t;
using CommandBufferOffset = OffsetT;
using SurfaceStateHeapOffset = OffsetT;
using IndirectObjectHeapOffset = OffsetT;
using InstructionsOffset = OffsetT;

using AddressT = uint64_t;
using GpuAddress = uint64_t;
using CpuAddress = uintptr_t;

using LocalOffsetT = uint16_t;
using CrossThreadDataOffset = LocalOffsetT;
using LocalSshOffset = LocalOffsetT;

using SlmOffset = uint32_t;

namespace Constants {
static constexpr AddressT dirtyAddress = 0xDEADBEEFDEADBEEF;
static constexpr uint32_t maxNumChannels = 3;
} // namespace Constants

using MaxChannelsArray = std::array<uint32_t, Constants::maxNumChannels>;
using MaxChannelsCArray = uint32_t[Constants::maxNumChannels];

} // namespace L0::MCL
