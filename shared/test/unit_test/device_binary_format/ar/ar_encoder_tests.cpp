/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "test.h"

#include <cstring>

using namespace NEO::Ar;

TEST(ArFileEntryHeader, GivenDefaultArFileEntryHeaderThenSectionAreProperlyPopulated) {
    ArFileEntryHeader header = {};
    EXPECT_EQ(ConstStringRef("/               ", 16), ConstStringRef(header.identifier));
    EXPECT_EQ(ConstStringRef("0           ", 12), ConstStringRef(header.fileModificationTimestamp));
    EXPECT_EQ(ConstStringRef("0     ", 6), ConstStringRef(header.ownerId));
    EXPECT_EQ(ConstStringRef("0     ", 6), ConstStringRef(header.groupId));
    EXPECT_EQ(ConstStringRef("644     ", 8), ConstStringRef(header.fileMode));
    EXPECT_EQ(ConstStringRef("0         ", 10), ConstStringRef(header.fileSizeInBytes));
    EXPECT_EQ(ConstStringRef("\x60\x0A", 2), ConstStringRef(header.trailingMagic));
}

TEST(ArEncoder, GivenTooLongIdentifierThenAppendingFileFails) {
    const uint8_t fileData[] = "2357111317192329";
    ArEncoder encoder;
    EXPECT_EQ(nullptr, encoder.appendFileEntry("my_identifier_is_longer_than_16_charters", fileData));
}

TEST(ArEncoder, GivenEmptyIdentifierThenAppendingFileFails) {
    const uint8_t fileData[] = "2357111317192329";
    ArEncoder encoder;
    EXPECT_EQ(nullptr, encoder.appendFileEntry("", fileData));
}

TEST(ArEncoder, GivenEmptyArThenEncodedFileConsistsOfOnlyArMagic) {
    ArEncoder encoder;
    auto arData = encoder.encode();
    EXPECT_EQ(arMagic.size(), arData.size());
    EXPECT_TRUE(NEO::hasSameMagic(arMagic, arData));
}

TEST(ArEncoder, GivenValidFileEntriesThenAppendingFileSucceeds) {
    std::string fileName = "file1.txt";
    const uint8_t fileData[18] = "23571113171923293";
    ArEncoder encoder;
    auto returnedSection = encoder.appendFileEntry(fileName, fileData);
    ASSERT_NE(nullptr, returnedSection);
    ArFileEntryHeader expectedSection;
    memcpy_s(expectedSection.identifier, sizeof(expectedSection.identifier), fileName.c_str(), fileName.size());
    expectedSection.identifier[fileName.size()] = '/';
    expectedSection.fileSizeInBytes[0] = '1';
    expectedSection.fileSizeInBytes[1] = '8';
    EXPECT_EQ(0, memcmp(returnedSection, &expectedSection, sizeof(expectedSection)));

    auto arData = encoder.encode();
    EXPECT_TRUE(NEO::hasSameMagic(arMagic, arData));
    ASSERT_EQ(arData.size(), arMagic.size() + sizeof(fileData) + sizeof(ArFileEntryHeader));
    ArFileEntryHeader *file0 = reinterpret_cast<ArFileEntryHeader *>(arData.data() + arMagic.size());
    auto file0Data = arData.data() + arMagic.size() + sizeof(ArFileEntryHeader);
    EXPECT_EQ(0, memcmp(file0, &expectedSection, sizeof(expectedSection)));
    EXPECT_EQ(0, memcmp(file0Data, fileData, sizeof(fileData)));
}

