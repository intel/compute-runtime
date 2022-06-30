/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

template <NEO::Elf::ELF_IDENTIFIER_CLASS NumBits = NEO::Elf::EI_CLASS_64>
struct MockElf : public NEO::Elf::Elf<NumBits> {
    using BaseClass = NEO::Elf::Elf<NumBits>;

    using BaseClass::debugInfoRelocations;
    using BaseClass::relocations;
    using BaseClass::symbolTable;

    std::string getSectionName(uint32_t id) const override {
        if (overrideSectionNames) {
            return sectionNames.find(id)->second;
        }
        return NEO::Elf::Elf<NumBits>::getSectionName(id);
    }

    std::string getSymbolName(uint32_t nameOffset) const override {
        if (overrideSymbolName) {
            return std::to_string(nameOffset);
        }
        return NEO::Elf::Elf<NumBits>::getSymbolName(nameOffset);
    }

    void setupSecionNames(std::unordered_map<uint32_t, std::string> map) {
        sectionNames = map;
        overrideSectionNames = true;
    }

    bool overrideSectionNames = false;
    std::unordered_map<uint32_t, std::string> sectionNames;
    bool overrideSymbolName = false;
};

template <NEO::Elf::ELF_IDENTIFIER_CLASS NumBits = NEO::Elf::EI_CLASS_64>
struct MockElfEncoder : public NEO::Elf::ElfEncoder<NumBits> {
    using NEO::Elf::ElfEncoder<NumBits>::sectionHeaders;

    uint32_t getLastSectionHeaderIndex() {
        return uint32_t(sectionHeaders.size()) - 1;
    }

    NEO::Elf::ElfSectionHeader<NumBits> *getSectionHeader(uint32_t idx) {
        return sectionHeaders.data() + idx;
    }

    static std::vector<uint8_t> createRelocateableDebugDataElf() {
        MockElfEncoder<> elfEncoder;

        elfEncoder.getElfFileHeader().type = NEO::Elf::ELF_TYPE::ET_REL;
        elfEncoder.getElfFileHeader().machine = NEO::Elf::ELF_MACHINE::EM_NONE;

        uint8_t dummyData[16];
        elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SpecialSectionNames::text.str(), ArrayRef<const uint8_t>(dummyData, sizeof(dummyData)));
        auto textSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        NEO::Elf::ElfRela<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64> relocationsWithAddend;
        relocationsWithAddend.addend = 0x1a8;
        relocationsWithAddend.info = static_cast<decltype(relocationsWithAddend.info)>(textSectionIndex) << 32 | uint32_t(NEO::Elf::RELOCATION_X8664_TYPE::R_X8664_64);
        relocationsWithAddend.offset = 8;

        elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SpecialSectionNames::debugInfo, ArrayRef<const uint8_t>(dummyData, sizeof(dummyData)));
        auto debugSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::ConstStringRef(NEO::Elf::SpecialSectionNames::debug.str() + "_line"), ArrayRef<const uint8_t>(dummyData, sizeof(dummyData)));
        auto debugLineSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        elfEncoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + NEO::Elf::SpecialSectionNames::debugInfo.str(),
                                 ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&relocationsWithAddend), sizeof(relocationsWithAddend)));
        auto relaDebugSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        auto relaDebugSection = elfEncoder.getSectionHeader(relaDebugSectionIndex);
        relaDebugSection->info = debugSectionIndex;

        relocationsWithAddend.addend = 0;
        relocationsWithAddend.info = static_cast<decltype(relocationsWithAddend.info)>(textSectionIndex) << 32 | uint32_t(NEO::Elf::RELOCATION_X8664_TYPE::R_X8664_64);
        relocationsWithAddend.offset = 0;

        elfEncoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + NEO::Elf::SpecialSectionNames::debug.str() + "_line",
                                 ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&relocationsWithAddend), sizeof(relocationsWithAddend)));
        relaDebugSectionIndex = elfEncoder.getLastSectionHeaderIndex();
        auto relaDebugLineSection = elfEncoder.getSectionHeader(relaDebugSectionIndex);
        relaDebugLineSection->info = debugLineSectionIndex;

        std::vector<uint8_t> symbolTable;
        symbolTable.resize(2 * sizeof(NEO::Elf::ElfSymbolEntry<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64>));

        auto symbols = reinterpret_cast<NEO::Elf::ElfSymbolEntry<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64> *>(symbolTable.data());
        symbols[0].name = 0; // undef
        symbols[0].info = 0;
        symbols[0].shndx = 0;
        symbols[0].size = 0;
        symbols[0].value = 0;

        symbols[1].name = elfEncoder.appendSectionName(NEO::ConstStringRef(".text"));
        symbols[1].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
        symbols[1].shndx = static_cast<decltype(symbols[1].shndx)>(textSectionIndex);
        symbols[1].size = 0;
        symbols[1].value = 0;
        symbols[1].other = 0;

        elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SpecialSectionNames::symtab.str(), ArrayRef<const uint8_t>(symbolTable.data(), symbolTable.size()));
        auto symTabSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        relaDebugSection->link = symTabSectionIndex;
        relaDebugLineSection->link = symTabSectionIndex;

        auto symTabSectionHeader = elfEncoder.getSectionHeader(symTabSectionIndex);
        symTabSectionHeader->info = 2;
        symTabSectionHeader->link = elfEncoder.getLastSectionHeaderIndex() + 1; //strtab section added as last
        return elfEncoder.encode();
    }
};
