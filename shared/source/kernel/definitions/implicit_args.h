/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NEO {

struct alignas(32) ImplicitArgs {
    uint8_t structSize;
    uint8_t structVersion;
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

    static constexpr uint8_t getSize() { return static_cast<uint8_t>((offsetof(ImplicitArgs, reserved))); }
};

static_assert(std::alignment_of_v<ImplicitArgs> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgs) == (32 * sizeof(uint32_t)));
static_assert(ImplicitArgs::getSize() == (28 * sizeof(uint32_t)));
static_assert(std::is_pod<ImplicitArgs>::value);

} // namespace NEO
