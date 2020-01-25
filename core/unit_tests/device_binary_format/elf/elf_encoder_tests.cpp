/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/elf/elf_encoder.h"
#include "core/utilities/range.h"
#include "test.h"

using namespace NEO::Elf;

TEST(ElfEncoder, WhenEmptyDataThenEncodedElfContainsOnlyHeader) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    auto &identity64 = header64.identity;
    EXPECT_EQ(elfMagic[0], identity64.magic[0]);
    EXPECT_EQ(elfMagic[1], identity64.magic[1]);
    EXPECT_EQ(elfMagic[2], identity64.magic[2]);
    EXPECT_EQ(elfMagic[3], identity64.magic[3]);
    EXPECT_EQ(EI_CLASS_64, identity64.eClass);
    EXPECT_EQ(EI_DATA_LITTLE_ENDIAN, identity64.data);
    EXPECT_EQ(EV_CURRENT, identity64.version);
    EXPECT_EQ(0U, identity64.osAbi);
    EXPECT_EQ(0U, identity64.abiVersion);
    EXPECT_EQ(ET_NONE, header64.type);
    EXPECT_EQ(EM_NONE, header64.machine);
    EXPECT_EQ(1U, header64.version);
    EXPECT_EQ(0U, header64.entry);
    EXPECT_EQ(0U, header64.phOff);
    EXPECT_EQ(0U, header64.shOff);
    EXPECT_EQ(0U, header64.flags);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>), header64.ehSize);
    EXPECT_EQ(sizeof(ElfProgramHeader<EI_CLASS_64>), header64.phEntSize);
    EXPECT_EQ(0U, header64.phNum);
    EXPECT_EQ(sizeof(ElfSectionHeader<EI_CLASS_64>), header64.shEntSize);
    EXPECT_EQ(0U, header64.shNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

    ElfEncoder<EI_CLASS_32> elfEncoder32(false, false);
    auto elfData32 = elfEncoder32.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_32>), elfData32.size());
    auto &header32 = *reinterpret_cast<ElfFileHeader<EI_CLASS_32> *>(elfData32.data());
    auto &identity32 = header32.identity;
    EXPECT_EQ(elfMagic[0], identity32.magic[0]);
    EXPECT_EQ(elfMagic[1], identity32.magic[1]);
    EXPECT_EQ(elfMagic[2], identity32.magic[2]);
    EXPECT_EQ(elfMagic[3], identity32.magic[3]);
    EXPECT_EQ(EI_CLASS_32, identity32.eClass);
    EXPECT_EQ(EI_DATA_LITTLE_ENDIAN, identity32.data);
    EXPECT_EQ(EV_CURRENT, identity32.version);
    EXPECT_EQ(0U, identity32.osAbi);
    EXPECT_EQ(0U, identity32.abiVersion);
    EXPECT_EQ(ET_NONE, header32.type);
    EXPECT_EQ(EM_NONE, header32.machine);
    EXPECT_EQ(1U, header32.version);
    EXPECT_EQ(0U, header32.entry);
    EXPECT_EQ(0U, header32.phOff);
    EXPECT_EQ(0U, header32.shOff);
    EXPECT_EQ(0U, header32.flags);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_32>), header32.ehSize);
    EXPECT_EQ(sizeof(ElfProgramHeader<EI_CLASS_32>), header32.phEntSize);
    EXPECT_EQ(0U, header32.phNum);
    EXPECT_EQ(sizeof(ElfSectionHeader<EI_CLASS_32>), header32.shEntSize);
    EXPECT_EQ(0U, header32.shNum);
    EXPECT_EQ(SHN_UNDEF, header32.shStrNdx);
}

TEST(ElfEncoder, GivenRequestForUndefAndSectionHeaderNamesSectionsWhenNotNeededThenNotEmitted) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(true, true);
    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.shOff);
    EXPECT_EQ(0U, header64.shNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);
}

