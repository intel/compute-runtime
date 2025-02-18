/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf_decoder.h"

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"

#include <string.h>

namespace NEO {

namespace Elf {

template <ElfIdentifierClass numBits>
bool decodeNoteSection(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning) {
    uint64_t pos = 0;
    auto sectionSize = sectionData.size();
    auto base = sectionData.begin();
    while (pos < sectionSize) {
        auto note = reinterpret_cast<const ElfNoteSection *>(base + pos);
        auto alignedEntrySize = alignUp(sizeof(ElfNoteSection) + note->nameSize + note->descSize, 4);
        if (pos + alignedEntrySize > sectionSize) {
            outErrReason.append("Invalid elf note section - not enough data\n");
            return false;
        }
        ConstStringRef name{reinterpret_cast<const char *>(note + 1), note->nameSize};
        ConstStringRef desc{reinterpret_cast<const char *>(note + 1) + note->nameSize, note->descSize};
        pos += alignedEntrySize;

        out.push_back(DecodedNote{name, desc, note->type});
    }
    return true;
}

template bool decodeNoteSection<EI_CLASS_32>(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning);
template bool decodeNoteSection<EI_CLASS_64>(ArrayRef<const uint8_t> sectionData, std::vector<DecodedNote> &out, std::string &outErrReason, std::string &outWarning);

template <ElfIdentifierClass numBits>
const ElfFileHeader<numBits> *decodeElfFileHeader(const ArrayRef<const uint8_t> binary) {
    if (binary.size() < sizeof(ElfFileHeader<numBits>)) {
        return nullptr;
    }

    const ElfFileHeader<numBits> *header = reinterpret_cast<const ElfFileHeader<numBits> *>(binary.begin());
    bool validHeader = (header->identity.magic[0] == elfMagic[0]);
    validHeader &= (header->identity.magic[1] == elfMagic[1]);
    validHeader &= (header->identity.magic[2] == elfMagic[2]);
    validHeader &= (header->identity.magic[3] == elfMagic[3]);
    validHeader &= (header->identity.eClass == numBits);

    return validHeader ? header : nullptr;
}

template const ElfFileHeader<EI_CLASS_32> *decodeElfFileHeader<EI_CLASS_32>(const ArrayRef<const uint8_t>);
template const ElfFileHeader<EI_CLASS_64> *decodeElfFileHeader<EI_CLASS_64>(const ArrayRef<const uint8_t>);

template <ElfIdentifierClass numBits>
Elf<numBits> decodeElf(const ArrayRef<const uint8_t> binary, std::string &outErrReason, std::string &outWarning) {
    Elf<numBits> ret = {};
    ret.elfFileHeader = decodeElfFileHeader<numBits>(binary);
    if (nullptr == ret.elfFileHeader) {
        outErrReason = "Invalid or missing ELF header";
        return {};
    }

    if (ret.elfFileHeader->phOff + static_cast<uint32_t>(ret.elfFileHeader->phNum * ret.elfFileHeader->phEntSize) > binary.size()) {
        outErrReason = "Out of bounds program headers table";
        return {};
    }

    if (ret.elfFileHeader->shOff + static_cast<uint32_t>(ret.elfFileHeader->shNum * ret.elfFileHeader->shEntSize) > binary.size()) {
        outErrReason = "Out of bounds section headers table";
        return {};
    }

    const ElfProgramHeader<numBits> *programHeader = reinterpret_cast<const ElfProgramHeader<numBits> *>(binary.begin() + ret.elfFileHeader->phOff);
    for (decltype(ret.elfFileHeader->phNum) i = 0; i < ret.elfFileHeader->phNum; ++i) {
        if (programHeader->offset + programHeader->fileSz > binary.size()) {
            outErrReason = "Out of bounds program header offset/filesz, program header idx : " + std::to_string(i);
            return {};
        }
        ArrayRef<const uint8_t> data(binary.begin() + programHeader->offset, static_cast<size_t>(programHeader->fileSz));
        ret.programHeaders.push_back({programHeader, data});
        programHeader = ptrOffset(programHeader, ret.elfFileHeader->phEntSize);
    }

    const ElfSectionHeader<numBits> *sectionHeader = reinterpret_cast<const ElfSectionHeader<numBits> *>(binary.begin() + ret.elfFileHeader->shOff);
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

    if (!ret.decodeSections(outErrReason)) {
        return {};
    }

    return ret;
}

template <ElfIdentifierClass numBits>
bool Elf<numBits>::decodeSymTab(SectionHeaderAndData<numBits> &sectionHeaderData, std::string &outError) {
    if (sectionHeaderData.header->type == SectionHeaderType::SHT_SYMTAB) {
        auto symSize = sizeof(ElfSymbolEntry<numBits>);
        if (symSize != sectionHeaderData.header->entsize) {
            outError.append("Invalid symbol table entries size - expected : " + std::to_string(symSize) + ", got : " + std::to_string(sectionHeaderData.header->entsize) + "\n");
            return false;
        }
        auto numberOfSymbols = static_cast<size_t>(sectionHeaderData.header->size / sectionHeaderData.header->entsize);
        auto symbol = reinterpret_cast<const ElfSymbolEntry<numBits> *>(sectionHeaderData.data.begin());

        symbolTable.resize(numberOfSymbols);
        for (size_t i = 0; i < numberOfSymbols; i++) {
            symbolTable[i] = *symbol;
            symbol++;
        }
    }
    return true;
}

template <ElfIdentifierClass numBits>
bool Elf<numBits>::decodeRelocations(SectionHeaderAndData<numBits> &sectionHeaderData, std::string &outError) {
    if (sectionHeaderData.header->type == SectionHeaderType::SHT_RELA) {
        auto relaSize = sizeof(ElfRela<numBits>);
        if (relaSize != sectionHeaderData.header->entsize) {
            outError.append("Invalid rela entries size - expected : " + std::to_string(relaSize) + ", got : " + std::to_string(sectionHeaderData.header->entsize) + "\n");
            return false;
        }
        size_t numberOfEntries = static_cast<size_t>(sectionHeaderData.header->size / sectionHeaderData.header->entsize);
        auto sectionHeaderNamesData = sectionHeaders[elfFileHeader->shStrNdx].data;
        int targetSectionIndex = sectionHeaderData.header->info;
        auto sectionName = getSectionName(targetSectionIndex);
        auto debugDataRelocation = isDebugDataRelocation(ConstStringRef(sectionName.c_str()));
        Relocations &relocs = debugDataRelocation ? debugInfoRelocations : relocations;

        auto rela = reinterpret_cast<const ElfRela<numBits> *>(sectionHeaderData.data.begin());

        // there may be multiple rela sections, reserve additional size
        auto previousEntries = relocations.size();
        auto allEntries = previousEntries + numberOfEntries;
        relocs.reserve(allEntries);

        for (auto i = previousEntries; i < allEntries; i++) {

            int symbolIndex = extractSymbolIndex<ElfRela<numBits>>(*rela);
            auto relocType = extractRelocType<ElfRela<numBits>>(*rela);
            int symbolSectionIndex = symbolTable[symbolIndex].shndx;
            std::string name = std::string(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()) + symbolTable[symbolIndex].name);

            RelocationInfo relocInfo = {symbolSectionIndex, symbolIndex, targetSectionIndex, rela->addend, rela->offset, relocType, std::move(name)};

            relocs.push_back(relocInfo);
            rela++;
        }
    }

