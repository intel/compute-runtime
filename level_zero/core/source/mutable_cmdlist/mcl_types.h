/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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

static constexpr AddressT dirtyAddress = 0xDEADBEEFDEADBEEF;

} // namespace L0::MCL
