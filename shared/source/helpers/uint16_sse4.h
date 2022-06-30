/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"

#include <cstdint>
#if defined(__ARM_ARCH)
#include <sse2neon.h>
#else
#include <immintrin.h>
#endif

namespace NEO {

struct uint16x8_t {
    enum { numChannels = 8 };

    __m128i value;

    uint16x8_t() {
        value = _mm_setzero_si128();
    }

    uint16x8_t(__m128i value) : value(value) {
    }

    uint16x8_t(uint16_t a) {
        value = _mm_set1_epi16(a); //SSE2
    }

    explicit uint16x8_t(const void *alignedPtr) {
        load(alignedPtr);
    }

    inline uint16_t get(unsigned int element) {
        DEBUG_BREAK_IF(element >= numChannels);
        return reinterpret_cast<uint16_t *>(&value)[element];
    }

    static inline uint16x8_t zero() {
        return uint16x8_t(static_cast<uint16_t>(0u));
    }

    static inline uint16x8_t one() {
        return uint16x8_t(static_cast<uint16_t>(1u));
    }

    static inline uint16x8_t mask() {
        return uint16x8_t(static_cast<uint16_t>(0xffffu));
    }

    inline void load(const void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<16>(alignedPtr));
        value = _mm_load_si128(reinterpret_cast<const __m128i *>(alignedPtr)); //SSE2
    }

    inline void loadUnaligned(const void *ptr) {
        value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr)); //SSE2
    }

    inline void store(void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<16>(alignedPtr));
        _mm_store_si128(reinterpret_cast<__m128i *>(alignedPtr), value); //SSE2
    }

    inline void storeUnaligned(void *ptr) {
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), value); //SSE2
    }

    inline operator bool() const {
        return _mm_test_all_zeros(value, mask().value) ? false : true; //SSE4.1 alternatives?
    }

    inline uint16x8_t &operator-=(const uint16x8_t &a) {
        value = _mm_sub_epi16(value, a.value); //SSE2
        return *this;
    }

    inline uint16x8_t &operator+=(const uint16x8_t &a) {
        value = _mm_add_epi16(value, a.value); //SSE2
        return *this;
    }

    inline friend uint16x8_t operator>=(const uint16x8_t &a, const uint16x8_t &b) {
        uint16x8_t result;
        result.value =
            _mm_xor_si128(mask().value,
                          _mm_cmplt_epi16(a.value, b.value)); //SSE2
        return result;
    }

    inline friend uint16x8_t operator&&(const uint16x8_t &a, const uint16x8_t &b) {
        uint16x8_t result;
        result.value = _mm_and_si128(a.value, b.value); //SSE2
        return result;
    }

    // NOTE: uint16x8_t::blend behaves like mask ? a : b
    inline friend uint16x8_t blend(const uint16x8_t &a, const uint16x8_t &b, const uint16x8_t &mask) {
        uint16x8_t result;

        // Have to swap arguments to get intended calling semantics
        result.value =
            _mm_blendv_epi8(b.value, a.value, mask.value); //SSE4.1 alternatives?
        return result;
    }
};
} // namespace NEO
