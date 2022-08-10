/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO::Elf;

using ELF_CLASS = NEO::Elf::ELF_IDENTIFIER_CLASS;

class TestElf {
  public:
    TestElf() {
        memset(dummyData, 0, sizeof(dummyData));
    }

    std::vector<uint8_t> createRelocateableElfWithDebugData() {
        MockElfEncoder<> elfEncoder;

        elfEncoder.getElfFileHeader().type = ELF_TYPE::ET_REL;
        elfEncoder.getElfFileHeader().machine = ELF_MACHINE::EM_NONE;

        elfEncoder.appendSection(SHT_PROGBITS, SpecialSectionNames::text.str(), ArrayRef<const uint8_t>(dummyData, sizeof(dummyData)));
        auto textSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        ElfRela<ELF_CLASS::EI_CLASS_64> relocationsWithAddend[2];
        relocationsWithAddend[0].addend = relaAddend;
        relocationsWithAddend[0].info = relaSymbolIndexes[0] << 32 | uint32_t(RELOCATION_X8664_TYPE::R_X8664_64);
        relocationsWithAddend[0].offset = relaOffsets[0];

        relocationsWithAddend[1].addend = relaAddend;
        relocationsWithAddend[1].info = relaSymbolIndexes[1] << 32 | uint32_t(RELOCATION_X8664_TYPE::R_X8664_32);
        relocationsWithAddend[1].offset = relaOffsets[1];

        elfEncoder.appendSection(SHT_PROGBITS, SpecialSectionNames::debug, ArrayRef<const uint8_t>(dummyData, sizeof(dummyData)));
        auto debugSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        elfEncoder.appendSection(SHT_RELA, SpecialSectionNames::relaPrefix.str() + SpecialSectionNames::debug.str(),
                                 ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(relocationsWithAddend), sizeof(relocationsWithAddend)));
        auto relaDebugSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        auto relaDebugSection = elfEncoder.getSectionHeader(relaDebugSectionIndex);
        relaDebugSection->info = debugSectionIndex;

        ElfRel<ELF_CLASS::EI_CLASS_64> relocations[2];
        relocations[0].info = relSymbolIndex << 32 | uint64_t(RELOCATION_X8664_TYPE::R_X8664_64);
        relocations[0].offset = relOffsets[0];

        relocations[1].info = relSymbolIndex << 32 | uint64_t(RELOCATION_X8664_TYPE::R_X8664_64);
        relocations[1].offset = relOffsets[1];

