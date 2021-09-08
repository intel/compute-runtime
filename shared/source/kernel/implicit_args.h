/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace NEO {

struct ImplicitArgs {
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
};
static_assert((sizeof(ImplicitArgs) & 31) == 0, "Implicit args size need to be aligned to 32");
static_assert(std::is_pod<ImplicitArgs>::value);

constexpr const char *implicitArgsRelocationSymbolName = "INTEL_PATCH_CROSS_THREAD_OFFSET_OFF_R0";
} // namespace NEO
