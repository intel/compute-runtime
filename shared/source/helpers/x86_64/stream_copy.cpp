/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/x86_64/stream_copy.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/cpu_info.h"

namespace NEO {

template <size_t width>
bool areBuffersAlignedTo(const void *dst, const void *src) {
    return isAligned<width>(dst) && isAligned<width>(src);
}

void streamCopy(void *dst, const void *src, size_t bytes) noexcept {
    const auto &cpuInfo = CpuInfo::getInstance();

    if (cpuInfo.isFeatureSupported(CpuInfo::featureAvX512) && areBuffersAlignedTo<streamCopyAvx512Width>(dst, src)) {
        streamCopyImpl<true>(dst, src, bytes);
        return;
    }
    if (cpuInfo.isFeatureSupported(CpuInfo::featureAvX2) && areBuffersAlignedTo<streamCopyAvx2Width>(dst, src)) {
        streamCopyImpl<false>(dst, src, bytes);
        return;
    }

    memcpy_s(dst, bytes, src, bytes);
}

} // namespace NEO
