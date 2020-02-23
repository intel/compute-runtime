/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <algorithm>
#include <cstdint>

namespace NEO {

static constexpr ConstStringRef llvmBcMagic = "BC\xc0\xde";
static constexpr ConstStringRef spirvMagic = "\x07\x23\x02\x03";
static constexpr ConstStringRef spirvMagicInv = "\x03\x02\x23\x07";

inline bool hasSameMagic(ConstStringRef expectedMagic, ArrayRef<const uint8_t> binary) {
    auto binaryMagicLen = std::min(expectedMagic.size(), binary.size());
    ConstStringRef binaryMagic(reinterpret_cast<const char *>(binary.begin()), binaryMagicLen);
    return expectedMagic == binaryMagic;
}

inline bool isLlvmBitcode(ArrayRef<const uint8_t> binary) {
    return hasSameMagic(llvmBcMagic, binary);
}

inline bool isSpirVBitcode(ArrayRef<const uint8_t> binary) {
    return hasSameMagic(spirvMagic, binary) || hasSameMagic(spirvMagicInv, binary);
}

} // namespace NEO
