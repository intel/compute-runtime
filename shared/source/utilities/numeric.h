/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <cstdint>

namespace NEO {

template <uint8_t numBits>
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

template <uint8_t numBits>
struct StorageType {
    using Type = typename StorageType<numBits + 1>::Type;
};

template <uint8_t numBits>
using StorageTypeT = typename StorageType<numBits>::Type;

template <uint8_t integerBits, uint8_t fractionalBits, uint8_t totalBits = integerBits + fractionalBits>
struct UnsignedFixedPointValue {
    UnsignedFixedPointValue(float v) {
        fromFloatingPoint(v);
    }

    StorageTypeT<totalBits> &getRawAccess() {
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
            static_cast<FloatingType>(maxNBitValue(integerBits)) + (static_cast<FloatingType>(maxNBitValue(fractionalBits)) / (1U << fractionalBits)));
    }

    template <typename FloatingType>
    void fromFloatingPoint(FloatingType val) {
        auto maxFloatVal = getMaxRepresentableFloatingPointValue<FloatingType>();
        // clamp to [0, maxFloatVal]
        val = (val < FloatingType{0}) ? FloatingType{0} : val;
        val = (val > maxFloatVal) ? maxFloatVal : val;

        // scale to fixed point representation
        this->storage = static_cast<StorageTypeT<totalBits>>(val * (1U << fractionalBits));
    }

    template <typename FloatingType>
    FloatingType asFloatPointType() {
        return static_cast<FloatingType>(storage) / (1U << fractionalBits);
    }

    StorageTypeT<totalBits> storage = 0;
};

using FixedU4D8 = UnsignedFixedPointValue<4, 8>;
} // namespace NEO
