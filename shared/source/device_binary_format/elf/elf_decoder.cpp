/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf_decoder.h"

#include "shared/source/helpers/ptr_math.h"

#include <string.h>

namespace NEO {

namespace Elf {

template <ELF_IDENTIFIER_CLASS NumBits>
const ElfFileHeader<NumBits> *decodeElfFileHeader(const ArrayRef<const uint8_t> binary) {
    if (binary.size() < sizeof(ElfFileHeader<NumBits>)) {
        return nullptr;
    }

    const ElfFileHeader<NumBits> *header = reinterpret_cast<const ElfFileHeader<NumBits> *>(binary.begin());
    bool validHeader = (header->identity.magic[0] == elfMagic[0]);
    validHeader &= (header->identity.magic[1] == elfMagic[1]);
    validHeader &= (header->identity.magic[2] == elfMagic[2]);
    validHeader &= (header->identity.magic[3] == elfMagic[3]);
    validHeader &= (header->identity.eClass == NumBits);

    return validHeader ? header : nullptr;
}

template const ElfFileHeader<EI_CLASS_32> *decodeElfFileHeader<EI_CLASS_32>(const ArrayRef<const uint8_t>);
template const ElfFileHeader<EI_CLASS_64> *decodeElfFileHeader<EI_CLASS_64>(const ArrayRef<const uint8_t>);

template <ELF_IDENTIFIER_CLASS NumBits>
Elf<NumBits> decodeElf(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarning) {
    Elf<NumBits> ret = {};
    ret.elfFileHeader = decodeElfFileHeader<NumBits>(binary);
    if (nullptr == ret.elfFileHeader) {
        outErrReason = "Invalid or missing ELF header";
        return {};
    }

    if (ret.elfFileHeader->phOff + ret.elfFileHeader->phNum * ret.elfFileHeader->phEntSize > binary.size()) {
        outErrReason = "Out of bounds program headers table";
        return {};
    }

    if (ret.elfFileHeader->shOff + ret.elfFileHeader->shNum * ret.elfFileHeader->shEntSize > binary.size()) {
        outErrReason = "Out of bounds section headers table";
        return {};
    }

    const ElfProgramHeader<NumBits> *programHeader = reinterpret_cast<const ElfProgramHeader<NumBits> *>(binary.begin() + ret.elfFileHeader->phOff);
    for (decltype(ret.elfFileHeader->phNum) i = 0; i < ret.elfFileHeader->phNum; ++i) {
        if (programHeader->offset + programHeader->fileSz > binary.size()) {
            outErrReason = "Out of bounds program header offset/filesz, program header idx : " + std::to_string(i);
            return {};
        }
        ArrayRef<const uint8_t> data(binary.begin() + programHeader->offset, static_cast<size_t>(programHeader->fileSz));
        ret.programHeaders.push_back({programHeader, data});
        programHeader = ptrOffset(programHeader, ret.elfFileHeader->phEntSize);
    }

    const ElfSectionHeader<NumBits> *sectionHeader = reinterpret_cast<const ElfSectionHeader<NumBits> *>(binary.begin() + ret.elfFileHeader->shOff);
    for (decltype(ret.elfFileHeader->shNum) i = 0; i < ret.elfFileHeader->shNum; ++i) {
        ArrayRef<const uint8_t> data;
        if (SHT_NOBITS != sectionHeader->type) {
            if (sectionHeader->offset + sectionHeader->size > binary.size()) {
                outErrReason = "Out of bounds section header offset/size, section header idx : " + std::to_string(i);
                return {};
            }
            data = ArrayRef<const uint8_t>(binary.begin() + sectionHeader->offset, static_cast<size_t>(sectionHeader->size));
        }
        ret.sectionHeaders.push_back({sectionHeader, data});
        sectionHeader = ptrOffset(sectionHeader, ret.elfFileHeader->shEntSize);
    }

    return ret;
}

template Elf<EI_CLASS_32> decodeElf<EI_CLASS_32>(const ArrayRef<const uint8_t>, std::string &, std::string &);
template Elf<EI_CLASS_64> decodeElf<EI_CLASS_64>(const ArrayRef<const uint8_t>, std::string &, std::string &);

} // namespace Elf

} // namespace NEO