TEST(ElfEncoder, GivenRequestForUndefAndSectionHeaderNamesSectionsWhenNeededThenEmitted) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(true, true);
    elfEncoder64.appendSection(SHT_NULL, ".my_name_is_important", {});
    auto elfData64 = elfEncoder64.encode();
    char expectedSectionNamesData[] = "\0.shstrtab\0.my_name_is_important";
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_64>) + alignUp(sizeof(expectedSectionNamesData), 8), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.phOff);
    EXPECT_EQ(header64.ehSize, header64.shOff);
    EXPECT_EQ(0U, header64.flags);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>), header64.ehSize);
    EXPECT_EQ(sizeof(ElfProgramHeader<EI_CLASS_64>), header64.phEntSize);
    EXPECT_EQ(0U, header64.phNum);
    EXPECT_EQ(sizeof(ElfSectionHeader<EI_CLASS_64>), header64.shEntSize);
    EXPECT_EQ(3U, header64.shNum);
    EXPECT_EQ(2U, header64.shStrNdx);
    auto &sectUndef64 = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
    auto &sectSkipMe64 = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>) + sizeof(ElfSectionHeader<EI_CLASS_64>));
    auto &sectSectionNames64 = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>) + 2 * sizeof(ElfSectionHeader<EI_CLASS_64>));
    const char *sectionNames64Data = reinterpret_cast<char *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_64>));

    EXPECT_EQ(0U, sectUndef64.name);
    EXPECT_EQ(SHT_NULL, sectUndef64.type);
    EXPECT_EQ(SHF_NONE, sectUndef64.flags);
    EXPECT_EQ(0U, sectUndef64.addr);
    EXPECT_EQ(0U, sectUndef64.offset);
    EXPECT_EQ(0U, sectUndef64.size);
    EXPECT_EQ(SHN_UNDEF, sectUndef64.link);
    EXPECT_EQ(0U, sectUndef64.info);
    EXPECT_EQ(0U, sectUndef64.addralign);
    EXPECT_EQ(0U, sectUndef64.entsize);

    EXPECT_EQ(11U, sectSkipMe64.name);
    EXPECT_EQ(SHT_NULL, sectSkipMe64.type);
    EXPECT_EQ(SHF_NONE, sectSkipMe64.flags);
    EXPECT_EQ(0U, sectSkipMe64.addr);
    EXPECT_EQ(0U, sectSkipMe64.offset);
    EXPECT_EQ(0U, sectSkipMe64.size);
    EXPECT_EQ(SHN_UNDEF, sectSkipMe64.link);
    EXPECT_EQ(0U, sectSkipMe64.info);
    EXPECT_EQ(8U, sectSkipMe64.addralign);
    EXPECT_EQ(0U, sectSkipMe64.entsize);

    EXPECT_EQ(1U, sectSectionNames64.name);
    EXPECT_EQ(SHT_STRTAB, sectSectionNames64.type);
    EXPECT_EQ(SHF_NONE, sectSectionNames64.flags);
    EXPECT_EQ(0U, sectSectionNames64.addr);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_64>), sectSectionNames64.offset);
    EXPECT_EQ(sizeof(expectedSectionNamesData), sectSectionNames64.size);
    EXPECT_EQ(SHN_UNDEF, sectSectionNames64.link);
    EXPECT_EQ(0U, sectSectionNames64.info);
    EXPECT_EQ(8U, sectSectionNames64.addralign);
    EXPECT_EQ(0U, sectSectionNames64.entsize);
    EXPECT_EQ(0, memcmp(expectedSectionNamesData, sectionNames64Data, sizeof(expectedSectionNamesData)));

    ElfEncoder<EI_CLASS_32> elfEncoder32(true, true);
    elfEncoder32.appendSection(SHT_NULL, ".my_name_is_important", {});
    auto elfData32 = elfEncoder32.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_32>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_32>) + alignUp(sizeof(expectedSectionNamesData), 8), elfData32.size());
    auto &header32 = *reinterpret_cast<ElfFileHeader<EI_CLASS_32> *>(elfData32.data());
    EXPECT_EQ(header32.ehSize, header32.shOff);
    auto &sectSectionNames32 = *reinterpret_cast<ElfSectionHeader<EI_CLASS_32> *>(elfData32.data() + sizeof(ElfFileHeader<EI_CLASS_32>) + 2 * sizeof(ElfSectionHeader<EI_CLASS_32>));
    const char *sectionNames32Data = reinterpret_cast<char *>(elfData32.data() + sizeof(ElfFileHeader<EI_CLASS_32>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_32>));
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_32>) + 3 * sizeof(ElfSectionHeader<EI_CLASS_32>), sectSectionNames32.offset);
    EXPECT_EQ(sizeof(expectedSectionNamesData), sectSectionNames32.size);
    EXPECT_EQ(0, memcmp(expectedSectionNamesData, sectionNames32Data, sizeof(expectedSectionNamesData)));
}

