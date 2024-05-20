/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>

namespace NEO {

namespace Elf {

enum class RelocationX8664Type : uint32_t {
    relocation64 = 0x1,
    relocation32 = 0xa
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
struct ProgramHeaderAndData {
    const ElfProgramHeader<numBits> *header = nullptr;
    ArrayRef<const uint8_t> data;
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
struct SectionHeaderAndData {
    const ElfSectionHeader<numBits> *header;
    ArrayRef<const uint8_t> data;
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
struct Elf {
    struct RelocationInfo {
        int symbolSectionIndex;
        int symbolTableIndex;
        int targetSectionIndex;
        int64_t addend;
        uint64_t offset;
        uint32_t relocType;
        std::string symbolName;
    };
    using Relocations = std::vector<RelocationInfo>;
    using SymbolsTable = std::vector<ElfSymbolEntry<numBits>>;

    bool decodeSections(std::string &outError);

    template <class ElfReloc>
    int extractSymbolIndex(const ElfReloc &elfReloc) const;
    template <class ElfReloc>
    uint32_t extractRelocType(const ElfReloc &elfReloc) const;

    template <class ElfSymbol>
    uint32_t extractSymbolType(const ElfSymbol &elfSymbol) const {
        return elfSymbol.info & 0xf;
    }

    template <class ElfSymbol>
    uint32_t extractSymbolBind(const ElfSymbol &elfSymbol) const {
        return (elfSymbol.info >> 4) & 0xf;
    }

    MOCKABLE_VIRTUAL std::string getSectionName(uint32_t id) const {
        if (sectionHeaders.size() > id && sectionHeaders.size() > elfFileHeader->shStrNdx) {
            auto sectionHeaderNamesData = sectionHeaders[elfFileHeader->shStrNdx].data;
            return std::string(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()) + sectionHeaders[id].header->name);
        } else {
            return std::string("");
        }
    }

    std::string getName(uint32_t nameOffset) const {
        auto sectionHeaderNamesData = sectionHeaders[elfFileHeader->shStrNdx].data;
        return std::string(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()) + nameOffset);
    }

    MOCKABLE_VIRTUAL std::string getSymbolName(uint32_t nameOffset) const {
        return getName(nameOffset);
    }

    decltype(ElfSymbolEntry<numBits>::value) getSymbolValue(uint32_t idx) const {
        return symbolTable[idx].value;
    }

    decltype(ElfSectionHeader<numBits>::offset) getSectionOffset(uint32_t idx) const {
        return sectionHeaders[idx].header->offset;
    }

    const Relocations &getRelocations() const {
        return relocations;
    }

    const Relocations &getDebugInfoRelocations() const {
        return debugInfoRelocations;
    }

    const SymbolsTable &getSymbols() const {
        return symbolTable;
    }

    const ElfFileHeader<numBits> *elfFileHeader = nullptr;
    StackVec<ProgramHeaderAndData<numBits>, 32> programHeaders;
    StackVec<SectionHeaderAndData<numBits>, 32> sectionHeaders;

  protected:
    bool decodeSymTab(SectionHeaderAndData<numBits> &sectionHeaderData, std::string &outError);
    bool decodeRelocations(SectionHeaderAndData<numBits> &sectionHeaderData, std::string &outError);
    bool isDebugDataRelocation(ConstStringRef sectionName);

    SymbolsTable symbolTable;
    Relocations relocations;
    Relocations debugInfoRelocations;
};

struct DecodedNote {
    ConstStringRef name;
    ConstStringRef desc;
    uint32_t type;
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
bool decodeNoteSection(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning);
extern template bool decodeNoteSection<EI_CLASS_32>(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning);
extern template bool decodeNoteSection<EI_CLASS_64>(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning);

template <ElfIdentifierClass numBits = EI_CLASS_64>
const ElfFileHeader<numBits> *decodeElfFileHeader(const ArrayRef<const uint8_t> binary);
extern template const ElfFileHeader<EI_CLASS_32> *decodeElfFileHeader<EI_CLASS_32>(const ArrayRef<const uint8_t>);
extern template const ElfFileHeader<EI_CLASS_64> *decodeElfFileHeader<EI_CLASS_64>(const ArrayRef<const uint8_t>);

template <ElfIdentifierClass numBits = EI_CLASS_64>
Elf<numBits> decodeElf(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarning);
extern template Elf<EI_CLASS_32> decodeElf<EI_CLASS_32>(const ArrayRef<const uint8_t>, std::string &, std::string &);
extern template Elf<EI_CLASS_64> decodeElf<EI_CLASS_64>(const ArrayRef<const uint8_t>, std::string &, std::string &);

template <ElfIdentifierClass numBits>
inline bool isElf(const ArrayRef<const uint8_t> binary) {
    return (nullptr != decodeElfFileHeader<numBits>(binary));
}

inline bool isElf(const ArrayRef<const uint8_t> binary) {
    return isElf<EI_CLASS_32>(binary) || isElf<EI_CLASS_64>(binary);
}

inline ElfIdentifierClass getElfNumBits(const ArrayRef<const uint8_t> binary) {
    if (isElf<EI_CLASS_32>(binary)) {
        return EI_CLASS_32;
    } else if (isElf<EI_CLASS_64>(binary)) {
        return EI_CLASS_64;
    }
    return EI_CLASS_NONE;
}

} // namespace Elf

} // namespace NEO
