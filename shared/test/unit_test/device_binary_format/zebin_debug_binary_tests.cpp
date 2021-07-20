/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/debug_zebin.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

#include "test.h"

TEST(DebugZebinTest, givenValidZebinThenDebugZebinIsGenerated) {
    MockElfEncoder<> elfEncoder;

    NEO::Debug::GPUSegments segments;
    uint8_t constData[8] = {0x1};
    uint8_t varData[8] = {0x2};
    uint8_t kernelISA[8] = {0x3};

    uint8_t debugInfo[0x20] = {0x0};
    uint8_t debugAbbrev[8] = {0x0};

    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "kernel", ArrayRef<const uint8_t>(kernelISA, sizeof(kernelISA)));
    auto kernelSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConst, ArrayRef<const uint8_t>(constData, sizeof(constData)));
    auto constDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataGlobal, ArrayRef<const uint8_t>(varData, sizeof(varData)));
    auto varDataSectionIndex = elfEncoder.getLastSectionHeaderIndex();
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

    segments.constData = {reinterpret_cast<uintptr_t>(constData), {constData, sizeof(constData)}};
    segments.varData = {reinterpret_cast<uintptr_t>(varData), {varData, sizeof(varData)}};
    segments.kernels.push_back({reinterpret_cast<uintptr_t>(kernelISA), {kernelISA, sizeof(kernelISA)}});
    segments.nameToSectIdMap["kernel"] = 0;
    auto zebinBin = elfEncoder.encode();

    std::string warning, error;
    auto zebin = NEO::Elf::decodeElf(zebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    auto debugZebinBin = NEO::Debug::getDebugZebin(zebinBin, segments);
    auto debugZebin = NEO::Elf::decodeElf(debugZebinBin, error, warning);
    ASSERT_TRUE(error.empty());
    ASSERT_TRUE(warning.empty());

    EXPECT_EQ(zebin.elfFileHeader->machine, debugZebin.elfFileHeader->machine);
    EXPECT_EQ(zebin.elfFileHeader->flags, debugZebin.elfFileHeader->flags);
    EXPECT_EQ(zebin.elfFileHeader->type, debugZebin.elfFileHeader->type);
    EXPECT_EQ(zebin.elfFileHeader->version, debugZebin.elfFileHeader->version);
    EXPECT_EQ(zebin.elfFileHeader->shStrNdx, debugZebin.elfFileHeader->shStrNdx);

    EXPECT_EQ(zebin.sectionHeaders.size(), debugZebin.sectionHeaders.size());

    uint64_t offsetKernel, offsetConstData, offsetVarData;
    offsetKernel = offsetConstData = offsetVarData = std::numeric_limits<uint64_t>::max();
    for (uint32_t i = 0; i < zebin.sectionHeaders.size(); ++i) {
        EXPECT_EQ(zebin.sectionHeaders[i].header->type, debugZebin.sectionHeaders[i].header->type);
        EXPECT_EQ(zebin.sectionHeaders[i].header->link, debugZebin.sectionHeaders[i].header->link);
        EXPECT_EQ(zebin.sectionHeaders[i].header->info, debugZebin.sectionHeaders[i].header->info);
        EXPECT_EQ(zebin.sectionHeaders[i].header->name, debugZebin.sectionHeaders[i].header->name);

        auto sectionName = debugZebin.getSectionName(i);
        auto refSectionName = NEO::ConstStringRef(sectionName);
        if (refSectionName.startsWith(NEO::Elf::SectionsNamesZebin::textPrefix.data())) {
            auto kernelName = sectionName.substr(NEO::Elf::SectionsNamesZebin::textPrefix.length());
            auto segmentIdIter = segments.nameToSectIdMap.find(kernelName);
            ASSERT_TRUE(segmentIdIter != segments.nameToSectIdMap.end());
            const auto &kernel = segments.kernels[segmentIdIter->second];

            EXPECT_EQ(kernel.data.size(), debugZebin.sectionHeaders[i].header->size);
            EXPECT_TRUE(memcmp(kernel.data.begin(), debugZebin.sectionHeaders[i].data.begin(), kernel.data.size()) == 0);
            offsetKernel = debugZebin.sectionHeaders[i].header->offset;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::dataConst) {
            EXPECT_EQ(segments.constData.data.size(), debugZebin.sectionHeaders[i].header->size);
            EXPECT_TRUE(memcmp(segments.constData.data.begin(), debugZebin.sectionHeaders[i].data.begin(), segments.constData.data.size()) == 0);
            offsetConstData = debugZebin.sectionHeaders[i].header->offset;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::dataGlobal) {
            EXPECT_EQ(segments.varData.data.size(), debugZebin.sectionHeaders[i].header->size);
            EXPECT_TRUE(memcmp(segments.varData.data.begin(), debugZebin.sectionHeaders[i].data.begin(), segments.varData.data.size()) == 0);
            offsetVarData = debugZebin.sectionHeaders[i].header->offset;
        } else if (refSectionName == NEO::Elf::SectionsNamesZebin::debugInfo) {
            EXPECT_EQ(zebin.sectionHeaders[i].header->size, debugZebin.sectionHeaders[i].header->size);

            auto ptrDebugInfo = debugZebin.sectionHeaders[i].data.begin();
            EXPECT_EQ(*reinterpret_cast<const uint64_t *>(ptrDebugInfo + debugRelocations[0].offset),
                      segments.kernels[0].gpuAddress + symbols[0].value + debugRelocations[0].addend);

            EXPECT_EQ(*reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[1].offset),
                      static_cast<uint32_t>((segments.constData.gpuAddress + symbols[1].value + debugRelocations[1].addend) & 0xffffffff));

            EXPECT_EQ(*reinterpret_cast<const uint32_t *>(ptrDebugInfo + debugRelocations[2].offset),
                      static_cast<uint32_t>(((segments.varData.gpuAddress + symbols[2].value + debugRelocations[2].addend) >> 32) & 0xffffffff));

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

    EXPECT_EQ(3U, debugZebin.programHeaders.size());
    EXPECT_EQ(segments.kernels[0].gpuAddress, static_cast<uintptr_t>(debugZebin.programHeaders[0].header->vAddr));
    EXPECT_EQ(segments.kernels[0].data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[0].header->fileSz));
    EXPECT_EQ(segments.kernels[0].data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[0].header->memSz));
    EXPECT_EQ(offsetKernel, static_cast<uintptr_t>(debugZebin.programHeaders[0].header->offset));

    EXPECT_EQ(segments.constData.gpuAddress, static_cast<uintptr_t>(debugZebin.programHeaders[1].header->vAddr));
    EXPECT_EQ(segments.constData.data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[1].header->fileSz));
    EXPECT_EQ(segments.constData.data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[1].header->memSz));
    EXPECT_EQ(offsetConstData, static_cast<uintptr_t>(debugZebin.programHeaders[1].header->offset));

    EXPECT_EQ(segments.varData.gpuAddress, static_cast<uintptr_t>(debugZebin.programHeaders[2].header->vAddr));
    EXPECT_EQ(segments.varData.data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[2].header->fileSz));
    EXPECT_EQ(segments.varData.data.size(), static_cast<uintptr_t>(debugZebin.programHeaders[2].header->memSz));
    EXPECT_EQ(offsetVarData, static_cast<uintptr_t>(debugZebin.programHeaders[2].header->offset));
}

TEST(DebugZebinTest, givenInvalidZebinThenDebugZebinIsNotGenerated) {
    uint8_t notZebin[] = {'N',
                          'O',
                          'T',
                          'E',
                          'L',
                          'F'};
    auto debugZebin = NEO::Debug::getDebugZebin(ArrayRef<const uint8_t>(notZebin, sizeof(notZebin)), {});
    EXPECT_EQ(0U, debugZebin.size());
}
