/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#include <immintrin.h>

namespace NEO {

#if __AVX2__
struct uint16x16_t {
    enum { numChannels = 16 };

    __m256i value;

    uint16x16_t() {
        value = _mm256_setzero_si256();
    }

    uint16x16_t(__m256i value) : value(value) {
    }

    uint16x16_t(uint16_t a) {
        value = _mm256_set1_epi16(a); //AVX
    }

    explicit uint16x16_t(const void *alignedPtr) {
        load(alignedPtr);
    }

    inline uint16_t get(unsigned int element) {
        DEBUG_BREAK_IF(element >= numChannels);
        return reinterpret_cast<uint16_t *>(&value)[element];
    }

    static inline uint16x16_t zero() {
        return uint16x16_t(static_cast<uint16_t>(0u));
    }

    static inline uint16x16_t one() {
        return uint16x16_t(static_cast<uint16_t>(1u));
    }

    static inline uint16x16_t mask() {
        return uint16x16_t(static_cast<uint16_t>(0xffffu));
    }

    inline void load(const void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<32>(alignedPtr));
        value = _mm256_load_si256(reinterpret_cast<const __m256i *>(alignedPtr)); //AVX
    }

    inline void loadUnaligned(const void *ptr) {
        value = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(ptr)); //AVX
    }

    inline void store(void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<32>(alignedPtr));
        _mm256_store_si256(reinterpret_cast<__m256i *>(alignedPtr), value); //AVX
    }

    inline void storeUnaligned(void *ptr) {
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(ptr), value); //AVX
    }

    inline operator bool() const {
        return _mm256_testz_si256(value, mask().value) ? false : true; //AVX
    }

    inline uint16x16_t &operator-=(const uint16x16_t &a) {
        value = _mm256_sub_epi16(value, a.value); //AVX2
        return *this;
    }

    inline uint16x16_t &operator+=(const uint16x16_t &a) {
        value = _mm256_add_epi16(value, a.value); //AVX2
        return *this;
    }

    inline friend uint16x16_t operator>=(const uint16x16_t &a, const uint16x16_t &b) {
        uint16x16_t result;
        result.value =
            _mm256_xor_si256(mask().value,
                             _mm256_cmpgt_epi16(b.value, a.value)); //AVX2
        return result;
    }

    inline friend uint16x16_t operator&&(const uint16x16_t &a, const uint16x16_t &b) {
        uint16x16_t result;
        result.value = _mm256_and_si256(a.value, b.value); //AVX2
        return result;
    }

    // NOTE: uint16x16_t::blend behaves like mask ? a : b
    inline friend uint16x16_t blend(const uint16x16_t &a, const uint16x16_t &b, const uint16x16_t &mask) {
        uint16x16_t result;

        // Have to swap arguments to get intended calling semantics
        result.value =
            _mm256_blendv_epi8(b.value, a.value, mask.value); //AVX2
        return result;
    }
};
#endif // __AVX2__
} // namespace NEO