        elfEncoder.appendSection(SHT_PROGBITS, SpecialSectionNames::line, std::string{"dummy_line_data______________________"});
        auto lineSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        elfEncoder.appendSection(SHT_REL, SpecialSectionNames::relPrefix.str() + SpecialSectionNames::line.str(),
                                 ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&relocations[0]), sizeof(relocations[0])));
        auto relLineSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        auto relLineSection = elfEncoder.getSectionHeader(relLineSectionIndex);
        relLineSection->info = lineSectionIndex;

        elfEncoder.appendSection(SHT_REL, SpecialSectionNames::relPrefix.str() + SpecialSectionNames::debug.str(),
                                 ArrayRef<const uint8_t>(reinterpret_cast<uint8_t *>(&relocations[1]), sizeof(relocations[1])));
        auto relDebugSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        auto relDebugSection = elfEncoder.getSectionHeader(relDebugSectionIndex);
        relDebugSection->info = debugSectionIndex;

        elfEncoder.appendSection(SHT_PROGBITS, SpecialSectionNames::data, std::string{"global_data_memory"});
        auto dataSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        symbolTable.resize(4 * sizeof(ElfSymbolEntry<ELF_CLASS::EI_CLASS_64>));

        symbols = reinterpret_cast<ElfSymbolEntry<ELF_CLASS::EI_CLASS_64> *>(symbolTable.data());
        symbols[0].name = 0; // undef
        symbols[0].info = 0;
        symbols[0].shndx = 0;
        symbols[0].size = 0;
        symbols[0].value = 0;

        symbols[1].name = elfEncoder.appendSectionName(NEO::ConstStringRef("local_function_symbol_1"));
        symbols[1].info = SYMBOL_TABLE_TYPE::STT_FUNC | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
        symbols[1].shndx = textSectionIndex;
        symbols[1].size = 8;
        symbols[1].value = 0;
        symbols[1].other = 0;

        symbols[2].name = elfEncoder.appendSectionName(NEO::ConstStringRef("section_symbol_2"));
        symbols[2].info = SYMBOL_TABLE_TYPE::STT_SECTION | SYMBOL_TABLE_BIND::STB_LOCAL << 4;
        symbols[2].shndx = textSectionIndex;
        symbols[2].size = 0;
        symbols[2].value = 0;
        symbols[2].other = 0;

        symbols[3].name = elfEncoder.appendSectionName(NEO::ConstStringRef("global_object_symbol_0"));
        symbols[3].info = SYMBOL_TABLE_TYPE::STT_OBJECT | SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
        symbols[3].shndx = dataSectionIndex;
        symbols[3].size = 4;
        symbols[3].value = 7; // offset to "data" string in data section
        symbols[3].other = 0;

        elfEncoder.appendSection(SHT_SYMTAB, SpecialSectionNames::symtab.str(), ArrayRef<const uint8_t>(symbolTable.data(), symbolTable.size()));
        auto symTabSectionIndex = elfEncoder.getLastSectionHeaderIndex();

        relaDebugSection->link = symTabSectionIndex;
        relLineSection->link = symTabSectionIndex;
        relDebugSection->link = symTabSectionIndex;

        auto symTabSectionHeader = elfEncoder.getSectionHeader(symTabSectionIndex);
        symTabSectionHeader->info = 4;                                          // one greater than last LOCAL symbol
        symTabSectionHeader->link = elfEncoder.getLastSectionHeaderIndex() + 1; //strtab section added as last
        return elfEncoder.encode();
    }
    const int64_t relaAddend = 16;
    const uint64_t relaSymbolIndexes[2] = {3, 1};
    const uint64_t relaOffsets[2] = {8, 24};

    const uint64_t relSymbolIndex = 2;
    const uint64_t relOffsets[2] = {16, 32};
    uint8_t dummyData[8 * 10];

    std::vector<uint8_t> symbolTable;
    ElfSymbolEntry<ELF_CLASS::EI_CLASS_64> *symbols = nullptr;
};

TEST(ElfDecoder, WhenEmptyDataThenElfHeaderDecodingFails) {
    ArrayRef<const uint8_t> empty;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_32>(empty));
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(empty));
}

TEST(ElfDecoder, WhenValidHaderThenElfHeaderDecodingSucceeds) {
    ElfFileHeader<EI_CLASS_64> header64;
    EXPECT_EQ(&header64, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header64, 1U)));

    ElfFileHeader<EI_CLASS_32> header32;
    EXPECT_EQ(&header32, decodeElfFileHeader<EI_CLASS_32>(ArrayRef<const uint8_t>::fromAny(&header32, 1U)));
}

TEST(ElfDecoder, WhenNotEngoughDataThenElfHeaderDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header64;
    auto header64Data = ArrayRef<const uint8_t>::fromAny(&header64, 1U);
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>(header64Data.begin(), header64Data.begin() + header64Data.size() - 1)));

    ElfFileHeader<EI_CLASS_32> header32;
    auto header32Data = ArrayRef<const uint8_t>::fromAny(&header32, 1U);
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_32>(ArrayRef<const uint8_t>(header32Data.begin(), header32Data.begin() + header32Data.size() - 1)));
}