    if (sectionHeaderData.header->type == SectionHeaderType::SHT_REL) {
        auto relSize = sizeof(ElfRel<numBits>);
        if (relSize != sectionHeaderData.header->entsize) {
            outError.append("Invalid rel entries size - expected : " + std::to_string(relSize) + ", got : " + std::to_string(sectionHeaderData.header->entsize) + "\n");
            return false;
        }
        auto numberOfEntries = static_cast<size_t>(sectionHeaderData.header->size / sectionHeaderData.header->entsize);

        auto sectionHeaderNamesData = sectionHeaders[elfFileHeader->shStrNdx].data;
        int targetSectionIndex = sectionHeaderData.header->info;
        auto sectionName = getSectionName(targetSectionIndex);
        auto debugDataRelocation = isDebugDataRelocation(ConstStringRef(sectionName.c_str()));
        Relocations &relocs = debugDataRelocation ? debugInfoRelocations : relocations;

        auto reloc = reinterpret_cast<const ElfRel<numBits> *>(sectionHeaderData.data.begin());

        // there may be multiple rel sections, reserve additional size
        auto previousEntries = relocations.size();
        auto allEntries = previousEntries + numberOfEntries;
        relocs.reserve(allEntries);

        for (auto i = previousEntries; i < allEntries; i++) {
            int symbolIndex = extractSymbolIndex<ElfRel<numBits>>(*reloc);
            auto relocType = extractRelocType<ElfRel<numBits>>(*reloc);
            int symbolSectionIndex = symbolTable[symbolIndex].shndx;
            std::string name = std::string(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()) + symbolTable[symbolIndex].name);

            RelocationInfo relocInfo = {symbolSectionIndex, symbolIndex, targetSectionIndex, 0, reloc->offset, relocType, std::move(name)};

            relocs.push_back(relocInfo);
            reloc++;
        }
    }

