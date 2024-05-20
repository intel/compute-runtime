/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/elf_rewriter.h"

#include "gtest/gtest.h"

namespace NEO::Elf {

inline bool operator==(const ElfFileHeaderIdentity &lhs, const ElfFileHeaderIdentity &rhs) {
    return (lhs.abiVersion == rhs.abiVersion) && (lhs.data == rhs.data) && (lhs.eClass == rhs.eClass) && (lhs.osAbi == rhs.osAbi) && (lhs.version == rhs.version) && (lhs.magic[0] == rhs.magic[0]) && (lhs.magic[1] == rhs.magic[1]) && (lhs.magic[2] == rhs.magic[2]) && (lhs.magic[3] == rhs.magic[3]);
}

template <ElfIdentifierClass numBits>
inline bool operator==(const ElfFileHeader<numBits> &lhs, const ElfFileHeader<numBits> &rhs) {
    return (lhs.identity == rhs.identity) && (lhs.type == rhs.type) && (lhs.machine == rhs.machine) && (lhs.version == rhs.version) && (lhs.entry == rhs.entry) && (lhs.phOff == rhs.phOff) && (lhs.shOff == rhs.shOff) && (lhs.flags == rhs.flags) && (lhs.ehSize == rhs.ehSize) && (lhs.phEntSize == rhs.phEntSize) && (lhs.phNum == rhs.phNum) && (lhs.shEntSize == rhs.shEntSize) && (lhs.shNum == rhs.shNum) && (lhs.shStrNdx == rhs.shStrNdx);
}

template <ElfIdentifierClass numBits>
inline bool operator==(const SectionHeaderAndData<numBits> &lhs, const SectionHeaderAndData<numBits> &rhs) {
    return (lhs.header->name == rhs.header->name) && (lhs.header->type == rhs.header->type) && (lhs.header->flags == rhs.header->flags) && (lhs.header->addr == rhs.header->addr) && (lhs.header->offset == rhs.header->offset) && (lhs.header->size == rhs.header->size) && (lhs.header->link == rhs.header->link) && (lhs.header->info == rhs.header->info) && (lhs.header->addralign == rhs.header->addralign) && (lhs.header->entsize == rhs.header->entsize) && (lhs.data.size() == rhs.data.size()) && ((lhs.data.size() == 0) || (0 == memcmp(lhs.data.begin(), rhs.data.begin(), lhs.data.size())));
}

template <ElfIdentifierClass numBits>
inline bool operator==(const ProgramHeaderAndData<numBits> &lhs, const ProgramHeaderAndData<numBits> &rhs) {
    return (lhs.header->type == rhs.header->type) && (lhs.header->offset == rhs.header->offset) && (lhs.header->vAddr == rhs.header->vAddr) && (lhs.header->pAddr == rhs.header->pAddr) && (lhs.header->fileSz == rhs.header->fileSz) && (lhs.header->memSz == rhs.header->memSz) && (lhs.header->flags == rhs.header->flags) && (lhs.header->align == rhs.header->align) && (lhs.data.size() == rhs.data.size()) && ((lhs.data.size() == 0) || (0 == memcmp(lhs.data.begin(), rhs.data.begin(), lhs.data.size())));
}

} // namespace NEO::Elf