TEST(ElfDecoder, WhenInvalidElfMagicThenElfHeaderDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header;
    header.identity.magic[0] = 5;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U)));

    header = ElfFileHeader<EI_CLASS_64>{};
    header.identity.magic[1] = 5;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U)));

    header = ElfFileHeader<EI_CLASS_64>{};
    header.identity.magic[2] = 5;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U)));

    header = ElfFileHeader<EI_CLASS_64>{};
    header.identity.magic[3] = 5;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U)));

    header = ElfFileHeader<EI_CLASS_64>{};
    EXPECT_EQ(&header, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U)));
}

TEST(ElfDecoder, WhenMismatchedClassThenElfHeaderDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header64;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_32>(ArrayRef<const uint8_t>::fromAny(&header64, 1U)));

    ElfFileHeader<EI_CLASS_32> header32;
    EXPECT_EQ(nullptr, decodeElfFileHeader<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header32, 1U)));
}

TEST(ElfDecoder, WhenNotElfThenCheckNumBitsReturnsClassNone) {
    ArrayRef<const uint8_t> empty;
    EXPECT_EQ(EI_CLASS_NONE, getElfNumBits(empty));
}

TEST(ElfDecoder, WhenValidElfThenCheckNumBitsReturnsProperClass) {
    ElfFileHeader<EI_CLASS_64> header64;
    EXPECT_EQ(EI_CLASS_64, getElfNumBits(ArrayRef<const uint8_t>::fromAny(&header64, 1U)));

    ElfFileHeader<EI_CLASS_32> header32;
    EXPECT_EQ(EI_CLASS_32, getElfNumBits(ArrayRef<const uint8_t>::fromAny(&header32, 1U)));
}

TEST(ElfDecoder, WhenNotElfThenIsElfReturnsFalse) {
    ArrayRef<const uint8_t> empty;
    EXPECT_FALSE(isElf(empty));
}

TEST(ElfDecoder, WhenValidElfThenIsElfReturnsTrue) {
    ElfFileHeader<EI_CLASS_64> header64;
    EXPECT_TRUE(isElf(ArrayRef<const uint8_t>::fromAny(&header64, 1U)));

    ElfFileHeader<EI_CLASS_32> header32;
    EXPECT_TRUE(isElf(ArrayRef<const uint8_t>::fromAny(&header32, 1U)));
}

TEST(ElfDecoder, WhenValidEmptyElfThenHeaderIsProperlyDecodedAndNoWarningsOrErrorsEmitted) {
    ElfFileHeader<EI_CLASS_64> header64;
    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header64, 1U), decodeErrors, decodeWarnings);
    EXPECT_EQ(&header64, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_TRUE(decodeWarnings.empty());

    decodeWarnings.clear();
    decodeErrors.clear();
    ElfFileHeader<EI_CLASS_32> header32;
    auto elf32 = decodeElf<EI_CLASS_32>(ArrayRef<const uint8_t>::fromAny(&header32, 1U), decodeErrors, decodeWarnings);
    EXPECT_EQ(&header32, elf32.elfFileHeader);
    EXPECT_TRUE(elf32.programHeaders.empty());
    EXPECT_TRUE(elf32.sectionHeaders.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenInvalidElfHeaderThenDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header;
    header.identity.magic[0] = 5;

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U), decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Invalid or missing ELF header", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenOutOfBoundsProgramHeaderTableOffsetThenDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header;
    header.phOff = sizeof(header) + 1;

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U), decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds program headers table", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenOutOfBoundsSectionHeaderTableOffsetThenDecodingFails) {
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = sizeof(header) + 1;

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(ArrayRef<const uint8_t>::fromAny(&header, 1U), decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds section headers table", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenValidProgramHeaderTableEntriesThenDecodingSucceedsAndNoWarningsOrErrorsEmitted) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.phOff = header.ehSize;
    header.phNum = 2;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfProgramHeader<EI_CLASS_64> programHeader0;
    programHeader0.fileSz = sizeof(programHeader0) * 2;
    programHeader0.offset = header.phOff;
    ElfProgramHeader<EI_CLASS_64> programHeader1;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader0), reinterpret_cast<const uint8_t *>(&programHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader1), reinterpret_cast<const uint8_t *>(&programHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<decltype(header) *>(storage.data()), elf64.elfFileHeader);
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_TRUE(decodeWarnings.empty());
    ASSERT_EQ(2U, elf64.programHeaders.size());
    EXPECT_EQ(reinterpret_cast<decltype(programHeader0) *>(storage.data() + header.phOff), elf64.programHeaders[0].header);
    EXPECT_EQ(reinterpret_cast<decltype(programHeader1) *>(storage.data() + header.phOff + header.phEntSize), elf64.programHeaders[1].header);
    EXPECT_TRUE(elf64.programHeaders[1].data.empty());
    EXPECT_FALSE(elf64.programHeaders[0].data.empty());
    EXPECT_EQ(storage.data() + programHeader0.offset, elf64.programHeaders[0].data.begin());
    EXPECT_EQ(programHeader0.fileSz, elf64.programHeaders[0].data.size());
}