TEST(ElfEncoder, WhenAppendingSectionWithDataThenOffsetsAreProperlyUpdated) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
    const uint8_t data[] = "235711131719";
    elfEncoder64.appendSection(SHT_PROGBITS, ".my_name_is_not_important", data);

    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfSectionHeader<EI_CLASS_64>) + alignUp(sizeof(data), 8), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.phOff);
    EXPECT_EQ(header64.ehSize, header64.shOff);
    EXPECT_EQ(1U, header64.shNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

    auto &sectProgBits = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
    auto sectProgBitsData = elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>) + sizeof(ElfSectionHeader<EI_CLASS_64>);
    EXPECT_EQ(0U, sectProgBits.name);
    EXPECT_EQ(SHT_PROGBITS, sectProgBits.type);
    EXPECT_EQ(SHF_NONE, sectProgBits.flags);
    EXPECT_EQ(0U, sectProgBits.addr);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + sizeof(ElfSectionHeader<EI_CLASS_64>), sectProgBits.offset);
    EXPECT_EQ(sizeof(data), sectProgBits.size);
    EXPECT_EQ(SHN_UNDEF, sectProgBits.link);
    EXPECT_EQ(0U, sectProgBits.info);
    EXPECT_EQ(8U, sectProgBits.addralign);
    EXPECT_EQ(0U, sectProgBits.entsize);
    EXPECT_EQ(0, memcmp(data, sectProgBitsData, sizeof(data)));
}

TEST(ElfEncoder, WhenAppendingSectionWithoutDataThenOffsetsAreLeftIntact) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
    elfEncoder64.appendSection(SHT_PROGBITS, ".my_name_is_not_important", {});

    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfSectionHeader<EI_CLASS_64>), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.phOff);
    EXPECT_EQ(header64.ehSize, header64.shOff);
    EXPECT_EQ(1U, header64.shNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

    auto &sectProgBits = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
    EXPECT_EQ(0U, sectProgBits.name);
    EXPECT_EQ(SHT_PROGBITS, sectProgBits.type);
    EXPECT_EQ(SHF_NONE, sectProgBits.flags);
    EXPECT_EQ(0U, sectProgBits.addr);
    EXPECT_EQ(0U, sectProgBits.offset);
    EXPECT_EQ(0U, sectProgBits.size);
    EXPECT_EQ(SHN_UNDEF, sectProgBits.link);
    EXPECT_EQ(0U, sectProgBits.info);
    EXPECT_EQ(8U, sectProgBits.addralign);
    EXPECT_EQ(0U, sectProgBits.entsize);
}

TEST(ElfEncoder, WhenAppendingSectionWithNoBitsTypeThenDataIsIgnored) {
    {
        ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
        const uint8_t data[] = "235711131719";
        elfEncoder64.appendSection(SHT_NOBITS, ".my_name_is_not_important", data);

        auto elfData64 = elfEncoder64.encode();
        ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfSectionHeader<EI_CLASS_64>), elfData64.size());
        auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
        EXPECT_EQ(0U, header64.phOff);
        EXPECT_EQ(header64.ehSize, header64.shOff);
        EXPECT_EQ(1U, header64.shNum);
        EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

        auto &sectNoBits = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
        EXPECT_EQ(0U, sectNoBits.name);
        EXPECT_EQ(SHT_NOBITS, sectNoBits.type);
        EXPECT_EQ(SHF_NONE, sectNoBits.flags);
        EXPECT_EQ(0U, sectNoBits.addr);
        EXPECT_EQ(0U, sectNoBits.offset);
        EXPECT_EQ(0U, sectNoBits.size);
        EXPECT_EQ(SHN_UNDEF, sectNoBits.link);
        EXPECT_EQ(0U, sectNoBits.info);
        EXPECT_EQ(8U, sectNoBits.addralign);
        EXPECT_EQ(0U, sectNoBits.entsize);
    }

    {
        ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
        const uint8_t data[] = "235711131719";
        elfEncoder64.appendSection(static_cast<uint32_t>(SHT_NOBITS), ".my_name_is_not_important", data).size = sizeof(data);

        auto elfData64 = elfEncoder64.encode();
        ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfSectionHeader<EI_CLASS_64>), elfData64.size());

        auto &sectNoBits = *reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
        EXPECT_EQ(0U, sectNoBits.offset);
        EXPECT_EQ(sizeof(data), sectNoBits.size);
    }
}

