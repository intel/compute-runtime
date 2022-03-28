/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"

#include <arm_neon.h>
#include <cstdint>

namespace NEO {

struct uint16x16_t {
    enum { numChannels = 16 };

    uint16x8x2_t value;

    uint16x16_t() {
        value.val[0] = vdupq_n_u16(0);
        value.val[1] = vdupq_n_u16(0);
    }

    uint16x16_t(uint16x8_t lo, uint16x8_t hi) {
        value.val[0] = lo;
        value.val[1] = hi;
    }

    uint16x16_t(uint16_t a) {
        value.val[0] = vdupq_n_u16(a);
        value.val[1] = vdupq_n_u16(a);
    }

    explicit uint16x16_t(const void *alignedPtr) {
        load(alignedPtr);
    }

    inline uint16_t get(unsigned int element) {
        DEBUG_BREAK_IF(element >= numChannels);
        uint16_t result;
        // vgetq_lane requires constant immediate
        switch (element) {
        case 0:
            result = vgetq_lane_u16(value.val[0], 0);
            break;
        case 1:
            result = vgetq_lane_u16(value.val[0], 1);
            break;
        case 2:
            result = vgetq_lane_u16(value.val[0], 2);
            break;
        case 3:
            result = vgetq_lane_u16(value.val[0], 3);
            break;
        case 4:
            result = vgetq_lane_u16(value.val[0], 4);
            break;
        case 5:
            result = vgetq_lane_u16(value.val[0], 5);
            break;
        case 6:
            result = vgetq_lane_u16(value.val[0], 6);
            break;
        case 7:
            result = vgetq_lane_u16(value.val[0], 7);
            break;
        case 8:
            result = vgetq_lane_u16(value.val[1], 0);
            break;
        case 9:
            result = vgetq_lane_u16(value.val[1], 1);
            break;
        case 10:
            result = vgetq_lane_u16(value.val[1], 2);
            break;
        case 11:
            result = vgetq_lane_u16(value.val[1], 3);
            break;
        case 12:
            result = vgetq_lane_u16(value.val[1], 4);
            break;
        case 13:
            result = vgetq_lane_u16(value.val[1], 5);
            break;
        case 14:
            result = vgetq_lane_u16(value.val[1], 6);
            break;
        case 15:
            result = vgetq_lane_u16(value.val[1], 7);
            break;
        }

        return result;
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
        value = vld1q_u16_x2(reinterpret_cast<const uint16_t *>(alignedPtr));
    }

    inline void store(void *alignedPtr) {
        DEBUG_BREAK_IF(!isAligned<32>(alignedPtr));
        vst1q_u16_x2(reinterpret_cast<uint16_t *>(alignedPtr), value);
    }

    inline operator bool() const {
        uint64x2_t hi = vreinterpretq_u64_u16(value.val[0]);
        uint64x2_t lo = vreinterpretq_u64_u16(value.val[1]);
        uint64x2_t tmp = vorrq_u64(hi, lo);
        uint64_t result = vget_lane_u64(vorr_u64(vget_high_u64(tmp), vget_low_u64(tmp)), 0);

        return result;
    }

    inline uint16x16_t &operator-=(const uint16x16_t &a) {
        value.val[0] = vsubq_u16(value.val[0], a.value.val[0]);
        value.val[1] = vsubq_u16(value.val[1], a.value.val[1]);

        return *this;
    }

    inline uint16x16_t &operator+=(const uint16x16_t &a) {
        value.val[0] = vaddq_u16(value.val[0], a.value.val[0]);
        value.val[1] = vaddq_u16(value.val[1], a.value.val[1]);

        return *this;
    }

    inline friend uint16x16_t operator>=(const uint16x16_t &a, const uint16x16_t &b) {
        uint16x16_t result;

        result.value.val[0] = veorq_u16(mask().value.val[0],
                                        vcgtq_u16(b.value.val[0], a.value.val[0]));
        result.value.val[1] = veorq_u16(mask().value.val[1],
                                        vcgtq_u16(b.value.val[1], a.value.val[1]));
        return result;
    }

    inline friend uint16x16_t operator&&(const uint16x16_t &a, const uint16x16_t &b) {
        uint16x16_t result;

        result.value.val[0] = vandq_u16(a.value.val[0], b.value.val[0]);
        result.value.val[1] = vandq_u16(a.value.val[1], b.value.val[1]);

        return result;
    }

    // NOTE: uint16x16_t::blend behaves like mask ? a : b
    inline friend uint16x16_t blend(const uint16x16_t &a, const uint16x16_t &b, const uint16x16_t &mask) {
        uint16x16_t result;

        result.value.val[0] = vbslq_u16(mask.value.val[0], a.value.val[0], b.value.val[0]);
        result.value.val[1] = vbslq_u16(mask.value.val[1], a.value.val[1], b.value.val[1]);

        return result;
    }
};
} // namespace NEO