TEST(ElfDecoder, WhenOutOfBoundsProgramHeaderTableEntriesThenDecodingFails) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.phOff = header.ehSize;
    header.phNum = 3;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfProgramHeader<EI_CLASS_64> programHeader0;
    ElfProgramHeader<EI_CLASS_64> programHeader1;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader0), reinterpret_cast<const uint8_t *>(&programHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader1), reinterpret_cast<const uint8_t *>(&programHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds program headers table", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenOutOfBoundsProgramHeaderDataThenDecodingFails) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.phOff = header.ehSize;
    header.phNum = 2;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfProgramHeader<EI_CLASS_64> programHeader0;
    programHeader0.fileSz = sizeof(programHeader0) * 2;
    programHeader0.offset = header.phOff;
    ElfProgramHeader<EI_CLASS_64> programHeader1;
    programHeader1.fileSz = sizeof(programHeader0) * 3;
    programHeader1.offset = header.phOff;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader0), reinterpret_cast<const uint8_t *>(&programHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&programHeader1), reinterpret_cast<const uint8_t *>(&programHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds program header offset/filesz, program header idx : 1", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenValidSectionHeaderTableEntriesThenDecodingSucceedsAndNoWarningsOrErrorsEmitted) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 2;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    sectionHeader0.size = sizeof(sectionHeader0) * 2;
    sectionHeader0.offset = header.shOff;
    ElfSectionHeader<EI_CLASS_64> sectionHeader1;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader1), reinterpret_cast<const uint8_t *>(&sectionHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<decltype(header) *>(storage.data()), elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_TRUE(decodeWarnings.empty());
    ASSERT_EQ(2U, elf64.sectionHeaders.size());
    EXPECT_EQ(reinterpret_cast<decltype(sectionHeader0) *>(storage.data() + header.shOff), elf64.sectionHeaders[0].header);
    EXPECT_EQ(reinterpret_cast<decltype(sectionHeader1) *>(storage.data() + header.shOff + header.shEntSize), elf64.sectionHeaders[1].header);
    EXPECT_TRUE(elf64.sectionHeaders[1].data.empty());
    EXPECT_FALSE(elf64.sectionHeaders[0].data.empty());
    EXPECT_EQ(storage.data() + sectionHeader0.offset, elf64.sectionHeaders[0].data.begin());
    EXPECT_EQ(sectionHeader0.size, elf64.sectionHeaders[0].data.size());
}

