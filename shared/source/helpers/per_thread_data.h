/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/utilities/arrayref.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class LinearStream;

struct PerThreadDataHelper {
    static inline size_t getPerThreadDataSizeTotal(
        uint32_t simd,
        uint32_t grfSize,
        uint32_t numChannels,
        size_t localWorkSize) {
        return getThreadsPerWG(simd, localWorkSize) * getPerThreadSizeLocalIDs(simd, grfSize, numChannels);
    }
}; // namespace PerThreadDataHelper
} // namespace NEO
