/*
 * Copyright (C) 2018-2023 Intel Corporation
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
#elif defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__riscv)
#include <cstring>
typedef std::uint16_t __attribute__((vector_size(8))) __m128i;
#endif

namespace NEO {

struct uint16x8_t { // NOLINT(readability-identifier-naming)
    enum { numChannels = 8 };

    __m128i value;

    uint16x8_t() {
#if defined(__riscv)
            std::memset(&value, 0, sizeof(std::uint16_t)*8);
#else
        value = _mm_setzero_si128();
#endif
    }

#if defined(__riscv)
    uint16x8_t(__m128i val) {
            std::memcpy(&value, &val, sizeof(std::uint16_t)*8);
    }
#else
    uint16x8_t(__m128i value) : value(value) {
    }
#endif

    uint16x8_t(uint16_t a) {
#if defined(__riscv)
        std::memset(&value, a, sizeof(std::uint16_t)*8);
#else
        value = _mm_set1_epi16(a); // SSE2
#endif
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
#if defined(__riscv)
        std::memcpy(&value, reinterpret_cast<const __m128i *>(alignedPtr), sizeof(std::uint16_t)*8);
#else
        value = _mm_load_si128(reinterpret_cast<const __m128i *>(alignedPtr)); // SSE2
#endif
    }

    inline void loadUnaligned(const void *ptr) {
#if defined(__riscv)
            std::memcpy(&value, reinterpret_cast<const __m128i *>(ptr), sizeof(std::uint16_t)*8);
#else
        value = _mm_loadu_si128(reinterpret_cast<const __m128i *>(ptr)); // SSE2
#endif
    }

    inline void store(void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<16>(alignedPtr));
#if defined(__riscv)
        std::memcpy(alignedPtr, &value, sizeof(std::uint16_t)*8);
#else
        _mm_store_si128(reinterpret_cast<__m128i *>(alignedPtr), value); // SSE2
#endif
    }

    inline void storeUnaligned(void *ptr) {
#if defined(__riscv)
            std::memcpy(ptr, &value, sizeof(std::uint16_t)*8);
#else
        _mm_storeu_si128(reinterpret_cast<__m128i *>(ptr), value); // SSE2
#endif
    }

    inline operator bool() const {
#if defined(__riscv)
        unsigned int result = 0;
        uint16x8_t mask_value = mask();
        for(unsigned int i = 0; i < 8; ++i) {
           result += value[i] & mask_value.value[i];
        }
        return (result < 1);
#else
        return _mm_test_all_zeros(value, mask().value) ? false : true; // SSE4.1 alternatives?
#endif
    }

    inline uint16x8_t &operator-=(const uint16x8_t &a) {
#if defined(__riscv)
        for(unsigned int i = 0; i < 8; ++i) {
           value[i] = value[i] - a.value[i];
        }
#else
        value = _mm_sub_epi16(value, a.value); // SSE2
#endif
        return *this;
    }

    inline uint16x8_t &operator+=(const uint16x8_t &a) {
#if defined(__riscv)
        for(unsigned int i = 0; i < 8; ++i) {
           value[i] = value[i] + a.value[i];
        }
#else
        value = _mm_add_epi16(value, a.value); // SSE2
#endif
        return *this;
    }

    inline friend uint16x8_t operator>=(const uint16x8_t &a, const uint16x8_t &b) {
        uint16x8_t result;
#if defined(__riscv)
        std::uint16_t mask = 0;
        for(unsigned int i = 0; i < 8; ++i) {
           mask = ( a.value[i] < b.value[i] ) ? 1 : 0;
           result.value[i] = result.value[i] ^ mask;
        }
#else
        result.value =
            _mm_xor_si128(mask().value,
                          _mm_cmplt_epi16(a.value, b.value)); // SSE2
#endif
        return result;
    }

    inline friend uint16x8_t operator&&(const uint16x8_t &a, const uint16x8_t &b) {
        uint16x8_t result;
#if defined(__riscv)
        for(unsigned int i = 0; i < 8; ++i) {
           result.value[i] = a.value[i] & b.value[i];
        }
#else
        result.value = _mm_and_si128(a.value, b.value); // SSE2
#endif
        return result;
    }

    // NOTE: uint16x8_t::blend behaves like mask ? a : b
    inline friend uint16x8_t blend(const uint16x8_t &a, const uint16x8_t &b, const uint16x8_t &mask) {
        uint16x8_t result;
#if defined(__riscv)
        for(unsigned int i = 0; i < 8; ++i) {
           std::uint8_t mask_values[2] = {  static_cast<uint8_t>((mask.value[i] << 8) >> 8), static_cast<uint8_t>(mask.value[i] >> 8) };
           std::uint8_t a_values[2] = { static_cast<uint8_t>((a.value[i] << 8) >> 8), static_cast<uint8_t>(a.value[i] >> 8) };
           std::uint8_t b_values[2] = { static_cast<uint8_t>((b.value[i] << 8) >> 8), static_cast<uint8_t>(b.value[i] >> 8) };

           result.value[i] = (( mask_values[1] ? a_values[1] : b_values[0] ) << 8 ) | ( mask_values[0] ? a_values[0] : b_values[0] );
        }
#else
        // Have to swap arguments to get intended calling semantics
        result.value =
            _mm_blendv_epi8(b.value, a.value, mask.value); // SSE4.1 alternatives?
#endif
        return result;
    }
};
} // namespace NEO