TEST(ElfDecoder, WhenOutOfBoundsSectionHeaderTableEntriesThenDecodingFails) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 3;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    ElfSectionHeader<EI_CLASS_64> sectionHeader1;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader1), reinterpret_cast<const uint8_t *>(&sectionHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds section headers table", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenOutOfBoundsSectionHeaderDataThenDecodingFails) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 2;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    sectionHeader0.size = sizeof(sectionHeader0) * 2;
    sectionHeader0.offset = header.shOff;
    ElfSectionHeader<EI_CLASS_64> sectionHeader1;
    sectionHeader1.size = sizeof(sectionHeader0) * 3;
    sectionHeader1.offset = header.shOff;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader1), reinterpret_cast<const uint8_t *>(&sectionHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Out of bounds section header offset/size, section header idx : 1", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenSectionDoesNotHaveDataInFileThenDataIsSetAsEmpty) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 2;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    sectionHeader0.size = sizeof(sectionHeader0) * 2;
    sectionHeader0.offset = header.shOff;
    ElfSectionHeader<EI_CLASS_64> sectionHeader1;
    sectionHeader1.size = sizeof(sectionHeader0) * 3;
    sectionHeader1.offset = header.shOff;
    sectionHeader1.type = SHT_NOBITS;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader1), reinterpret_cast<const uint8_t *>(&sectionHeader1 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<decltype(header) *>(storage.data()), elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_TRUE(decodeWarnings.empty());
    ASSERT_EQ(2U, elf64.sectionHeaders.size());
    EXPECT_EQ(reinterpret_cast<decltype(sectionHeader0) *>(storage.data() + header.shOff), elf64.sectionHeaders[0].header);
    EXPECT_EQ(reinterpret_cast<decltype(sectionHeader1) *>(storage.data() + header.shOff + header.shEntSize), elf64.sectionHeaders[1].header);
    EXPECT_TRUE(elf64.sectionHeaders[1].data.empty());
    EXPECT_FALSE(elf64.sectionHeaders[0].data.empty());
    EXPECT_EQ(storage.data() + sectionHeader0.offset, elf64.sectionHeaders[0].data.begin());
    EXPECT_EQ(sectionHeader0.size, elf64.sectionHeaders[0].data.size());
}

TEST(ElfDecoder, WhenElfContainsInvalidSymbolSectionHeaderThenDecodingFailsAndErrorIsEmitted) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 1;
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    sectionHeader0.type = SECTION_HEADER_TYPE::SHT_SYMTAB;
    sectionHeader0.size = sizeof(sectionHeader0);
    sectionHeader0.offset = header.shOff;
    sectionHeader0.entsize = sizeof(ElfSymbolEntry<EI_CLASS_64>) + 4; //invalid entSize

    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);

    EXPECT_EQ(nullptr, elf64.elfFileHeader);
    EXPECT_TRUE(elf64.programHeaders.empty());
    EXPECT_TRUE(elf64.sectionHeaders.empty());
    ASSERT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Invalid symbol table entries size - expected : 24, got : 28\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty());
}

TEST(ElfDecoder, WhenElfContainsInvalidRelocationSectionHeaderThenDecodingFailsAndErrorIsEmitted) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 1;

    ElfSectionHeader<EI_CLASS_64> sectionHeader0;

    sectionHeader0.size = sizeof(sectionHeader0);
    sectionHeader0.offset = header.shOff;

    SECTION_HEADER_TYPE types[] = {SECTION_HEADER_TYPE::SHT_REL, SECTION_HEADER_TYPE::SHT_RELA};
    size_t entSizes[] = {sizeof(ElfRel<EI_CLASS_64>) + 4, sizeof(ElfRela<EI_CLASS_64>) + 4};
    std::string errors[] = {"Invalid rel entries size - expected : ",
                            "Invalid rela entries size - expected : "};

    for (int i = 0; i < 2; i++) {
        sectionHeader0.type = types[i];
        sectionHeader0.entsize = entSizes[i];

        storage.clear();
        storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
        storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));

        std::string decodeWarnings;
        std::string decodeErrors;
        auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);

        EXPECT_EQ(nullptr, elf64.elfFileHeader);
        EXPECT_TRUE(elf64.programHeaders.empty());
        EXPECT_TRUE(elf64.sectionHeaders.empty());
        ASSERT_FALSE(decodeErrors.empty());
        EXPECT_TRUE(hasSubstr(decodeErrors, errors[i]));
        EXPECT_TRUE(decodeWarnings.empty());
    }
}