TEST(ArEncoder, GivenValidTwoFileEntriesWith2byteUnalignedDataThenPaddingIsImplicitlyAdded) {
    std::string fileName0 = "a";
    std::string fileName1 = "b";
    const uint8_t data0[7] = "123456";
    const uint8_t data1[16] = "9ABCDEFGHIJKLMN";
    ArEncoder encoder;

    auto returnedSection = encoder.appendFileEntry(fileName0, data0);
    ASSERT_NE(nullptr, returnedSection);
    ArFileEntryHeader expectedSection0;
    expectedSection0.identifier[0] = 'a';
    expectedSection0.identifier[1] = '/';
    expectedSection0.fileSizeInBytes[0] = '7';
    EXPECT_EQ(0, memcmp(returnedSection, &expectedSection0, sizeof(expectedSection0)));

    returnedSection = encoder.appendFileEntry(fileName1, data1);
    ASSERT_NE(nullptr, returnedSection);
    ArFileEntryHeader expectedSection1;
    expectedSection1.identifier[0] = 'b';
    expectedSection1.identifier[1] = '/';
    expectedSection1.fileSizeInBytes[0] = '1';
    expectedSection1.fileSizeInBytes[1] = '6';
    EXPECT_EQ(0, memcmp(returnedSection, &expectedSection1, sizeof(expectedSection1)));

    auto arData = encoder.encode();
    EXPECT_TRUE(NEO::hasSameMagic(arMagic, arData));
    ASSERT_EQ(arData.size(), arMagic.size() + sizeof(data0) + sizeof(data1) + 2 * sizeof(ArFileEntryHeader) + 1);
    ArFileEntryHeader *file0 = reinterpret_cast<ArFileEntryHeader *>(arData.data() + arMagic.size());
    auto file0Data = arData.data() + arMagic.size() + sizeof(ArFileEntryHeader);
    ArFileEntryHeader *file1 = reinterpret_cast<ArFileEntryHeader *>(arData.data() + arMagic.size() + sizeof(data0) + sizeof(ArFileEntryHeader) + 1);
    auto file1Data = arData.data() + arMagic.size() + sizeof(data0) + 2 * sizeof(ArFileEntryHeader) + 1;
    EXPECT_EQ(0, memcmp(file0, &expectedSection0, sizeof(expectedSection0)));
    EXPECT_EQ(0, memcmp(file0Data, data0, sizeof(data0)));
    EXPECT_EQ(0, memcmp(file1, &expectedSection1, sizeof(expectedSection1)));
    EXPECT_EQ(0, memcmp(file1Data, data1, sizeof(data1)));
}

TEST(ArEncoder, GivenValidTwoFileEntriesWhen8BytePaddingIsRequestedThenPaddingFileEntriesAreAddedWhenNeeded) {
    std::string fileName0 = "a";
    std::string fileName1 = "b";
    std::string fileName2 = "c";
    const uint8_t data0[4] = "123";      // will require padding before
    const uint8_t data1[8] = "9ABCDEF";  // won't require padding before
    const uint8_t data2[16] = "9ABCDEF"; // will require padding before
    ArEncoder encoder(true);

    encoder.appendFileEntry(fileName0, data0);
    encoder.appendFileEntry(fileName1, data1);
    encoder.appendFileEntry(fileName2, data2);

    auto arData = encoder.encode();
    EXPECT_TRUE(NEO::hasSameMagic(arMagic, arData));
    ASSERT_EQ(arData.size(), arMagic.size() + sizeof(data0) + sizeof(data1) + sizeof(data2) + 5 * sizeof(ArFileEntryHeader) + 16);

    auto files = arData.data() + arMagic.size();

    ArFileEntryHeader *pad0 = reinterpret_cast<ArFileEntryHeader *>(files);
    auto pad0Data = reinterpret_cast<uint8_t *>(pad0) + sizeof(ArFileEntryHeader);

    ArFileEntryHeader *file0 = reinterpret_cast<ArFileEntryHeader *>(pad0Data + 8);
    auto file0Data = reinterpret_cast<uint8_t *>(file0) + sizeof(ArFileEntryHeader);

    ArFileEntryHeader *file1 = reinterpret_cast<ArFileEntryHeader *>(file0Data + sizeof(data0));
    auto file1Data = reinterpret_cast<uint8_t *>(file1) + sizeof(ArFileEntryHeader);

    ArFileEntryHeader *pad1 = reinterpret_cast<ArFileEntryHeader *>(file1Data + sizeof(data1));
    auto pad1Data = reinterpret_cast<uint8_t *>(pad1) + sizeof(ArFileEntryHeader);

    ArFileEntryHeader *file2 = reinterpret_cast<ArFileEntryHeader *>(pad1Data + 8);
    auto file2Data = reinterpret_cast<uint8_t *>(file2) + sizeof(ArFileEntryHeader);

    EXPECT_EQ(0U, ptrDiff(file0Data, arData.data()) % 8);
    EXPECT_EQ(0U, ptrDiff(file1Data, arData.data()) % 8);
    EXPECT_EQ(0U, ptrDiff(file2Data, arData.data()) % 8);

    EXPECT_EQ(0, memcmp(file0Data, data0, sizeof(data0)));
    EXPECT_EQ(0, memcmp(file1Data, data1, sizeof(data1)));
    EXPECT_EQ(0, memcmp(file2Data, data2, sizeof(data2)));
}