TEST(ElfRewriter, GivenElfWhenNotModifiedThenOutputIsSameAsSource) {
    NEO::Elf::ElfEncoder<> encoder;
    std::vector<uint8_t> txtData;
    txtData.resize(4096U, 7);
    auto &txtSection = encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt", txtData);
    encoder.appendProgramHeaderLoad(encoder.getSectionHeaderIndex(txtSection), 4096U, 4096U);
    auto elfBinSrc = encoder.encode();

    std::string err, warn;
    auto decodedElfSrc = NEO::Elf::decodeElf(elfBinSrc, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    NEO::Elf::ElfRewriter<> rewriter{decodedElfSrc};
    auto elfBinOut = rewriter.encode();

    ASSERT_EQ(elfBinSrc.size(), elfBinOut.size());
    auto decodedElfOut = NEO::Elf::decodeElf(elfBinOut, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    EXPECT_EQ(*decodedElfSrc.elfFileHeader, *decodedElfOut.elfFileHeader);
    ASSERT_EQ(decodedElfSrc.sectionHeaders.size(), decodedElfOut.sectionHeaders.size());
    for (size_t i = 0; i < decodedElfSrc.sectionHeaders.size(); ++i) {
        EXPECT_EQ(decodedElfSrc.sectionHeaders[i], decodedElfOut.sectionHeaders[i]);
    }
    ASSERT_EQ(decodedElfSrc.programHeaders.size(), decodedElfOut.programHeaders.size());
    for (size_t i = 0; i < decodedElfSrc.programHeaders.size(); ++i) {
        EXPECT_EQ(decodedElfSrc.programHeaders[i], decodedElfOut.programHeaders[i]);
    }
}

TEST(ElfRewriterFindSections, GivenSectionTypeAndNameThenReturnsMatchedSections) {
    NEO::Elf::ElfEncoder<> encoder;
    std::vector<uint8_t> txtData;
    txtData.resize(4096U, 7);
    encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt", txtData);
    encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txtX", txtData);
    encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt", txtData);
    encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_STRTAB, ".txt", txtData);
    auto elfBinSrc = encoder.encode();
    std::string err, warn;
    auto decodedElfSrc = NEO::Elf::decodeElf(elfBinSrc, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    NEO::Elf::ElfRewriter<> rewriter{decodedElfSrc};

    auto foundSections = rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt");
    ASSERT_EQ(2U, foundSections.size());
    EXPECT_EQ(NEO::Elf::SectionHeaderType::SHT_PROGBITS, rewriter.getSection(foundSections[0]).header.type);
    EXPECT_EQ(NEO::Elf::SectionHeaderType::SHT_PROGBITS, rewriter.getSection(foundSections[1]).header.type);
    EXPECT_EQ(".txt", rewriter.getSection(foundSections[0]).name);
    EXPECT_EQ(".txt", rewriter.getSection(foundSections[1]).name);
}

TEST(ElfRewriterRemoveSection, GivenSectionIndexThenRemovesThatSection) {
    NEO::Elf::ElfEncoder<> encoder;
    std::vector<uint8_t> txtData;
    txtData.resize(4096U, 7);
    auto &sectionWithLoad = encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.0", txtData);
    encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.1", txtData);
    encoder.appendProgramHeaderLoad(encoder.getSectionHeaderIndex(sectionWithLoad), 4096U, 4096U);
    auto elfBinSrc = encoder.encode();
    std::string err, warn;
    auto decodedElfSrc = NEO::Elf::decodeElf(elfBinSrc, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    NEO::Elf::ElfRewriter<> rewriter{decodedElfSrc};

    auto foundSections = rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.0");
    ASSERT_EQ(1U, foundSections.size());
    EXPECT_EQ(NEO::Elf::SectionHeaderType::SHT_PROGBITS, rewriter.getSection(foundSections[0]).header.type);
    EXPECT_EQ(".txt.0", rewriter.getSection(foundSections[0]).name);
    rewriter.removeSection(foundSections[0]);
    EXPECT_EQ(0U, rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.0").size());
    NEO::Elf::decodeElf(rewriter.encode(), err, warn);
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_TRUE(warn.empty()) << warn;

    foundSections = rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.1");
    ASSERT_EQ(1U, foundSections.size());
    EXPECT_EQ(NEO::Elf::SectionHeaderType::SHT_PROGBITS, rewriter.getSection(foundSections[0]).header.type);
    EXPECT_EQ(".txt.1", rewriter.getSection(foundSections[0]).name);
    rewriter.removeSection(foundSections[0]);
    EXPECT_EQ(0U, rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt.1").size());
    NEO::Elf::decodeElf(rewriter.encode(), err, warn);
    EXPECT_TRUE(err.empty()) << err;
    EXPECT_TRUE(warn.empty()) << warn;
}

TEST(ElfRewriter, GivenElfThenPreservesNamesOffsets) {
    NEO::Elf::ElfEncoder<> encoder;
    std::vector<uint8_t> txtData;
    txtData.resize(4096U, 7);
    auto name0Idx = encoder.appendSectionName("name0");
    auto &txtSection = encoder.appendSection(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt", txtData);
    auto name2Idx = encoder.appendSectionName("name2");
    encoder.appendProgramHeaderLoad(encoder.getSectionHeaderIndex(txtSection), 4096U, 4096U);
    auto elfBinSrc = encoder.encode();

    std::string err, warn;
    auto decodedElfSrc = NEO::Elf::decodeElf(elfBinSrc, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    NEO::Elf::ElfRewriter<> rewriter{decodedElfSrc};
    auto elfBinOut = rewriter.encode();

    ASSERT_EQ(elfBinSrc.size(), elfBinOut.size());
    auto decodedElfOut = NEO::Elf::decodeElf(elfBinOut, err, warn);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_TRUE(warn.empty()) << warn;
    EXPECT_EQ(1U, rewriter.findSections(NEO::Elf::SectionHeaderType::SHT_PROGBITS, ".txt").size());
    EXPECT_EQ("name0", decodedElfOut.getName(name0Idx));
    EXPECT_EQ("name2", decodedElfOut.getName(name2Idx));
}