TEST(ElfDecoder, GivenElf64WhenExtractingDataFromElfRelocationThenCorrectRelocTypeAndSymbolIndexIsReturned) {
    ElfFileHeader<EI_CLASS_64> header64;
    Elf<EI_CLASS_64> elf = {};
    elf.elfFileHeader = &header64;

    ElfRela<EI_CLASS_64> rela;
    rela.info = decltype(ElfRela<EI_CLASS_64>::info)(RELOCATION_X8664_TYPE::R_X8664_64) | decltype(ElfRela<EI_CLASS_64>::info)(5) << 32;
    auto type = elf.extractRelocType(rela);
    auto symbolIndex = elf.extractSymbolIndex(rela);

    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_64), type);
    EXPECT_EQ(5, symbolIndex);

    ElfRel<EI_CLASS_64> rel;
    rel.info = decltype(ElfRela<EI_CLASS_64>::info)(RELOCATION_X8664_TYPE::R_X8664_32) | decltype(ElfRela<EI_CLASS_64>::info)(6) << 32;
    type = elf.extractRelocType(rel);
    symbolIndex = elf.extractSymbolIndex(rel);

    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_32), type);
    EXPECT_EQ(6, symbolIndex);
}

TEST(ElfDecoder, GivenElf32WhenExtractingDataFromElfRelocationThenCorrectRelocTypeAndSymbolIndexIsReturned) {
    ElfFileHeader<EI_CLASS_32> header64;
    Elf<EI_CLASS_32> elf = {};
    elf.elfFileHeader = &header64;

    ElfRela<EI_CLASS_32> rela;
    rela.info = decltype(ElfRela<EI_CLASS_32>::info)(RELOCATION_X8664_TYPE::R_X8664_32) | decltype(ElfRela<EI_CLASS_32>::info)(5) << 8;
    auto type = elf.extractRelocType(rela);
    auto symbolIndex = elf.extractSymbolIndex(rela);

    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_32), type);
    EXPECT_EQ(5, symbolIndex);

    ElfRel<EI_CLASS_32> rel;
    rel.info = decltype(ElfRel<EI_CLASS_32>::info)(RELOCATION_X8664_TYPE::R_X8664_32) | decltype(ElfRel<EI_CLASS_32>::info)(6) << 8;
    type = elf.extractRelocType(rel);
    symbolIndex = elf.extractSymbolIndex(rel);

    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_32), type);
    EXPECT_EQ(6, symbolIndex);
}

TEST(ElfDecoder, GivenElfWhenExtractingDataFromElfSymbolThenCorrectTypeAndBindIsReturned) {
    ElfFileHeader<EI_CLASS_64> header64;
    Elf<EI_CLASS_64> elf = {};
    elf.elfFileHeader = &header64;

    ElfSymbolEntry<EI_CLASS_64> symbolEntry;
    symbolEntry.info = SYMBOL_TABLE_TYPE::STT_OBJECT | SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    auto type = elf.extractSymbolType(symbolEntry);
    auto symbolBind = elf.extractSymbolBind(symbolEntry);

    EXPECT_EQ(SYMBOL_TABLE_TYPE::STT_OBJECT, type);
    EXPECT_EQ(SYMBOL_TABLE_BIND::STB_GLOBAL, symbolBind);
}