TEST(ElfEncoder, WhenAppendingSegmentWithDataThenOffsetsAreProperlyUpdated) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
    const uint8_t data[] = "235711131719";
    elfEncoder64.appendSegment(PT_LOAD, data);

    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfProgramHeader<EI_CLASS_64>) + alignUp(sizeof(data), 8), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.shOff);
    EXPECT_EQ(header64.ehSize, header64.phOff);
    EXPECT_EQ(1U, header64.phNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

    auto &segLoad = *reinterpret_cast<ElfProgramHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
    auto segLoadBitsData = elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>) + sizeof(ElfProgramHeader<EI_CLASS_64>);
    EXPECT_EQ(PT_LOAD, segLoad.type);
    EXPECT_EQ(PF_NONE, segLoad.flags);
    EXPECT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + sizeof(ElfProgramHeader<EI_CLASS_64>), segLoad.offset);
    EXPECT_EQ(0U, segLoad.vAddr);
    EXPECT_EQ(0U, segLoad.pAddr);
    EXPECT_EQ(sizeof(data), segLoad.fileSz);
    EXPECT_EQ(0U, segLoad.memSz);
    EXPECT_EQ(8U, segLoad.align);
    EXPECT_EQ(0, memcmp(data, segLoadBitsData, sizeof(data)));
}

TEST(ElfEncoder, WhenAppendingSegmentWithoutDataThenOffsetsAreLeftIntact) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, false);
    elfEncoder64.appendSegment(PT_LOAD, {});

    auto elfData64 = elfEncoder64.encode();
    ASSERT_EQ(sizeof(ElfFileHeader<EI_CLASS_64>) + 1 * sizeof(ElfProgramHeader<EI_CLASS_64>), elfData64.size());
    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    EXPECT_EQ(0U, header64.shOff);
    EXPECT_EQ(header64.ehSize, header64.phOff);
    EXPECT_EQ(1U, header64.phNum);
    EXPECT_EQ(SHN_UNDEF, header64.shStrNdx);

    auto &segLoad = *reinterpret_cast<ElfProgramHeader<EI_CLASS_64> *>(elfData64.data() + sizeof(ElfFileHeader<EI_CLASS_64>));
    EXPECT_EQ(PT_LOAD, segLoad.type);
    EXPECT_EQ(PF_NONE, segLoad.flags);
    EXPECT_EQ(0U, segLoad.offset);
    EXPECT_EQ(0U, segLoad.vAddr);
    EXPECT_EQ(0U, segLoad.pAddr);
    EXPECT_EQ(0U, segLoad.fileSz);
    EXPECT_EQ(0U, segLoad.memSz);
    EXPECT_EQ(8U, segLoad.align);
}

