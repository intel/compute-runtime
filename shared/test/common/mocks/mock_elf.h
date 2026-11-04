/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

#include <unordered_map>

template <NEO::Elf::ElfIdentifierClass numBits = NEO::Elf::EI_CLASS_64>
struct MockElf : public NEO::Elf::Elf<numBits> {
    using BaseClass = NEO::Elf::Elf<numBits>;

    using BaseClass::relocations;
    using BaseClass::symbolTable;

    std::string getSectionName(uint32_t id) const override {
        if (overrideSectionNames) {
            return sectionNames.find(id)->second;
        }
        return NEO::Elf::Elf<numBits>::getSectionName(id);
    }

    std::string getSymbolName(uint32_t nameOffset) const override {
        if (overrideSymbolName) {
            return std::to_string(nameOffset);
        }
        return NEO::Elf::Elf<numBits>::getSymbolName(nameOffset);
    }

    using ElfSymT = NEO::Elf::ElfSymbolEntry<numBits>;
    void addSymbol(typename ElfSymT::Name name, typename ElfSymT::Value value, typename ElfSymT::Size size, typename ElfSymT::Shndx shndx,
                   typename ElfSymT::Info type, typename ElfSymT::Info binding) {
        ElfSymT sym{};
        sym.name = name;
        sym.value = value;
        sym.size = size;
        sym.shndx = shndx;
        sym.setType(type);
        sym.setBinding(binding);
        symbolTable.emplace_back(sym);
    }

    void addReloc(uint64_t offset, int64_t addend, uint32_t relocType, int targetSecIdx, int symIdx, NEO::ConstStringRef symbolName) {
        typename BaseClass::RelocationInfo reloc{};
        reloc.offset = offset;
        reloc.addend = addend;
        reloc.relocType = relocType;
        reloc.symbolName = symbolName.str();
        reloc.symbolTableIndex = symIdx;
        reloc.targetSectionIndex = targetSecIdx;
        relocations.emplace_back(reloc);
    }

    void setupSectionNames(std::unordered_map<uint32_t, std::string> map) {
        sectionNames = map;
        overrideSectionNames = true;
    }

    bool overrideSectionNames = false;
    std::unordered_map<uint32_t, std::string> sectionNames;
    bool overrideSymbolName = false;
};

template <NEO::Elf::ElfIdentifierClass numBits = NEO::Elf::EI_CLASS_64>
struct MockElfEncoder : public NEO::Elf::ElfEncoder<numBits> {
    using NEO::Elf::ElfEncoder<numBits>::data;
    using NEO::Elf::ElfEncoder<numBits>::sectionHeaders;

    uint32_t getLastSectionHeaderIndex() {
        return uint32_t(sectionHeaders.size()) - 1;
    }

    NEO::Elf::ElfSectionHeader<numBits> *getSectionHeader(uint32_t idx) {
        return sectionHeaders.data() + idx;
    }
};
