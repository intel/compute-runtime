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

using namespace NEO::Elf;
TEST(DebugZebinTest, givenValidZebinThenDebugZebinIsGenerated) {
    MockElfEncoder<> elfEncoder;

    uint8_t constData[8] = {0x1};
    uint8_t varData[8] = {0x2};
    uint8_t kernelISA[8] = {0x3};
    uint8_t stringData[8] = {0x4};

    uint8_t debugInfo[0x30] = {0x22};
    uint8_t debugAbbrev[8] = {0x0};

    using Segment = NEO::Debug::Segments::Segment;

    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::textPrefix.str() + "kernel", ArrayRef<const uint8_t>(kernelISA, sizeof(kernelISA)));
    auto kernelSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::dataConst, ArrayRef<const uint8_t>(constData, sizeof(constData)));
    auto constDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::dataGlobal, ArrayRef<const uint8_t>(varData, sizeof(varData)));
    auto varDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::dataConstString, ArrayRef<const uint8_t>(stringData, sizeof(stringData)));
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::debugInfo, ArrayRef<const uint8_t>(debugInfo, sizeof(debugInfo)));
    auto debugInfoSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::debugAbbrev, ArrayRef<const uint8_t>(debugAbbrev, sizeof(debugAbbrev)));
    auto debugAbbrevSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_ZEBIN_ZEINFO, SectionsNamesZebin::zeInfo, std::string{});
    auto zeInfoSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(SHT_ZEBIN_SPIRV, SectionsNamesZebin::spv, std::string{});

    using SymbolEntry = ElfSymbolEntry<ELF_IDENTIFIER_CLASS::EI_CLASS_64>;
    using Relocation = ElfRela<ELF_IDENTIFIER_CLASS::EI_CLASS_64>;

    SymbolEntry symbols[7]{};
    symbols[0].name = elfEncoder.appendSectionName("kernel");
    symbols[0].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[0].shndx = static_cast<decltype(SymbolEntry::shndx)>(kernelSectionIndex);
    symbols[0].value = 0U;

    symbols[1].name = elfEncoder.appendSectionName("constData");
    symbols[1].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[1].shndx = static_cast<decltype(SymbolEntry::shndx)>(constDataSectionIndex);
    symbols[1].value = 0U;

    symbols[2].name = elfEncoder.appendSectionName("varData");
    symbols[2].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[2].shndx = static_cast<decltype(SymbolEntry::shndx)>(varDataSectionIndex);
    symbols[2].value = 0U;

    symbols[3].name = elfEncoder.appendSectionName("debugInfo");
    symbols[3].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[3].shndx = static_cast<decltype(SymbolEntry::shndx)>(debugAbbrevSectionIndex);
    symbols[3].value = 0x1U;

    symbols[4].name = elfEncoder.appendSectionName("zeInfo");
    symbols[4].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[4].shndx = static_cast<decltype(SymbolEntry::shndx)>(zeInfoSectionIndex);
    symbols[4].value = 0U;

    symbols[5].name = elfEncoder.appendSectionName(SectionsNamesZebin::textPrefix.str() + "kernel");
    symbols[5].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[5].shndx = static_cast<decltype(SymbolEntry::shndx)>(debugInfoSectionIndex);
    symbols[5].value = 0U;

    symbols[6].name = elfEncoder.appendSectionName("kernel_payload_offset");
    symbols[6].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbols[6].shndx = static_cast<decltype(SymbolEntry::shndx)>(kernelSectionIndex);
    symbols[6].value = 0x10U;

    Relocation debugRelocations[7]{};
    debugRelocations[0].addend = 0xabc;
    debugRelocations[0].offset = 0x0;
    debugRelocations[0].setSymbolTableIndex(0);
    debugRelocations[0].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR);

    debugRelocations[1].addend = 0x0;
    debugRelocations[1].offset = 0x8U;
    debugRelocations[1].setSymbolTableIndex(1);
    debugRelocations[1].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32);

    debugRelocations[2].addend = 0x0;
    debugRelocations[2].offset = 0xCU;
    debugRelocations[2].setSymbolTableIndex(2);
    debugRelocations[2].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32_HI);

    debugRelocations[3].addend = -0xa;
    debugRelocations[3].offset = 0x10U;
    debugRelocations[3].setSymbolTableIndex(3);
    debugRelocations[3].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR);

    // Will be ignored
    debugRelocations[4].addend = 0x0;
    debugRelocations[4].offset = 0x18U;
    debugRelocations[4].setSymbolTableIndex(4);
    debugRelocations[4].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR);

    debugRelocations[5].addend = 0x0;
    debugRelocations[5].offset = 0x20U;
    debugRelocations[5].setSymbolTableIndex(5);
    debugRelocations[5].setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR);

    // Will be ignored due to reloc type
    debugRelocations[6].addend = 0x0;
    debugRelocations[6].offset = 0x28;
    debugRelocations[6].setSymbolTableIndex(6);
    debugRelocations[6].setRelocationType(RELOC_TYPE_ZEBIN::R_PER_THREAD_PAYLOAD_OFFSET);

    elfEncoder.appendSection(SHT_SYMTAB, SectionsNamesZebin::symtab, ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(symbols), sizeof(symbols)));
    auto &relaHeader = elfEncoder.appendSection(SHT_RELA, SpecialSectionNames::relaPrefix.str() + SectionsNamesZebin::debugInfo.str(), ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(debugRelocations), sizeof(debugRelocations)));
    relaHeader.info = debugInfoSectionIndex;

    NEO::Debug::Segments segments;
    segments.constData = {0x10000000, 0x10000};
    segments.varData = {0x20000000, 0x20000};
    segments.stringData = {0x30000000, 0x30000};
    segments.nameToSegMap["kernel"] = {0x40000000, 0x50000};

    auto zebinBin = elfEncoder.encode();

    std::string warning, error;
    auto zebin = decodeElf(zebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    auto debugZebinBin = NEO::Debug::createDebugZebin(zebinBin, segments);
    auto debugZebin = decodeElf(debugZebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    EXPECT_EQ(zebin.elfFileHeader->machine, debugZebin.elfFileHeader->machine);
    EXPECT_EQ(zebin.elfFileHeader->flags, debugZebin.elfFileHeader->flags);
    EXPECT_EQ(ET_EXEC, debugZebin.elfFileHeader->type);
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

        const auto &sectionHeader = debugZebin.sectionHeaders[i].header;
        const auto &sectionData = debugZebin.sectionHeaders[i].data;
        const auto sectionName = debugZebin.getSectionName(i);
        auto refSectionName = NEO::ConstStringRef(sectionName);
        if (refSectionName.startsWith(SectionsNamesZebin::textPrefix.data())) {
            offsetKernel = sectionHeader->offset;
            fileSzKernel = sectionHeader->size;
            EXPECT_EQ(segments.nameToSegMap["kernel"].address, sectionHeader->addr);
        } else if (refSectionName == SectionsNamesZebin::dataConst) {
            offsetConstData = sectionHeader->offset;
            fileSzConstData = sectionHeader->size;
            EXPECT_EQ(segments.constData.address, sectionHeader->addr);
        } else if (refSectionName == SectionsNamesZebin::dataGlobal) {
            offsetVarData = sectionHeader->offset;
            fileSzVarData = sectionHeader->size;
            EXPECT_EQ(segments.varData.address, sectionHeader->addr);
        } else if (refSectionName == SectionsNamesZebin::dataConstString) {
            offsetStringData = sectionHeader->offset;
            fileSzStringData = sectionHeader->size;
            EXPECT_EQ(segments.stringData.address, sectionHeader->addr);
        } else if (refSectionName == SectionsNamesZebin::debugInfo) {
            auto ptrDebugInfo = sectionData.begin();
            EXPECT_EQ(segments.nameToSegMap["kernel"].address + symbols[0].value + debugRelocations[0].addend,
                      *reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[0].offset));

            EXPECT_EQ(static_cast<uint32_t>((segments.constData.address + symbols[1].value + debugRelocations[1].addend) & 0xffffffff),
                      *reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[1].offset));

            EXPECT_EQ(static_cast<uint32_t>(((segments.varData.address + symbols[2].value + debugRelocations[2].addend) >> 32) & 0xffffffff),
                      *reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[2].offset));

            // debug symbols with name different than text segments are not offseted
            EXPECT_EQ(symbols[3].value + debugRelocations[3].addend,
                      *reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[3].offset));

            // if symbols points to other sections relocation is skipped - not text, data, debug
            EXPECT_EQ(*reinterpret_cast<uint64_t *>(debugInfo + debugRelocations[4].offset),
                      *reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[4].offset));

            // debug symbols with text segment name are offseted by corresponding segment's address
            EXPECT_EQ(segments.nameToSegMap["kernel"].address,
                      *reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[5].offset));

            EXPECT_EQ(*reinterpret_cast<uint64_t *>(debugInfo + debugRelocations[6].offset),
                      *reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[6].offset));
        } else if (refSectionName == SectionsNamesZebin::symtab) {
            using ElfSymbolT = ElfSymbolEntry<EI_CLASS_64>;
            size_t symbolCount = static_cast<size_t>(sectionHeader->size) / static_cast<size_t>(sectionHeader->entsize);
            ArrayRef<const ElfSymbolT> debugSymbols = {reinterpret_cast<const ElfSymbolT *>(sectionData.begin()), symbolCount};

            EXPECT_EQ(7U, debugSymbols.size());
            EXPECT_EQ(segments.nameToSegMap["kernel"].address + symbols[0].value, debugSymbols[0].value);
            EXPECT_EQ(segments.constData.address + symbols[1].value, debugSymbols[1].value);
            EXPECT_EQ(segments.varData.address + symbols[2].value, debugSymbols[2].value);
            EXPECT_EQ(symbols[3].value, debugSymbols[3].value);
            EXPECT_EQ(symbols[4].value, debugSymbols[4].value);
            EXPECT_EQ(segments.nameToSegMap["kernel"].address + symbols[5].value, debugSymbols[5].value);
            EXPECT_EQ(segments.nameToSegMap["kernel"].address + symbols[6].value, debugSymbols[6].value);

        } else {
            EXPECT_EQ(zebin.sectionHeaders[i].header->size, sectionHeader->size);
            if (sectionHeader->size > 0U) {
                EXPECT_TRUE(memcmp(zebin.sectionHeaders[i].data.begin(), sectionData.begin(), sectionData.size()) == 0);
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

TEST(DebugZebinTest, givenSymTabShndxUndefinedThenDoNotApplyRelocations) {
    MockElfEncoder<> elfEncoder;

    uint8_t kernelISA[8] = {0U};
    elfEncoder.appendSection(SHT_PROGBITS, SectionsNamesZebin::textPrefix.str() + "kernel", ArrayRef<const uint8_t>(kernelISA, sizeof(kernelISA)));
    auto kernelSectionIndex = elfEncoder.getLastSectionHeaderIndex();

    using SymbolEntry = ElfSymbolEntry<ELF_IDENTIFIER_CLASS::EI_CLASS_64>;
    using Relocation = ElfRela<ELF_IDENTIFIER_CLASS::EI_CLASS_64>;

    SymbolEntry symbol{};
    symbol.name = elfEncoder.appendSectionName("kernel");
    symbol.info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbol.shndx = static_cast<decltype(SymbolEntry::shndx)>(kernelSectionIndex);
    symbol.value = 0xAU;

    Relocation relocation{};
    relocation.addend = 0x0;
    relocation.offset = 0x0;
    relocation.setSymbolTableIndex(0);
    relocation.setRelocationType(RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR);

    elfEncoder.appendSection(SHT_SYMTAB, SectionsNamesZebin::symtab, ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&symbol), sizeof(symbol)));
    auto &relaHeader = elfEncoder.appendSection(SHT_RELA, SpecialSectionNames::relaPrefix.str() + SectionsNamesZebin::textPrefix.str() + "kernel", ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&relocation), sizeof(relocation)));
    relaHeader.info = kernelSectionIndex;

    auto zebin = elfEncoder.encode();
    std::string errors, warnings;
    auto zebinElf = decodeElf(zebin, errors, warnings);
    ASSERT_TRUE(errors.empty());
    ASSERT_TRUE(warnings.empty());

    class MockDebugZebinCreator : public NEO::Debug::DebugZebinCreator {
      public:
        using Base = NEO::Debug::DebugZebinCreator;
        using Base::Base;
        using Base::debugZebin;
        using Base::symTabShndx;
    };
    NEO::Debug::Segments segments;
    MockDebugZebinCreator dzc(zebinElf, segments);
    dzc.debugZebin = zebin;
    dzc.symTabShndx = std::numeric_limits<uint32_t>::max();
    dzc.applyRelocations();
    EXPECT_EQ(0U, *reinterpret_cast<uint64_t *>(zebin.data() + zebinElf.sectionHeaders[1].header->offset + relocation.offset));
}
