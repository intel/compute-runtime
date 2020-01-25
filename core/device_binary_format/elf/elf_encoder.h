/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/device_binary_format/elf/elf.h"
#include "core/helpers/aligned_memory.h"
#include "core/utilities/arrayref.h"
#include "core/utilities/const_stringref.h"
#include "core/utilities/stackvec.h"

#include <queue>
#include <string>

namespace NEO {

namespace Elf {

template <ELF_IDENTIFIER_CLASS NumBits = EI_CLASS_64>
struct ElfEncoder {
    ElfEncoder(bool addUndefSectionHeader = true, bool addHeaderSectionNamesSection = true, uint64_t defaultDataAlignment = 8U);

    void appendSection(const ElfSectionHeader<NumBits> &sectionHeader, const ArrayRef<const uint8_t> sectionData);
    void appendSegment(const ElfProgramHeader<NumBits> &programHeader, const ArrayRef<const uint8_t> segmentData);

    ElfSectionHeader<NumBits> &appendSection(SECTION_HEADER_TYPE sectionType, ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
    ElfProgramHeader<NumBits> &appendSegment(PROGRAM_HEADER_TYPE segmentType, const ArrayRef<const uint8_t> segmentData);

    template <typename SectionHeaderEnumT>
    ElfSectionHeader<NumBits> &appendSection(SectionHeaderEnumT sectionType, ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData) {
        return appendSection(static_cast<SECTION_HEADER_TYPE>(sectionType), sectionLabel, sectionData);
    }

    template <typename SectionHeaderEnumT>
    ElfSectionHeader<NumBits> &appendSection(SectionHeaderEnumT sectionType, ConstStringRef sectionLabel, const std::string &sectionData) {
        return appendSection(static_cast<SECTION_HEADER_TYPE>(sectionType), sectionLabel,
                             ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(sectionData.c_str()), sectionData.size() + 1));
    }

    uint32_t appendSectionName(ConstStringRef str);

    std::vector<uint8_t> encode() const;

    ElfFileHeader<NumBits> &getElfFileHeader() {
        return elfFileHeader;
    }

  protected:
    bool addUndefSectionHeader = false;
    bool addHeaderSectionNamesSection = false;
    uint64_t defaultDataAlignment = 8U;
    uint64_t maxDataAlignmentNeeded = 1U;
    ElfFileHeader<NumBits> elfFileHeader;
    StackVec<ElfProgramHeader<NumBits>, 32> programHeaders;
    StackVec<ElfSectionHeader<NumBits>, 32> sectionHeaders;
    std::vector<uint8_t> data;
    std::vector<char> stringTable;
    struct {
        uint32_t shStrTab = 0;
        uint32_t undef = 0;
    } specialStringsOffsets;
};

extern template struct ElfEncoder<EI_CLASS_32>;
extern template struct ElfEncoder<EI_CLASS_64>;

} // namespace Elf

} // namespace NEO
