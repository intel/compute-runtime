/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/memory_manager/memory_constants.h"

#include <cstdint>

namespace NEO {

template <uint8_t NumBits>
struct StorageType;

template <>
struct StorageType<8> {
    using Type = uint8_t;
};

template <>
struct StorageType<16> {
    using Type = uint16_t;
};

template <>
struct StorageType<32> {
    using Type = uint32_t;
};

template <>
struct StorageType<64> {
    using Type = uint64_t;
};

template <uint8_t NumBits>
struct StorageType {
    using Type = typename StorageType<NumBits + 1>::Type;
};

template <uint8_t NumBits>
using StorageTypeT = typename StorageType<NumBits>::Type;

template <uint8_t IntegerBits, uint8_t FractionalBits, uint8_t TotalBits = IntegerBits + FractionalBits>
struct UnsignedFixedPointValue {
    UnsignedFixedPointValue(float v) {
        fromFloatingPoint(v);
    }

    StorageTypeT<TotalBits> &getRawAccess() {
        return storage;
    }

    static constexpr float getMaxRepresentableFloat() {
        return getMaxRepresentableFloatingPointValue<float>();
    }

    float asFloat() {
        return asFloatPointType<float>();
    }

  protected:
    template <typename FloatingType>
    static constexpr FloatingType getMaxRepresentableFloatingPointValue() {
        return static_cast<FloatingType>(
            static_cast<FloatingType>(maxNBitValue(IntegerBits)) + (static_cast<FloatingType>(maxNBitValue(FractionalBits)) / (1U << FractionalBits)));
    }

    template <typename FloatingType>
    void fromFloatingPoint(FloatingType val) {
        auto maxFloatVal = getMaxRepresentableFloatingPointValue<FloatingType>();
        // clamp to [0, maxFloatVal]
        val = (val < FloatingType{0}) ? FloatingType{0} : val;
        val = (val > maxFloatVal) ? maxFloatVal : val;

        // scale to fixed point representation
        this->storage = static_cast<StorageTypeT<TotalBits>>(val * (1U << FractionalBits));
    }

    template <typename FloatingType>
    FloatingType asFloatPointType() {
        return static_cast<FloatingType>(storage) / (1U << FractionalBits);
    }

    StorageTypeT<TotalBits> storage = 0;
};

using FixedU4D8 = UnsignedFixedPointValue<4, 8>;
} // namespace NEO
