/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/device_binary_format/elf/elf.h"
#include "core/utilities/arrayref.h"
#include "core/utilities/const_stringref.h"
#include "core/utilities/stackvec.h"

#include <cstdint>

namespace NEO {

namespace Elf {

template <ELF_IDENTIFIER_CLASS NumBits = EI_CLASS_64>
struct Elf {
    struct ProgramHeaderAndData {
        const ElfProgramHeader<NumBits> *header = nullptr;
        ArrayRef<const uint8_t> data;
    };

    struct SectionHeaderAndData {
        const ElfSectionHeader<NumBits> *header;
        ArrayRef<const uint8_t> data;
    };

    const ElfFileHeader<NumBits> *elfFileHeader = nullptr;
    StackVec<ProgramHeaderAndData, 32> programHeaders;
    StackVec<SectionHeaderAndData, 32> sectionHeaders;
};

template <ELF_IDENTIFIER_CLASS NumBits = EI_CLASS_64>
const ElfFileHeader<NumBits> *decodeElfFileHeader(const ArrayRef<const uint8_t> binary);
extern template const ElfFileHeader<EI_CLASS_32> *decodeElfFileHeader<EI_CLASS_32>(const ArrayRef<const uint8_t>);
extern template const ElfFileHeader<EI_CLASS_64> *decodeElfFileHeader<EI_CLASS_64>(const ArrayRef<const uint8_t>);

template <ELF_IDENTIFIER_CLASS NumBits = EI_CLASS_64>
Elf<NumBits> decodeElf(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarning);
extern template Elf<EI_CLASS_32> decodeElf<EI_CLASS_32>(const ArrayRef<const uint8_t>, std::string &, std::string &);
extern template Elf<EI_CLASS_64> decodeElf<EI_CLASS_64>(const ArrayRef<const uint8_t>, std::string &, std::string &);

template <ELF_IDENTIFIER_CLASS NumBits>
inline bool isElf(const ArrayRef<const uint8_t> binary) {
    return (nullptr != decodeElfFileHeader<NumBits>(binary));
}

inline bool isElf(const ArrayRef<const uint8_t> binary) {
    return isElf<EI_CLASS_32>(binary) || isElf<EI_CLASS_64>(binary);
}

inline ELF_IDENTIFIER_CLASS getElfNumBits(const ArrayRef<const uint8_t> binary) {
    if (isElf<EI_CLASS_32>(binary)) {
        return EI_CLASS_32;
    } else if (isElf<EI_CLASS_64>(binary)) {
        return EI_CLASS_64;
    }
    return EI_CLASS_NONE;
}

} // namespace Elf

} // namespace NEO