    return true;
}

template <ElfIdentifierClass numBits>
bool Elf<numBits>::decodeSections(std::string &outError) {
    bool success = true;
    for (size_t i = 0; i < sectionHeaders.size(); i++) {
        success &= decodeSymTab(sectionHeaders[i], outError);
    }

    if (success) {
        for (size_t i = 0; i < sectionHeaders.size(); i++) {
            success &= decodeRelocations(sectionHeaders[i], outError);
        }
    }
    return success;
}

template <ElfIdentifierClass numBits>
bool Elf<numBits>::isDebugDataRelocation(ConstStringRef sectionName) {
    if (sectionName.startsWith(NEO::Elf::SpecialSectionNames::debug.data())) {
        return true;
    }
    return false;
}

template <>
template <class ElfReloc>
int Elf<EI_CLASS_32>::extractSymbolIndex(const ElfReloc &elfReloc) const {
    return static_cast<int>(elfReloc.info >> 8);
}

template <>
template <class ElfReloc>
int Elf<EI_CLASS_64>::extractSymbolIndex(const ElfReloc &elfReloc) const {
    return static_cast<int>(elfReloc.info >> 32);
}

template <>
template <class ElfReloc>
uint32_t Elf<EI_CLASS_32>::extractRelocType(const ElfReloc &elfReloc) const {
    return elfReloc.info & 0xff;
}

template <>
template <class ElfReloc>
uint32_t Elf<EI_CLASS_64>::extractRelocType(const ElfReloc &elfReloc) const {
    return elfReloc.info & 0xffffffff;
}

template bool Elf<EI_CLASS_64>::decodeSections(std::string &outError);

template int Elf<EI_CLASS_32>::extractSymbolIndex(const ElfRel<EI_CLASS_32> &elfReloc) const;
template int Elf<EI_CLASS_64>::extractSymbolIndex(const ElfRel<EI_CLASS_64> &elfReloc) const;
template int Elf<EI_CLASS_32>::extractSymbolIndex(const ElfRela<EI_CLASS_32> &elfReloc) const;
template int Elf<EI_CLASS_64>::extractSymbolIndex(const ElfRela<EI_CLASS_64> &elfReloc) const;

template uint32_t Elf<EI_CLASS_32>::extractRelocType(const ElfRel<EI_CLASS_32> &elfReloc) const;
template uint32_t Elf<EI_CLASS_64>::extractRelocType(const ElfRel<EI_CLASS_64> &elfReloc) const;
template uint32_t Elf<EI_CLASS_32>::extractRelocType(const ElfRela<EI_CLASS_32> &elfReloc) const;
template uint32_t Elf<EI_CLASS_64>::extractRelocType(const ElfRela<EI_CLASS_64> &elfReloc) const;

template Elf<EI_CLASS_32> decodeElf<EI_CLASS_32>(const ArrayRef<const uint8_t>, std::string &, std::string &);
template Elf<EI_CLASS_64> decodeElf<EI_CLASS_64>(const ArrayRef<const uint8_t>, std::string &, std::string &);

} // namespace Elf

} // namespace NEO
