/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {
namespace Ar {

struct ArFileEntryHeaderAndData {
    ConstStringRef fileName;
    ArrayRef<const uint8_t> fileData;

    const ArFileEntryHeader *fullHeader = nullptr;
};

struct Ar {
    const char *magic = nullptr;
    StackVec<ArFileEntryHeaderAndData, 32> files;
    ArFileEntryHeaderAndData longFileNamesEntry;
};

inline bool isAr(const ArrayRef<const uint8_t> binary) {
    return NEO::hasSameMagic(arMagic, binary);
}

template <uint32_t MaxLength>
inline uint64_t readDecimal(const char *decimalAsString) {
    uint64_t ret = 0U;
    for (uint32_t i = 0; i < MaxLength; ++i) {
        if (('\0' == decimalAsString[i]) || (' ' == decimalAsString[i])) {
            break;
        }
        ret = ret * 10 + (decimalAsString[i] - '0');
    }
    return ret;
}

inline bool isStringPadding(char character) {
    switch (character) {
    default:
        return false;
    case ' ':
        return true;
    case '\0':
        return true;
    case '/':
        return true;
    }
}

template <uint32_t MaxLength>
inline ConstStringRef readUnpaddedString(const char *paddedString) {
    uint32_t unpaddedSize = MaxLength - 1;
    for (; unpaddedSize > 0U; --unpaddedSize) {
        if (false == isStringPadding(paddedString[unpaddedSize])) {
            break;
        }
    }
    if (false == isStringPadding(paddedString[unpaddedSize])) {
        ++unpaddedSize;
    }
    return ConstStringRef(paddedString, unpaddedSize);
}

inline ConstStringRef readLongFileName(ConstStringRef longFileNamesSection, size_t offset) {
    size_t end = offset;
    while ((end < longFileNamesSection.size()) && (longFileNamesSection[end] != SpecialFileNames::fileNameTerminator)) {
        ++end;
    }
    return ConstStringRef(longFileNamesSection.begin() + offset, end - offset);
}

Ar decodeAr(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarnings);

} // namespace Ar

} // namespace NEO