TEST(ElfDecoder, GivenElfWithStringTableSectionWhenGettingSectionNameThenCorrectNameIsReturned) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 3;
    header.shStrNdx = 2;

    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));
    ElfSectionHeader<EI_CLASS_64> sectionHeader0;
    sectionHeader0.size = sizeof(sectionHeader0) * 2;
    sectionHeader0.offset = header.shOff;

    ElfSectionHeader<EI_CLASS_64> sectionHeader1;
    sectionHeader1.size = 0;
    sectionHeader1.offset = header.shOff + sizeof(sectionHeader0);

    std::string_view strTab("\000123456789\0section0\0section1\0", 30);
    sectionHeader0.name = static_cast<uint32_t>(strTab.find("section0"));
    sectionHeader1.name = static_cast<uint32_t>(strTab.find("section1"));

    ElfSectionHeader<EI_CLASS_64> sectionHeaderStrTab;
    sectionHeaderStrTab.size = strTab.size();
    sectionHeaderStrTab.offset = header.shOff + 3 * sizeof(ElfSectionHeader<EI_CLASS_64>);

    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader0), reinterpret_cast<const uint8_t *>(&sectionHeader0 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeader1), reinterpret_cast<const uint8_t *>(&sectionHeader1 + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeaderStrTab), reinterpret_cast<const uint8_t *>(&sectionHeaderStrTab + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(strTab.data()), reinterpret_cast<const uint8_t *>(strTab.data() + strTab.size()));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);

    auto section0 = elf64.getSectionName(0);
    EXPECT_STREQ("section0", section0.c_str());
    auto section1 = elf64.getSectionName(1);
    EXPECT_STREQ("section1", section1.c_str());
}

TEST(ElfDecoder, GivenElfWithStringTableSectionWhenGettingSymbolNameThenCorrectNameIsReturned) {
    std::vector<uint8_t> storage;
    ElfFileHeader<EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 1;
    header.shStrNdx = 0;

    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&header), reinterpret_cast<const uint8_t *>(&header + 1));

    std::string_view strTab("abcdef_symbol\0", 15);

    ElfSectionHeader<EI_CLASS_64> sectionHeaderStrTab;
    sectionHeaderStrTab.size = strTab.size();
    sectionHeaderStrTab.offset = header.shOff + sizeof(ElfSectionHeader<EI_CLASS_64>);

    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&sectionHeaderStrTab), reinterpret_cast<const uint8_t *>(&sectionHeaderStrTab + 1));
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(strTab.data()), reinterpret_cast<const uint8_t *>(strTab.data() + strTab.size()));

    std::string decodeWarnings;
    std::string decodeErrors;
    auto elf64 = decodeElf<EI_CLASS_64>(storage, decodeErrors, decodeWarnings);

    auto nameOffset = static_cast<uint32_t>(strTab.find("symbol"));
    auto symbol = elf64.getSymbolName(nameOffset);
    EXPECT_STREQ("symbol", symbol.c_str());
}

TEST(ElfDecoder, WhenGettingSymbolAddressThenCorectValueIsReturned) {
    MockElf<EI_CLASS_64> elf;
    ElfSymbolEntry<EI_CLASS_64> symbol;
    symbol.info = SYMBOL_TABLE_TYPE::STT_OBJECT;
    symbol.name = 0;
    symbol.shndx = 1;
    symbol.size = 8;
    symbol.value = 0x1234000;

    elf.symbolTable.push_back(symbol);

    symbol.value = 0xfffff000;
    elf.symbolTable.push_back(symbol);

    auto address0 = elf.getSymbolValue(0);
    auto address1 = elf.getSymbolValue(1);

    EXPECT_EQ(0x1234000u, address0);
    EXPECT_EQ(0xfffff000u, address1);
}

