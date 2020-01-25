/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/elf/elf_decoder.h"
#include "test.h"

using namespace NEO::Elf;

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
