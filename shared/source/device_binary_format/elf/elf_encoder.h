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

#include <queue>
#include <string>
#include <vector>

namespace NEO {

namespace Elf {

struct StringSectionBuilder {
    StringSectionBuilder() {
        stringTable.push_back('\0');
        undefStringIdx = 0U;
    }

    uint32_t appendString(ConstStringRef str) {
        if (str.empty()) {
            return undefStringIdx;
        }
        uint32_t offset = static_cast<uint32_t>(stringTable.size());
        stringTable.insert(stringTable.end(), str.begin(), str.end());
        if (str[str.size() - 1] != '\0') {
            stringTable.push_back('\0');
        }
        return offset;
    }

    ArrayRef<const uint8_t> data() const {
        return ArrayRef<const uint8_t>::fromAny(stringTable.data(), stringTable.size());
    }

    uint32_t undef() const {
        return undefStringIdx;
    }

  protected:
    std::vector<char> stringTable;
    uint32_t undefStringIdx;
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
struct ElfEncoder {
    ElfEncoder(bool addUndefSectionHeader = true, bool addHeaderSectionNamesSection = true,
               typename ElfSectionHeaderTypes<numBits>::AddrAlign defaultDataAlignment = 8U);

    ElfSectionHeader<numBits> &appendSection(const ElfSectionHeader<numBits> &sectionHeader, const ArrayRef<const uint8_t> sectionData);
    ElfProgramHeader<numBits> &appendSegment(const ElfProgramHeader<numBits> &programHeader, const ArrayRef<const uint8_t> segmentData);

    ElfSectionHeader<numBits> &appendSection(SectionHeaderType sectionType, ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
    ElfProgramHeader<numBits> &appendSegment(ProgramHeaderType segmentType, const ArrayRef<const uint8_t> segmentData);
    uint32_t getSectionHeaderIndex(const ElfSectionHeader<numBits> &sectionHeader);
    void appendProgramHeaderLoad(size_t sectionId, uint64_t vAddr, uint64_t segSize);

    template <typename SectionHeaderEnumT>
    ElfSectionHeader<numBits> &appendSection(SectionHeaderEnumT sectionType, ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData) {
        return appendSection(static_cast<SectionHeaderType>(sectionType), sectionLabel, sectionData);
    }

    template <typename SectionHeaderEnumT>
    ElfSectionHeader<numBits> &appendSection(SectionHeaderEnumT sectionType, ConstStringRef sectionLabel, const std::string &sectionData) {
        return appendSection(static_cast<SectionHeaderType>(sectionType), sectionLabel,
                             ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(sectionData.c_str()), sectionData.size() + 1));
    }

    uint32_t appendSectionName(ConstStringRef str);

    std::vector<uint8_t> encode() const;

    ElfFileHeader<numBits> &getElfFileHeader() {
        return elfFileHeader;
    }

  protected:
    bool addUndefSectionHeader = false;
    bool addHeaderSectionNamesSection = false;
    typename ElfSectionHeaderTypes<numBits>::AddrAlign defaultDataAlignment = 8U;
    uint64_t maxDataAlignmentNeeded = 1U;
    ElfFileHeader<numBits> elfFileHeader;
    StackVec<ElfProgramHeader<numBits>, 32> programHeaders;
    StackVec<ElfSectionHeader<numBits>, 32> sectionHeaders;
    std::vector<uint8_t> data;
    StringSectionBuilder strSecBuilder;
    struct ProgramSectionID {
        size_t programId;
        size_t sectionId;
    };
    StackVec<ProgramSectionID, 32> programSectionLookupTable;
    uint32_t shStrTabNameOffset = 0;
};

struct NoteToEncode {
    std::string name;
    std::string desc;
    uint32_t type;
};

template <ElfIdentifierClass numBits = EI_CLASS_64>
std::vector<uint8_t> encodeNoteSectionData(ArrayRef<const NoteToEncode> notes);
extern template std::vector<uint8_t> encodeNoteSectionData<EI_CLASS_64>(ArrayRef<const NoteToEncode> notes);
extern template std::vector<uint8_t> encodeNoteSectionData<EI_CLASS_32>(ArrayRef<const NoteToEncode> notes);

extern template struct ElfEncoder<EI_CLASS_32>;
extern template struct ElfEncoder<EI_CLASS_64>;

} // namespace Elf

} // namespace NEO
