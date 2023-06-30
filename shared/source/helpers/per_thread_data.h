/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_id_gen.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class LinearStream;

struct PerThreadDataHelper {
    static inline size_t getPerThreadDataSizeTotal(
        uint32_t simd,
        uint32_t grfSize,
        uint32_t numChannels,
        size_t localWorkSize,
        bool isHwLocalIdGeneration,
        const GfxCoreHelper &gfxCoreHelper) {
        return gfxCoreHelper.calculateNumThreadsPerThreadGroup(simd, static_cast<uint32_t>(localWorkSize), grfSize, isHwLocalIdGeneration) * getPerThreadSizeLocalIDs(simd, grfSize, numChannels);
    }
}; // namespace PerThreadDataHelper
} // namespace NEO