TEST(ElfEncoder, WhenAppendingBothSectionAndSegmentThenProperElfIsEmited) {
    const uint8_t segmentData[] = "235711131719";
    const uint8_t sectionData[] = "232931374143";
    const uint8_t sectionNamesData[] = "\0.shstrtab\0.my_name_is_important";

    ElfFileHeader<EI_CLASS_64> header;
    header.type = ET_EXEC;
    header.entry = 16U;
    header.phOff = static_cast<decltype(header.phOff)>(sizeof(header));
    header.shOff = static_cast<decltype(header.shOff)>(sizeof(header) + sizeof(ElfProgramHeader<EI_CLASS_64>));
    header.flags = 4U;
    header.phNum = 1U;
    header.shNum = 3U;
    header.shStrNdx = 2U;

    ElfProgramHeader<EI_CLASS_64> segment;
    segment.type = PT_LOAD;
    segment.flags = PF_R | PF_X;
    segment.offset = static_cast<decltype(segment.offset)>(header.shOff + sizeof(ElfSectionHeader<EI_CLASS_64>) * 3);
    segment.vAddr = 4096U;
    segment.pAddr = 4096U * 16;
    segment.fileSz = static_cast<decltype(segment.fileSz)>(sizeof(segmentData));
    segment.memSz = 8192U;
    segment.align = 8U;

    ElfSectionHeader<EI_CLASS_64> sectionUndef;
    ElfSectionHeader<EI_CLASS_64> sectionProgBits;
    sectionProgBits.name = 11U;
    sectionProgBits.type = SHT_PROGBITS;
    sectionProgBits.flags = SHF_ALLOC;
    sectionProgBits.addr = 16U;
    sectionProgBits.offset = static_cast<decltype(sectionProgBits.offset)>(segment.offset + alignUp(static_cast<size_t>(segment.fileSz), static_cast<size_t>(segment.align)));
    sectionProgBits.size = static_cast<decltype(sectionProgBits.size)>(sizeof(sectionData));
    sectionProgBits.link = SHN_UNDEF;
    sectionProgBits.info = 0U;
    sectionProgBits.addralign = 16U;
    sectionProgBits.entsize = 0U;
    ElfSectionHeader<EI_CLASS_64> sectionSectionNames;
    sectionSectionNames.name = 1U;
    sectionSectionNames.type = SHT_STRTAB;
    sectionSectionNames.flags = SHF_NONE;
    sectionSectionNames.addr = 0U;
    sectionSectionNames.offset = static_cast<decltype(sectionSectionNames.offset)>(sectionProgBits.offset + alignUp(static_cast<size_t>(sectionProgBits.size), static_cast<size_t>(sectionProgBits.addralign)));
    sectionSectionNames.size = static_cast<decltype(sectionSectionNames.size)>(sizeof(sectionNamesData));
    sectionSectionNames.link = SHN_UNDEF;
    sectionSectionNames.info = 0U;
    sectionSectionNames.addralign = 8U;
    sectionSectionNames.entsize = 0U;

    std::vector<uint8_t> handMade;
    handMade.insert(handMade.end(), reinterpret_cast<uint8_t *>(&header), reinterpret_cast<uint8_t *>(&header + 1));
    handMade.insert(handMade.end(), reinterpret_cast<uint8_t *>(&segment), reinterpret_cast<uint8_t *>(&segment + 1));
    handMade.insert(handMade.end(), reinterpret_cast<uint8_t *>(&sectionUndef), reinterpret_cast<uint8_t *>(&sectionUndef + 1));
    handMade.insert(handMade.end(), reinterpret_cast<uint8_t *>(&sectionProgBits), reinterpret_cast<uint8_t *>(&sectionProgBits + 1));
    handMade.insert(handMade.end(), reinterpret_cast<uint8_t *>(&sectionSectionNames), reinterpret_cast<uint8_t *>(&sectionSectionNames + 1));
    handMade.insert(handMade.end(), segmentData, segmentData + sizeof(segmentData));
    handMade.resize(static_cast<size_t>(sectionProgBits.offset), 0U);
    handMade.insert(handMade.end(), sectionData, sectionData + sizeof(sectionData));
    handMade.resize(static_cast<size_t>(sectionSectionNames.offset), 0U);
    handMade.insert(handMade.end(), sectionNamesData, sectionNamesData + sizeof(sectionNamesData));
    handMade.resize(static_cast<size_t>(sectionSectionNames.offset + alignUp(sizeof(sectionNamesData), 8U)), 0U);

    ElfEncoder<EI_CLASS_64> elfEncoder64;
    {
        auto &encodedHeader = elfEncoder64.getElfFileHeader();
        encodedHeader.type = ET_EXEC;
        encodedHeader.entry = 16U;
        encodedHeader.flags = 4U;

        auto &encodedSegment = elfEncoder64.appendSegment(PT_LOAD, segmentData);
        encodedSegment.type = PT_LOAD;
        encodedSegment.flags = PF_R | PF_X;
        encodedSegment.vAddr = 4096U;
        encodedSegment.pAddr = 4096U * 16;
        encodedSegment.memSz = 8192U;
        encodedSegment.align = 8U;

        ElfSectionHeader<EI_CLASS_64> encodedSectionProgBits;
        encodedSectionProgBits.name = elfEncoder64.appendSectionName(".my_name_is_important");
        encodedSectionProgBits.type = SHT_PROGBITS;
        encodedSectionProgBits.flags = SHF_ALLOC;
        encodedSectionProgBits.addr = 16U;
        encodedSectionProgBits.link = SHN_UNDEF;
        encodedSectionProgBits.info = 0U;
        encodedSectionProgBits.addralign = 16U;
        encodedSectionProgBits.entsize = 0U;

        elfEncoder64.appendSection(encodedSectionProgBits, sectionData);
    }

    auto generated = elfEncoder64.encode();

    EXPECT_EQ(handMade, generated);
}

