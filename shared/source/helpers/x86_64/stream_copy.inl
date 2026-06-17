/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/helpers/x86_64/stream_copy.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace NEO {

template <bool withAvx512>
void streamCopyImpl(void *dst, const void *src, size_t bytes) noexcept {
    constexpr size_t minBytesRequiringFence = streamCopySseWidth;

    auto *dstBytes = static_cast<uint8_t *>(dst);
    auto *srcBytes = static_cast<const uint8_t *>(src);
    size_t remainingBytes = bytes;

#if defined(__AVX512F__)
    if constexpr (withAvx512) {
        while (remainingBytes >= streamCopyAvx512Width) {
            _mm512_stream_si512(reinterpret_cast<__m512i *>(dstBytes),
                                _mm512_stream_load_si512(reinterpret_cast<__m512i *>(const_cast<uint8_t *>(srcBytes))));
            dstBytes += streamCopyAvx512Width;
            srcBytes += streamCopyAvx512Width;
            remainingBytes -= streamCopyAvx512Width;
        }
    }
#endif

    while (remainingBytes >= streamCopyAvx2Width) {
        _mm256_stream_si256(reinterpret_cast<__m256i *>(dstBytes),
                            _mm256_stream_load_si256(reinterpret_cast<const __m256i *>(srcBytes)));
        dstBytes += streamCopyAvx2Width;
        srcBytes += streamCopyAvx2Width;
        remainingBytes -= streamCopyAvx2Width;
    }

    if (remainingBytes >= streamCopySseWidth) {
        _mm_stream_si128(reinterpret_cast<__m128i *>(dstBytes),
                         _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<uint8_t *>(srcBytes))));
        dstBytes += streamCopySseWidth;
        srcBytes += streamCopySseWidth;
        remainingBytes -= streamCopySseWidth;
    }

    if (remainingBytes & 8) {
        std::memcpy(dstBytes, srcBytes, 8);
        dstBytes += 8;
        srcBytes += 8;
    }
    if (remainingBytes & 4) {
        std::memcpy(dstBytes, srcBytes, 4);
        dstBytes += 4;
        srcBytes += 4;
    }
    if (remainingBytes & 2) {
        std::memcpy(dstBytes, srcBytes, 2);
        dstBytes += 2;
        srcBytes += 2;
    }
    if (remainingBytes & 1) {
        *dstBytes = *srcBytes;
    }

    if (bytes >= minBytesRequiringFence) {
        _mm_sfence();
    }
}

} // namespace NEO