TEST(ElfDecoder, GivenElfWithRelocationsWhenDecodedThenCorrectRelocationsAndSymolsAreExtracted) {
    std::string decodeWarnings;
    std::string decodeErrors;
    TestElf testElf;

    auto elfFile = testElf.createRelocateableElfWithDebugData();
    size_t size = elfFile.size();

    auto elf64 = decodeElf<EI_CLASS_64>(ArrayRef<uint8_t>(elfFile.data(), size), decodeErrors, decodeWarnings);
    EXPECT_NE(nullptr, elf64.elfFileHeader);

    auto relocations = elf64.getRelocations();
    auto debugRelocations = elf64.getDebugInfoRelocations();
    ASSERT_EQ(1u, relocations.size());
    ASSERT_EQ(3u, debugRelocations.size());

    EXPECT_EQ(testElf.relaAddend, debugRelocations[0].addend);
    EXPECT_EQ(testElf.relaOffsets[0], debugRelocations[0].offset);
    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_64), debugRelocations[0].relocType);
    EXPECT_STREQ("global_object_symbol_0", debugRelocations[0].symbolName.c_str());
    EXPECT_EQ(7, debugRelocations[0].symbolSectionIndex);
    EXPECT_EQ(3, debugRelocations[0].symbolTableIndex);
    EXPECT_EQ(2, debugRelocations[0].targetSectionIndex);

    EXPECT_EQ(testElf.relaAddend, debugRelocations[1].addend);
    EXPECT_EQ(testElf.relaOffsets[1], debugRelocations[1].offset);
    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_32), debugRelocations[1].relocType);
    EXPECT_STREQ("local_function_symbol_1", debugRelocations[1].symbolName.c_str());
    EXPECT_EQ(1, debugRelocations[1].symbolSectionIndex);
    EXPECT_EQ(1, debugRelocations[1].symbolTableIndex);
    EXPECT_EQ(2, debugRelocations[1].targetSectionIndex);

    EXPECT_EQ(0u, debugRelocations[2].addend);
    EXPECT_EQ(testElf.relOffsets[1], debugRelocations[2].offset);
    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_64), debugRelocations[2].relocType);
    EXPECT_STREQ("section_symbol_2", debugRelocations[2].symbolName.c_str());
    EXPECT_EQ(1, debugRelocations[2].symbolSectionIndex);
    EXPECT_EQ(2, debugRelocations[2].symbolTableIndex);
    EXPECT_EQ(2, debugRelocations[2].targetSectionIndex);

    EXPECT_EQ(0u, relocations[0].addend);
    EXPECT_EQ(testElf.relOffsets[0], relocations[0].offset);
    EXPECT_EQ(uint32_t(RELOCATION_X8664_TYPE::R_X8664_64), relocations[0].relocType);
    EXPECT_STREQ("section_symbol_2", relocations[0].symbolName.c_str());
    EXPECT_EQ(1, relocations[0].symbolSectionIndex);
    EXPECT_EQ(2, relocations[0].symbolTableIndex);
    EXPECT_EQ(4, relocations[0].targetSectionIndex);

    auto symbolTable = elf64.getSymbols();
    ASSERT_EQ(4u, symbolTable.size());

    EXPECT_EQ(SYMBOL_TABLE_TYPE::STT_NOTYPE, elf64.extractSymbolType(symbolTable[0]));
    EXPECT_EQ(SYMBOL_TABLE_BIND::STB_LOCAL, elf64.extractSymbolBind(symbolTable[0]));

    EXPECT_EQ(SYMBOL_TABLE_TYPE::STT_FUNC, elf64.extractSymbolType(symbolTable[1]));
    EXPECT_EQ(SYMBOL_TABLE_BIND::STB_LOCAL, elf64.extractSymbolBind(symbolTable[1]));

    EXPECT_EQ(SYMBOL_TABLE_TYPE::STT_SECTION, elf64.extractSymbolType(symbolTable[2]));
    EXPECT_EQ(SYMBOL_TABLE_BIND::STB_LOCAL, elf64.extractSymbolBind(symbolTable[2]));

    EXPECT_EQ(SYMBOL_TABLE_TYPE::STT_OBJECT, elf64.extractSymbolType(symbolTable[3]));
    EXPECT_EQ(SYMBOL_TABLE_BIND::STB_GLOBAL, elf64.extractSymbolBind(symbolTable[3]));
}