TEST(ElfEncoder, WhenDefaultAlignmentIsRaisedThenSegmentDataAbideByIt) {
    const uint8_t segmentData[] = "235711131719";
    const uint8_t sectionData[] = "232931374143475";

    static constexpr size_t alignment = 128U;

    ElfEncoder<EI_CLASS_64> elfEncoder64(true, true, alignment);
    elfEncoder64.appendSegment(PT_LOAD, segmentData);

    elfEncoder64.appendSection(NEO::Elf::SHT_PROGBITS, "my_name_is_important", sectionData);
    auto elfData64 = elfEncoder64.encode();

    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    auto sectionHeaders = reinterpret_cast<NEO::Elf::ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + static_cast<size_t>(header64.shOff));
    auto programHeaders = reinterpret_cast<NEO::Elf::ElfProgramHeader<EI_CLASS_64> *>(elfData64.data() + static_cast<size_t>(header64.phOff));
    for (const auto &section : NEO::CreateRange(sectionHeaders, header64.shNum)) {
        EXPECT_EQ(0U, section.offset % 8U);
    }
    for (const auto &segment : NEO::CreateRange(programHeaders, header64.phNum)) {
        EXPECT_EQ(0U, segment.offset % alignment);
        EXPECT_LE(alignment, segment.align);
    }
}

TEST(ElfEncoder, WhenDefaultAlignmentIsLoweredThenSectionAndSegmentDataAbideByIt) {
    const uint8_t segmentData[] = "235711131719";
    const uint8_t sectionData[] = "232931374143475";

    static constexpr size_t alignment = 1U;

    ElfEncoder<EI_CLASS_64> elfEncoder64(true, true, alignment);
    elfEncoder64.appendSegment(PT_LOAD, segmentData);

    elfEncoder64.appendSection(NEO::Elf::SHT_PROGBITS, "my_name_is_important", sectionData);
    auto elfData64 = elfEncoder64.encode();

    auto &header64 = *reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData64.data());
    auto sectionHeaders = reinterpret_cast<NEO::Elf::ElfSectionHeader<EI_CLASS_64> *>(elfData64.data() + static_cast<size_t>(header64.shOff));
    NEO::Elf::ElfSectionHeader<EI_CLASS_64> *sectionNamesSection = sectionHeaders + header64.shStrNdx;
    size_t unpaddedSize = sizeof(ElfFileHeader<EI_CLASS_64>) + 3 * sizeof(NEO::Elf::ElfSectionHeader<EI_CLASS_64>) + sizeof(NEO::Elf::ElfProgramHeader<EI_CLASS_64>);
    unpaddedSize += sizeof(segmentData) + sizeof(sectionData) + static_cast<size_t>(sectionNamesSection->size);
    EXPECT_EQ(unpaddedSize, elfData64.size());
}

TEST(ElfEncoder, WhenAppendingEmptySectionNameThenAlwaysReturn0AsOffset) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(false, true);
    auto offset0 = elfEncoder64.appendSectionName({});
    auto offset1 = elfEncoder64.appendSectionName({});
    auto offset2 = elfEncoder64.appendSectionName({});
    EXPECT_EQ(0U, offset0);
    EXPECT_EQ(0U, offset1);
    EXPECT_EQ(0U, offset2);
}

TEST(ElfEncoder, WhenAppendingSectionNameThenEmplacedStringIsAlwaysNullterminated) {
    ElfEncoder<EI_CLASS_64> elfEncoder64(true, true);
    auto strOffset = elfEncoder64.appendSectionName(ConstStringRef("abc", 2));
    auto strOffset2 = elfEncoder64.appendSectionName(ConstStringRef("de", 3));
    auto strOffset3 = elfEncoder64.appendSectionName(ConstStringRef("g"));
    elfEncoder64.appendSection(SHT_NOBITS, "my_name_is_important", {});
    EXPECT_EQ(strOffset + 3, strOffset2);
    EXPECT_EQ(strOffset2 + 3, strOffset3);
    auto elfData = elfEncoder64.encode();
    auto header = reinterpret_cast<ElfFileHeader<EI_CLASS_64> *>(elfData.data());

    auto sectionNamesSection = reinterpret_cast<ElfSectionHeader<EI_CLASS_64> *>(elfData.data() + static_cast<size_t>(header->shOff + header->shStrNdx * header->shEntSize));
    EXPECT_STREQ("ab", reinterpret_cast<const char *>(elfData.data() + sectionNamesSection->offset + strOffset));
    EXPECT_STREQ("de", reinterpret_cast<const char *>(elfData.data() + sectionNamesSection->offset + strOffset2));
    EXPECT_STREQ("g", reinterpret_cast<const char *>(elfData.data() + sectionNamesSection->offset + strOffset3));
}
