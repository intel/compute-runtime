/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NEO {

struct alignas(1) ImplicitArgsHeader {
    uint8_t structSize;
    uint8_t structVersion;
};

struct alignas(32) ImplicitArgsV0 {
    ImplicitArgsHeader header;
    uint8_t numWorkDim;
    uint8_t simdWidth;
    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;
    uint64_t globalSizeX;
    uint64_t globalSizeY;
    uint64_t globalSizeZ;
    uint64_t printfBufferPtr;
    uint64_t globalOffsetX;
    uint64_t globalOffsetY;
    uint64_t globalOffsetZ;
    uint64_t localIdTablePtr;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t padding0;
    uint64_t rtGlobalBufferPtr;
    uint64_t assertBufferPtr;
    uint8_t reserved[16];

    static constexpr uint8_t getSize() { return static_cast<uint8_t>((offsetof(ImplicitArgsV0, reserved))); }
    static constexpr uint8_t getAlignedSize() {
        return static_cast<uint8_t>(alignUp(sizeof(ImplicitArgsV0), 64));
    }
};

static_assert(ImplicitArgsV0::getSize() == (28 * sizeof(uint32_t)));

struct alignas(32) ImplicitArgsV1 {
    ImplicitArgsHeader header;
    uint8_t numWorkDim;
    uint8_t simdWidth;
    uint32_t localSizeX;
    uint32_t localSizeY;
    uint32_t localSizeZ;
    uint64_t globalSizeX;
    uint64_t globalSizeY;
    uint64_t globalSizeZ;
    uint64_t printfBufferPtr;
    uint64_t globalOffsetX;
    uint64_t globalOffsetY;
    uint64_t globalOffsetZ;
    uint64_t localIdTablePtr;
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t padding0;
    uint64_t rtGlobalBufferPtr;
    uint64_t assertBufferPtr;
    uint64_t scratchPtr;
    uint64_t syncBufferPtr;
    uint32_t enqueuedLocalSizeX;
    uint32_t enqueuedLocalSizeY;
    uint32_t enqueuedLocalSizeZ;

    static constexpr uint8_t getSize() { return static_cast<uint8_t>((offsetof(ImplicitArgsV1, enqueuedLocalSizeZ) + sizeof(ImplicitArgsV1::enqueuedLocalSizeZ))); }
    static constexpr uint8_t getAlignedSize() {
        return static_cast<uint8_t>(alignUp(sizeof(ImplicitArgsV1), 64));
    }
};

static_assert(ImplicitArgsV1::getSize() == (35 * sizeof(uint32_t)));

} // namespace NEO
