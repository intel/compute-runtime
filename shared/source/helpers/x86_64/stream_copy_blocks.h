/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/x86_64/stream_copy.h"

#include <cstddef>
#include <immintrin.h>

namespace NEO {

#if defined(__SSE4_1__) || defined(_MSC_VER)
struct StreamBlockSse {
    static constexpr size_t width = streamCopySseWidth;

    static inline __m128i streamLoad(const void *alignedSrc) {
        return _mm_stream_load_si128(reinterpret_cast<__m128i *>(const_cast<void *>(alignedSrc)));
    }

    static inline void storeUnaligned(void *dst, __m128i value) {
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst), value);
    }

    static inline void streamStore(void *alignedDst, __m128i value) {
        _mm_stream_si128(reinterpret_cast<__m128i *>(alignedDst), value);
    }
};
#endif // defined(__SSE4_1__) || defined(_MSC_VER)

#if defined(__AVX2__) || defined(_MSC_VER)
struct StreamBlockAvx2 {
    static constexpr size_t width = streamCopyAvx2Width;

    static inline __m256i streamLoad(const void *alignedSrc) {
        return _mm256_stream_load_si256(reinterpret_cast<const __m256i *>(alignedSrc));
    }

    static inline void storeUnaligned(void *dst, __m256i value) {
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst), value);
    }

    static inline void streamStore(void *alignedDst, __m256i value) {
        _mm256_stream_si256(reinterpret_cast<__m256i *>(alignedDst), value);
    }
};
#endif // defined(__AVX2__) || defined(_MSC_VER)

#if defined(__AVX512F__) || defined(_MSC_VER)
struct StreamBlockAvx512 {
    static constexpr size_t width = streamCopyAvx512Width;

    static inline __m512i streamLoad(const void *alignedSrc) {
        return _mm512_stream_load_si512(static_cast<__m512i *>(const_cast<void *>(alignedSrc)));
    }

    static inline void storeUnaligned(void *dst, __m512i value) {
        _mm512_storeu_si512(dst, value);
    }

    static inline void streamStore(void *alignedDst, __m512i value) {
        _mm512_stream_si512(reinterpret_cast<__m512i *>(alignedDst), value);
    }
};
#endif // defined(__AVX512F__) || defined(_MSC_VER)

} // namespace NEO
