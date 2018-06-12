/*
 * Copyright (c) 2018, Intel Corporation
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

#include <cstdint>

namespace OCLRT {

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
            static_cast<FloatingType>((1U << IntegerBits) - 1) + (static_cast<FloatingType>((1U << FractionalBits) - 1) / (1U << FractionalBits)));
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
} // namespace OCLRT
