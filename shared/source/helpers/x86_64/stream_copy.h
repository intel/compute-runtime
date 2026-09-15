/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include <cstddef>

namespace NEO {
inline constexpr size_t streamCopyAvx512Width = 64;
inline constexpr size_t streamCopyAvx2Width = 32;
inline constexpr size_t streamCopySseWidth = 16;

void streamCopyFromWriteCombinedSse(void *dst, const void *src, size_t bytes) noexcept;
void streamCopyFromWriteCombinedAvx2(void *dst, const void *src, size_t bytes) noexcept;
void streamCopyFromWriteCombinedAvx512(void *dst, const void *src, size_t bytes) noexcept;

template <bool emitSfenceAfterCopy = true>
inline void streamCopy(void *dst, const void *src, size_t bytes) noexcept {
    const auto &cpuInfo = CpuInfo::getInstance();
    if (bytes >= streamCopyAvx512Width && cpuInfo.isFeatureSupported(CpuInfo::featureAvX512)) {
        streamCopyFromWriteCombinedAvx512(dst, src, bytes);
    } else if (bytes >= streamCopyAvx2Width && cpuInfo.isFeatureSupported(CpuInfo::featureAvX2)) {
        streamCopyFromWriteCombinedAvx2(dst, src, bytes);
    } else if (bytes >= streamCopySseWidth && cpuInfo.isFeatureSupported(CpuInfo::featureSse41)) {
        streamCopyFromWriteCombinedSse(dst, src, bytes);
    } else {
        memcpy_s(dst, bytes, src, bytes);
    }

    if constexpr (emitSfenceAfterCopy) {
        CpuIntrinsics::sfence();
    }
}
} // namespace NEO
