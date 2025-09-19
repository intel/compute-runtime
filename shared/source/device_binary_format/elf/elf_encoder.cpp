/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf_encoder.h"

#include "shared/source/helpers/aligned_memory.h"

#include <algorithm>

namespace NEO {

namespace Elf {

template <ElfIdentifierClass numBits>
ElfEncoder<numBits>::ElfEncoder(bool addUndefSectionHeader, bool addHeaderSectionNamesSection, typename ElfSectionHeaderTypes<numBits>::AddrAlign defaultDataAlignment)
    : addUndefSectionHeader(addUndefSectionHeader), addHeaderSectionNamesSection(addHeaderSectionNamesSection), defaultDataAlignment(defaultDataAlignment) {
    // add special strings
    UNRECOVERABLE_IF(defaultDataAlignment == 0);
    shStrTabNameOffset = this->appendSectionName(SpecialSectionNames::shStrTab);

    if (addUndefSectionHeader) {
        ElfSectionHeader<numBits> undefSection;
        sectionHeaders.push_back(undefSection);
    }
}

template <ElfIdentifierClass numBits>
ElfSectionHeader<numBits> &ElfEncoder<numBits>::appendSection(const ElfSectionHeader<numBits> &sectionHeader, const ArrayRef<const uint8_t> sectionData) {
    sectionHeaders.push_back(sectionHeader);
    if ((SHT_NOBITS != sectionHeader.type) && (false == sectionData.empty())) {
        auto sectionDataAlignment = std::min<uint64_t>(defaultDataAlignment, 8U);
        auto alignedOffset = alignUp(this->data.size(), static_cast<size_t>(sectionDataAlignment));
        auto alignedSize = alignUp(sectionData.size(), static_cast<size_t>(sectionDataAlignment));
        this->data.reserve(alignedOffset + alignedSize);
        this->data.resize(alignedOffset, 0U);
        this->data.insert(this->data.end(), sectionData.begin(), sectionData.end());
        this->data.resize(alignedOffset + alignedSize, 0U);
        sectionHeaders.rbegin()->offset = static_cast<decltype(sectionHeaders.rbegin()->offset)>(alignedOffset);
        sectionHeaders.rbegin()->size = static_cast<decltype(sectionHeaders.rbegin()->size)>(sectionData.size());
    }
    return *sectionHeaders.rbegin();
}

template <ElfIdentifierClass numBits>
ElfProgramHeader<numBits> &ElfEncoder<numBits>::appendSegment(const ElfProgramHeader<numBits> &programHeader, const ArrayRef<const uint8_t> segmentData) {
    maxDataAlignmentNeeded = std::max<uint64_t>(maxDataAlignmentNeeded, static_cast<uint64_t>(programHeader.align));
    programHeaders.push_back(programHeader);
    if (false == segmentData.empty()) {
        UNRECOVERABLE_IF(programHeader.align == 0);
        auto alignedOffset = alignUp(this->data.size(), static_cast<size_t>(programHeader.align));
        auto alignedSize = alignUp(segmentData.size(), static_cast<size_t>(programHeader.align));
        this->data.reserve(alignedOffset + alignedSize);
        this->data.resize(alignedOffset, 0U);
        this->data.insert(this->data.end(), segmentData.begin(), segmentData.end());
        this->data.resize(alignedOffset + alignedSize, 0U);
        programHeaders.rbegin()->offset = static_cast<decltype(programHeaders.rbegin()->offset)>(alignedOffset);
        programHeaders.rbegin()->fileSz = static_cast<decltype(programHeaders.rbegin()->fileSz)>(segmentData.size());
    }
    return *programHeaders.rbegin();
}

template <ElfIdentifierClass numBits>
uint32_t ElfEncoder<numBits>::getSectionHeaderIndex(const ElfSectionHeader<numBits> &sectionHeader) {
    UNRECOVERABLE_IF(&sectionHeader < sectionHeaders.begin());
    UNRECOVERABLE_IF(&sectionHeader >= sectionHeaders.begin() + sectionHeaders.size());
    return static_cast<uint32_t>(&sectionHeader - &*sectionHeaders.begin());
}

template <ElfIdentifierClass numBits>
ElfSectionHeader<numBits> &ElfEncoder<numBits>::appendSection(SectionHeaderType sectionType, ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData) {
    ElfSectionHeader<numBits> section = {};
    section.type = static_cast<decltype(section.type)>(sectionType);
    section.flags = static_cast<decltype(section.flags)>(SHF_NONE);
    section.offset = 0U;
    section.name = appendSectionName(sectionLabel);
    section.addralign = defaultDataAlignment;
    switch (sectionType) {
    case SHT_REL:
        section.entsize = sizeof(ElfRel<numBits>);
        break;
    case SHT_RELA:
        section.entsize = sizeof(ElfRela<numBits>);
        break;
    case SHT_SYMTAB:
        section.entsize = sizeof(ElfSymbolEntry<numBits>);
        break;
    default:
        break;
    }
    return appendSection(section, sectionData);
}

template <ElfIdentifierClass numBits>
ElfProgramHeader<numBits> &ElfEncoder<numBits>::appendSegment(ProgramHeaderType segmentType, const ArrayRef<const uint8_t> segmentData) {
    ElfProgramHeader<numBits> segment = {};
    segment.type = static_cast<decltype(segment.type)>(segmentType);
    segment.flags = static_cast<decltype(segment.flags)>(PF_NONE);
    segment.offset = 0U;
    segment.align = static_cast<decltype(segment.align)>(defaultDataAlignment);
    return appendSegment(segment, segmentData);
}

template <ElfIdentifierClass numBits>
void ElfEncoder<numBits>::appendProgramHeaderLoad(size_t sectionId, uint64_t vAddr, uint64_t segSize) {
    programSectionLookupTable.push_back({programHeaders.size(), sectionId});
    auto &programHeader = appendSegment(ProgramHeaderType::PT_LOAD, {});
    programHeader.vAddr = static_cast<decltype(programHeader.vAddr)>(vAddr);
    programHeader.memSz = static_cast<decltype(programHeader.memSz)>(segSize);
}

template <ElfIdentifierClass numBits>
uint32_t ElfEncoder<numBits>::appendSectionName(ConstStringRef str) {
    if (false == addHeaderSectionNamesSection) {
        return strSecBuilder.undef();
    }
    return strSecBuilder.appendString(str);
}

template <ElfIdentifierClass numBits>
std::vector<uint8_t> ElfEncoder<numBits>::encode() const {
    ElfFileHeader<numBits> elfFileHeader = this->elfFileHeader;
    StackVec<ElfProgramHeader<numBits>, 32> programHeaders = this->programHeaders;
    StackVec<ElfSectionHeader<numBits>, 32> sectionHeaders = this->sectionHeaders;

    if (addUndefSectionHeader && (1U == sectionHeaders.size())) {
        sectionHeaders.clear();
    }

    ElfSectionHeader<numBits> sectionHeaderNamesSection;
    size_t alignedSectionNamesDataSize = 0U;
    size_t dataPaddingBeforeSectionNames = 0U;
    if ((false == sectionHeaders.empty()) && addHeaderSectionNamesSection) {
        auto alignedDataSize = alignUp(data.size(), static_cast<size_t>(defaultDataAlignment));
        dataPaddingBeforeSectionNames = alignedDataSize - data.size();
        sectionHeaderNamesSection.type = SHT_STRTAB;
        sectionHeaderNamesSection.name = shStrTabNameOffset;
        sectionHeaderNamesSection.offset = static_cast<decltype(sectionHeaderNamesSection.offset)>(alignedDataSize);
        sectionHeaderNamesSection.size = static_cast<decltype(sectionHeaderNamesSection.size)>(strSecBuilder.data().size());
        sectionHeaderNamesSection.addralign = static_cast<decltype(sectionHeaderNamesSection.addralign)>(defaultDataAlignment);
        elfFileHeader.shStrNdx = static_cast<decltype(elfFileHeader.shStrNdx)>(sectionHeaders.size());
        sectionHeaders.push_back(sectionHeaderNamesSection);
        alignedSectionNamesDataSize = alignUp(strSecBuilder.data().size(), static_cast<size_t>(sectionHeaderNamesSection.addralign));
    }

    elfFileHeader.phNum = static_cast<decltype(elfFileHeader.phNum)>(programHeaders.size());
    elfFileHeader.shNum = static_cast<decltype(elfFileHeader.shNum)>(sectionHeaders.size());

    auto programHeadersOffset = elfFileHeader.ehSize;
    auto sectionHeadersOffset = programHeadersOffset + elfFileHeader.phEntSize * elfFileHeader.phNum;

    if (false == programHeaders.empty()) {
        elfFileHeader.phOff = static_cast<decltype(elfFileHeader.phOff)>(programHeadersOffset);
    }
    if (false == sectionHeaders.empty()) {
        elfFileHeader.shOff = static_cast<decltype(elfFileHeader.shOff)>(sectionHeadersOffset);
    }

    auto dataOffset = alignUp(sectionHeadersOffset + elfFileHeader.shEntSize * elfFileHeader.shNum, static_cast<size_t>(maxDataAlignmentNeeded));
    auto stringTabOffset = dataOffset + data.size();

    std::vector<uint8_t> ret;
    ret.reserve(stringTabOffset + alignedSectionNamesDataSize);
    ret.insert(ret.end(), reinterpret_cast<uint8_t *>(&elfFileHeader), reinterpret_cast<uint8_t *>(&elfFileHeader + 1));
    ret.resize(programHeadersOffset, 0U);

    for (auto &progSecLookup : programSectionLookupTable) {
        programHeaders[progSecLookup.programId].offset = sectionHeaders[progSecLookup.sectionId].offset;
        programHeaders[progSecLookup.programId].fileSz = sectionHeaders[progSecLookup.sectionId].size;
    }

    std::sort(programHeaders.begin(), programHeaders.end(), [](auto &p1, auto &p2) { return p1.vAddr < p2.vAddr; });
    for (auto &programHeader : programHeaders) {
        if (0 != programHeader.fileSz) {
            programHeader.offset = static_cast<decltype(programHeader.offset)>(programHeader.offset + dataOffset);
        }
        ret.insert(ret.end(), reinterpret_cast<uint8_t *>(&programHeader), reinterpret_cast<uint8_t *>(&programHeader + 1));
        ret.resize(ret.size() + elfFileHeader.phEntSize - sizeof(programHeader), 0U);
    }

    for (auto &sectionHeader : sectionHeaders) {
        if ((SHT_NOBITS != sectionHeader.type) && (0 != sectionHeader.size)) {
            sectionHeader.offset = static_cast<decltype(sectionHeader.offset)>(sectionHeader.offset + dataOffset);
        }
        ret.insert(ret.end(), reinterpret_cast<uint8_t *>(&sectionHeader), reinterpret_cast<uint8_t *>(&sectionHeader + 1));
        ret.resize(ret.size() + elfFileHeader.shEntSize - sizeof(sectionHeader), 0U);
    }

    ret.resize(dataOffset, 0U);
    ret.insert(ret.end(), data.begin(), data.end());
    ret.resize(ret.size() + dataPaddingBeforeSectionNames, 0U);
    if (alignedSectionNamesDataSize > 0U) {
        auto sectionNames = strSecBuilder.data();
        ret.insert(ret.end(), sectionNames.begin(), sectionNames.end());
    }
    ret.resize(ret.size() + alignedSectionNamesDataSize - static_cast<size_t>(sectionHeaderNamesSection.size), 0U);
    return ret;
}

template <ElfIdentifierClass numBits>
std::vector<uint8_t> encodeNoteSectionData(ArrayRef<const NoteToEncode> notes) {
    std::vector<uint8_t> ret;
    {
        size_t dataSize = 0U;
        for (auto &note : notes) {
            dataSize += sizeof(ElfNoteSection);
            dataSize += note.name.size();
            dataSize += note.desc.size();
            dataSize = alignUp(dataSize, 4);
        }
        ret.reserve(dataSize);
    }

    for (auto &note : notes) {
        ElfNoteSection entry;
        entry.nameSize = static_cast<uint32_t>(note.name.size());
        entry.descSize = static_cast<uint32_t>(note.desc.size());
        entry.type = note.type;
        ret.insert(ret.end(), reinterpret_cast<uint8_t *>(&entry), reinterpret_cast<uint8_t *>((&entry) + 1));
        ret.insert(ret.end(), reinterpret_cast<const uint8_t *>(note.name.c_str()), reinterpret_cast<const uint8_t *>(note.name.c_str() + note.name.size()));
        ret.insert(ret.end(), reinterpret_cast<const uint8_t *>(note.desc.c_str()), reinterpret_cast<const uint8_t *>(note.desc.c_str() + note.desc.size()));
        ret.resize(alignUp(ret.size(), 4), 0U);
    }

    return ret;
}

template std::vector<uint8_t> encodeNoteSectionData<EI_CLASS_64>(ArrayRef<const NoteToEncode> notes);
template std::vector<uint8_t> encodeNoteSectionData<EI_CLASS_32>(ArrayRef<const NoteToEncode> notes);

template struct ElfEncoder<EI_CLASS_32>;
template struct ElfEncoder<EI_CLASS_64>;

} // namespace Elf

} // namespace NEO
