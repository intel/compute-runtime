/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/debug_zebin.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

TEST(DebugZebinTest, givenValidZebinThenDebugZebinIsGenerated) {
    MockElfEncoder<> elfEncoder;

    uint8_t constData[8] = {0x1};
    uint8_t varData[8] = {0x2};
    uint8_t kernelISA[8] = {0x3};
    uint8_t stringData[8] = {0x4};

    uint8_t debugInfo[0x20] = {0x0};
    uint8_t debugAbbrev[8] = {0x0};

    using Segment = NEO::Debug::Segments::Segment;

    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "kernel", ArrayRef<const uint8_t>(kernelISA, sizeof(kernelISA)));
    auto kernelSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConst, ArrayRef<const uint8_t>(constData, sizeof(constData)));
    auto constDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataGlobal, ArrayRef<const uint8_t>(varData, sizeof(varData)));
    auto varDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConstString, ArrayRef<const uint8_t>(stringData, sizeof(stringData)));
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::debugInfo, ArrayRef<const uint8_t>(debugInfo, sizeof(debugInfo)));
    auto debugInfoSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::debugAbbrev, ArrayRef<const uint8_t>(debugAbbrev, sizeof(debugAbbrev)));
    auto debugAbbrevSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, std::string{});
    auto zeInfoSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, std::string{});

    typedef NEO::Elf::ElfSymbolEntry<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64> SymbolEntry;
    typedef NEO::Elf::ElfRela<NEO::Elf::ELF_IDENTIFIER_CLASS::EI_CLASS_64> Relocation;

    SymbolEntry symbols[5]{};
    symbols[0].name = elfEncoder.appendSectionName("kernel");
    symbols[0].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[0].shndx = static_cast<decltype(SymbolEntry::shndx)>(kernelSectionIndex);
    symbols[0].value = 0U;

    symbols[1].name = elfEncoder.appendSectionName("constData");
    symbols[1].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[1].shndx = static_cast<decltype(SymbolEntry::shndx)>(constDataSectionIndex);
    symbols[1].value = 0U;

    symbols[2].name = elfEncoder.appendSectionName("varData");
    symbols[2].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[2].shndx = static_cast<decltype(SymbolEntry::shndx)>(varDataSectionIndex);
    symbols[2].value = 0U;

    symbols[3].name = elfEncoder.appendSectionName("debugInfo");
    symbols[3].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[3].shndx = static_cast<decltype(SymbolEntry::shndx)>(debugAbbrevSectionIndex);
    symbols[3].value = 0x1U;

    symbols[4].name = elfEncoder.appendSectionName("zeInfo");
    symbols[4].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_SECTION | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[4].shndx = static_cast<decltype(SymbolEntry::shndx)>(zeInfoSectionIndex);
    symbols[4].value = 0U;

    Relocation debugRelocations[5]{};
    debugRelocations[0].addend = 0xabc;
    debugRelocations[0].offset = 0x0;
    debugRelocations[0].info = (uint64_t(0) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;

    debugRelocations[1].addend = 0x0;
    debugRelocations[1].offset = 0x8U;
    debugRelocations[1].info = (uint64_t(1) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32;

    debugRelocations[2].addend = 0x0;
    debugRelocations[2].offset = 0xCU;
    debugRelocations[2].info = (uint64_t(2) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32_HI;

    debugRelocations[3].addend = -0xa;
    debugRelocations[3].offset = 0x10U;
    debugRelocations[3].info = (uint64_t(3) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;

    // Will be ignored
    debugRelocations[4].addend = 0x0;
    debugRelocations[4].offset = 0x18U;
    debugRelocations[4].info = (uint64_t(4) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;

    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab, ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(symbols), sizeof(symbols)));
    auto &relaHeader = elfEncoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + NEO::Elf::SectionsNamesZebin::debugInfo.str(), ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(debugRelocations), sizeof(debugRelocations)));
    relaHeader.info = debugInfoSectionIndex;

    NEO::Debug::Segments segments;
    segments.constData = {0x10000000, 0x10000};
    segments.varData = {0x20000000, 0x20000};
    segments.stringData = {0x30000000, 0x30000};
    segments.nameToSegMap["kernel"] = {0x40000000, 0x50000};

    auto zebinBin = elfEncoder.encode();

    std::string warning, error;
    auto zebin = NEO::Elf::decodeElf(zebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    auto debugZebinBin = NEO::Debug::createDebugZebin(zebinBin, segments);
    auto debugZebin = NEO::Elf::decodeElf(debugZebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    EXPECT_EQ(zebin.elfFileHeader->machine, debugZebin.elfFileHeader->machine);
    EXPECT_EQ(zebin.elfFileHeader->flags, debugZebin.elfFileHeader->flags);
    EXPECT_EQ(NEO::Elf::ET_EXEC, debugZebin.elfFileHeader->type);
    EXPECT_EQ(zebin.elfFileHeader->version, debugZebin.elfFileHeader->version);
    EXPECT_EQ(zebin.elfFileHeader->shStrNdx, debugZebin.elfFileHeader->shStrNdx);

    EXPECT_EQ(zebin.sectionHeaders.size(), debugZebin.sectionHeaders.size());

    uint64_t offsetKernel, offsetConstData, offsetVarData, offsetStringData;
    uint64_t fileSzKernel, fileSzConstData, fileSzVarData, fileSzStringData;
    offsetKernel = offsetConstData = offsetVarData = offsetStringData = std::numeric_limits<uint64_t>::max();
    fileSzKernel = fileSzConstData = fileSzVarData = fileSzStringData = 0;
    for (uint32_t i = 0; i < zebin.sectionHeaders.size(); ++i) {
        EXPECT_EQ(zebin.sectionHeaders[i].header->type, debugZebin.sectionHeaders[i].header->type);
        EXPECT_EQ(zebin.sectionHeaders[i].header->link, debugZebin.sectionHeaders[i].header->link);
        EXPECT_EQ(zebin.sectionHeaders[i].header->info, debugZebin.sectionHeaders[i].header->info);
        EXPECT_EQ(zebin.sectionHeaders[i].header->name, debugZebin.sectionHeaders[i].header->name);
        EXPECT_EQ(zebin.sectionHeaders[i].header->flags, debugZebin.sectionHeaders[i].header->flags);

        auto sectionName = debugZebin.getSectionName(i);
        auto refSectionName = NEO::ConstStringRef(sectionName);
        if (refSectionName.startsWith(NEO::Elf::SectionsNamesZebin::textPrefix.data())) {
            offsetKernel = debugZebin.sectionHeaders[i].header->offset;
            fileSzKernel = debugZebin.sectionHeaders[i].header->size;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::dataConst) {
            offsetConstData = debugZebin.sectionHeaders[i].header->offset;
            fileSzConstData = debugZebin.sectionHeaders[i].header->size;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::dataGlobal) {
            offsetVarData = debugZebin.sectionHeaders[i].header->offset;
            fileSzVarData = debugZebin.sectionHeaders[i].header->size;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::dataConstString) {
            offsetStringData = debugZebin.sectionHeaders[i].header->offset;
            fileSzStringData = debugZebin.sectionHeaders[i].header->size;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::debugInfo) {
            auto ptrDebugInfo = debugZebin.sectionHeaders[i].data.begin();
            EXPECT_EQ(*reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[0].offset),
                      segments.nameToSegMap["kernel"].address + symbols[0].value + debugRelocations[0].addend);

            EXPECT_EQ(*reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[1].offset),
                      static_cast<uint32_t>((segments.constData.address + symbols[1].value + debugRelocations[1].addend) & 0xffffffff));

            EXPECT_EQ(*reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[2].offset),
                      static_cast<uint32_t>(((segments.varData.address + symbols[2].value + debugRelocations[2].addend) >> 32) & 0xffffffff));

            // debug symbols are not offseted
            EXPECT_EQ(*reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[3].offset),
                      symbols[3].value + debugRelocations[3].addend);

            // if symbols points to other sections relocation is skipped
            EXPECT_EQ(*reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[4].offset), 0U);
        } else {
            EXPECT_EQ(zebin.sectionHeaders[i].header->size, debugZebin.sectionHeaders[i].header->size);
            if (debugZebin.sectionHeaders[i].header->size > 0U) {
                EXPECT_TRUE(memcmp(zebin.sectionHeaders[i].data.begin(), debugZebin.sectionHeaders[i].data.begin(), debugZebin.sectionHeaders[i].data.size()) == 0);
            }
        }
    }

    std::vector<std::tuple<Segment, uint64_t, uint64_t>> segmentsSortedByAddr = {{segments.constData, offsetConstData, fileSzConstData},
                                                                                 {segments.varData, offsetVarData, fileSzVarData},
                                                                                 {segments.stringData, offsetStringData, fileSzStringData},
                                                                                 {segments.nameToSegMap["kernel"], offsetKernel, fileSzKernel}};
    std::sort(segmentsSortedByAddr.begin(), segmentsSortedByAddr.end(), [](auto seg1, auto seg2) { return std::get<0>(seg1).address < std::get<0>(seg2).address; });

    EXPECT_EQ(4U, debugZebin.programHeaders.size());
    for (size_t i = 0; i < 4U; i++) {
        auto &segment = std::get<0>(segmentsSortedByAddr[i]);
        auto &offset = std::get<1>(segmentsSortedByAddr[i]);
        auto &fileSz = std::get<2>(segmentsSortedByAddr[i]);
        auto &ph = debugZebin.programHeaders[i].header;

        EXPECT_EQ(segment.address, static_cast<uintptr_t>(ph->vAddr));
        EXPECT_EQ(segment.size, static_cast<uintptr_t>(ph->memSz));
        EXPECT_EQ(offset, static_cast<uintptr_t>(ph->offset));
        EXPECT_EQ(fileSz, static_cast<size_t>(ph->fileSz));
    }
}

TEST(DebugZebinTest, givenInvalidZebinThenDebugZebinIsNotGenerated) {
    uint8_t notZebin[] = {'N',
                          'O',
                          'T',
                          'E',
                          'L',
                          'F'};
    auto debugZebin = NEO::Debug::createDebugZebin(ArrayRef<const uint8_t>(notZebin, sizeof(notZebin)), {});
    EXPECT_EQ(0U, debugZebin.size());
}
