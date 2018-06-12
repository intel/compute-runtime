/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include <cstdint>
#include <immintrin.h>

namespace OCLRT {

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
} // namespace OCLRT
