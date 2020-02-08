/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/ar/ar_decoder.h"
#include "test.h"

using namespace NEO::Ar;

TEST(ArDecoderIsAr, WhenNotArThenReturnsFalse) {
    ArrayRef<const uint8_t> empty;
    EXPECT_FALSE(isAr(empty));

    const uint8_t notAr[] = "aaaaa";
    EXPECT_FALSE(isAr(notAr));
}

TEST(ArDecoderIsAr, WhenValidArThenReturnsTrue) {
    auto emptyAr = ArrayRef<const uint8_t>::fromAny(arMagic.begin(), arMagic.size());
    EXPECT_TRUE(isAr(emptyAr));
}

TEST(ArDecoderReadDecimal, WhenNullOrSpaceIsEncounteredThenParsingStops) {
    const char spaceDelimited[] = "213 123";
    const char nullTerminateDelimited[] = {'4', '5', '6', '\0', '7', '8', '9'};
    EXPECT_EQ(213U, readDecimal<sizeof(spaceDelimited)>(spaceDelimited));
    EXPECT_EQ(456U, readDecimal<sizeof(nullTerminateDelimited)>(nullTerminateDelimited));
}

TEST(ArDecoderReadDecimal, WhenNullOrSpaceIsNotEncounteredThenParsesTillTheEnd) {
    const char noteDelimited[] = "213123";
    EXPECT_EQ(2131U, readDecimal<4>(noteDelimited));
}

TEST(ArDecoderIsStringPadding, GivenCharacterThenReturnsTrueOnlyIfArStringPaddingCharacter) {
    EXPECT_TRUE(isStringPadding(' '));
    EXPECT_TRUE(isStringPadding('/'));
    EXPECT_TRUE(isStringPadding('\0'));
    EXPECT_FALSE(isStringPadding('\t'));
    EXPECT_FALSE(isStringPadding('\r'));
    EXPECT_FALSE(isStringPadding('\n'));
    EXPECT_FALSE(isStringPadding('0'));
    EXPECT_FALSE(isStringPadding('a'));
}

TEST(ArDecoderReadUnpaddedString, GivenPaddedStringTheReturnsUnpaddedStringPart) {
    const char paddedString[] = "abcd/   \0";
    auto unpadded = readUnpaddedString<sizeof(paddedString)>(paddedString);
    EXPECT_EQ(paddedString, unpadded.begin());
    EXPECT_EQ(4U, unpadded.size());
}

TEST(ArDecoderReadUnpaddedString, GivenEmptyPaddedStringTheReturnsEmptyString) {
    const char paddedString[] = "//   \0";
    auto unpadded = readUnpaddedString<sizeof(paddedString)>(paddedString);
    EXPECT_TRUE(unpadded.empty());
}

TEST(ArDecoderReadUnpaddedString, GivenUnpaddedStringTheReturnsDataInBounds) {
    const char paddedString[] = "abcdefgh";
    auto unpadded = readUnpaddedString<3>(paddedString);
    EXPECT_EQ(paddedString, unpadded.begin());
    EXPECT_EQ(3U, unpadded.size());
}

TEST(ArDecoderReadLongFileName, GivenOffsetThenParsesCorrectString) {
    const char names[] = "abcde/fgh/ij";
    auto name0 = readLongFileName(names, 0U);
    auto name1 = readLongFileName(names, 6U);
    auto name2 = readLongFileName(names, 10U);
    auto name3 = readLongFileName(names, 40U);
    EXPECT_EQ(names, name0.begin());
    EXPECT_EQ(5U, name0.size());

    EXPECT_EQ(names + 6U, name1.begin());
    EXPECT_EQ(3U, name1.size());

    EXPECT_EQ(names + 10U, name2.begin());
    EXPECT_EQ(2U, name2.size());

    EXPECT_TRUE(name3.empty());
}

TEST(ArDecoderDecodeAr, GivenNotArThenFailDecoding) {
    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr({}, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, ar.magic);
    EXPECT_EQ(0U, ar.files.size());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Not an AR archive - mismatched file signature", decodeErrors.c_str());
}

TEST(ArDecoderDecodeAr, GivenValidArThenDecodingSucceeds) {
    const uint8_t data[8] = "1234567";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = 'a';
    fileEntry0.identifier[1] = '/';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data, data + sizeof(data));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(ar.longFileNamesEntry.fileData.empty());
    EXPECT_TRUE(ar.longFileNamesEntry.fileName.empty());
    ASSERT_EQ(1U, ar.files.size());
    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.files[0].fullHeader);
    EXPECT_EQ("a", ar.files[0].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.files[0].fileData.begin());
    EXPECT_EQ(8U, ar.files[0].fileData.size());
}

TEST(ArDecoderDecodeAr, GivenArWhenFileEntryHeaderHasEmptyIdentifierThenDecodingFails) {
    const uint8_t data[8] = "1234567";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data, data + sizeof(data));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, ar.magic);
    EXPECT_EQ(0U, ar.files.size());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Corrupt AR archive - file entry does not have identifier : '/               '", decodeErrors.c_str());
}

TEST(ArDecoderDecodeAr, GivenInvalidFileEntryHeaderTrailingMagicThenDecodingSucceedsButWarningIsEmitted) {
    const uint8_t data[8] = "1234567";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = 'a';
    fileEntry0.identifier[1] = '/';
    fileEntry0.fileSizeInBytes[0] = '8';
    fileEntry0.trailingMagic[0] = 'a';
    fileEntry0.trailingMagic[1] = 'a';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data, data + sizeof(data));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(ar.longFileNamesEntry.fileData.empty());
    EXPECT_TRUE(ar.longFileNamesEntry.fileName.empty());
    ASSERT_EQ(1U, ar.files.size());
    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.files[0].fullHeader);
    EXPECT_EQ("a", ar.files[0].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.files[0].fileData.begin());
    EXPECT_EQ(8U, ar.files[0].fileData.size());

    EXPECT_FALSE(decodeWarnings.empty());
    EXPECT_STREQ("File entry header with identifier 'a/              ' has invalid header trailing string", decodeWarnings.c_str());
}

TEST(ArDecoderDecodeAr, GivenOutOfBoundsFileEntryDataThenFailDecoding) {
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = 'a';
    fileEntry0.identifier[1] = '/';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, ar.magic);
    EXPECT_EQ(0U, ar.files.size());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Corrupt AR archive - out of bounds data of file entry with idenfitier 'a/              '", decodeErrors.c_str());
}

TEST(ArDecoderDecodeAr, GivenValidTwoFilesEntriesWith2byteAlignedDataThenDecodingSucceeds) {
    const uint8_t data0[8] = "1234567";
    const uint8_t data1[16] = "9ABCDEFGHIJKLMN";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = 'a';
    fileEntry0.identifier[1] = '/';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data0, data0 + sizeof(data0));

    ArFileEntryHeader fileEntry1;
    fileEntry1.identifier[0] = 'b';
    fileEntry1.identifier[1] = '/';
    fileEntry1.fileSizeInBytes[0] = '1';
    fileEntry1.fileSizeInBytes[1] = '6';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry1), reinterpret_cast<const uint8_t *>(&fileEntry1 + 1));
    arStorage.insert(arStorage.end(), data1, data1 + sizeof(data1));

    ASSERT_EQ(arMagic.size() + 2 * sizeof(ArFileEntryHeader) + sizeof(data0) + sizeof(data1), arStorage.size());

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(ar.longFileNamesEntry.fileData.empty());
    EXPECT_TRUE(ar.longFileNamesEntry.fileName.empty());

    ASSERT_EQ(2U, ar.files.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.files[0].fullHeader);
    EXPECT_EQ("a", ar.files[0].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.files[0].fileData.begin());
    EXPECT_EQ(8U, ar.files[0].fileData.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + sizeof(data0)), ar.files[1].fullHeader);
    EXPECT_EQ("b", ar.files[1].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + sizeof(data0) + sizeof(ArFileEntryHeader), ar.files[1].fileData.begin());
    EXPECT_EQ(16U, ar.files[1].fileData.size());
}

TEST(ArDecoderDecodeAr, GivenValidTwoFileEntriesWith2byteUnalignedDataAndPaddingThenImplicitPaddingIsTakenIntoAccount) {
    const uint8_t data0[7] = "123456";
    const uint8_t data1[15] = "9ABCDEFGHIJKLM";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = 'a';
    fileEntry0.identifier[1] = '/';
    fileEntry0.fileSizeInBytes[0] = '7';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data0, data0 + sizeof(data0));
    arStorage.push_back('\0'); // implicit 2-byte alignment padding

    ArFileEntryHeader fileEntry1;
    fileEntry1.identifier[0] = 'b';
    fileEntry1.identifier[1] = '/';
    fileEntry1.fileSizeInBytes[0] = '1';
    fileEntry1.fileSizeInBytes[1] = '5';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry1), reinterpret_cast<const uint8_t *>(&fileEntry1 + 1));
    arStorage.insert(arStorage.end(), data1, data1 + sizeof(data1));
    arStorage.push_back('\0'); // implicit 2-byte alignment padding
    ASSERT_EQ(arMagic.size() + 2 * sizeof(ArFileEntryHeader) + sizeof(data0) + sizeof(data1) + 2, arStorage.size());

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(ar.longFileNamesEntry.fileData.empty());
    EXPECT_TRUE(ar.longFileNamesEntry.fileName.empty());

    ASSERT_EQ(2U, ar.files.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.files[0].fullHeader);
    EXPECT_EQ("a", ar.files[0].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.files[0].fileData.begin());
    EXPECT_EQ(7U, ar.files[0].fileData.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + sizeof(data0) + 1), ar.files[1].fullHeader);
    EXPECT_EQ("b", ar.files[1].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + sizeof(data0) + 1 + sizeof(ArFileEntryHeader), ar.files[1].fileData.begin());
    EXPECT_EQ(15U, ar.files[1].fileData.size());
}

TEST(ArDecoderDecodeAr, GivenSpecialFileWithLongFilenamesThenSpecialFileIsProperlyRecognized) {
    const uint8_t names[8] = "123456/";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));
    ArFileEntryHeader fileEntrySpecial;
    fileEntrySpecial.identifier[0] = '/';
    fileEntrySpecial.identifier[1] = '/';
    fileEntrySpecial.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntrySpecial), reinterpret_cast<const uint8_t *>(&fileEntrySpecial + 1));
    arStorage.insert(arStorage.end(), names, names + sizeof(names));
    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.longFileNamesEntry.fullHeader);
    EXPECT_EQ("//", ar.longFileNamesEntry.fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.longFileNamesEntry.fileData.begin());
    EXPECT_EQ(8U, ar.longFileNamesEntry.fileData.size());
    EXPECT_EQ(0U, ar.files.size());
}

TEST(ArDecoderDecodeAr, GivenFilesWithLongFilenamesThenNamesAreProperlyDecoded) {
    const uint8_t longNames[] = "my_identifier_is_longer_than_16_charters/my_identifier_is_even_longer_than_previous_one/";
    size_t longNamesLen = sizeof(longNames) - 1U; // 88, ignore nullterminate
    const uint8_t data0[8] = "1234567";
    const uint8_t data1[16] = "9ABCDEFGHIJKLMN";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));

    ArFileEntryHeader fileEntrySpecial;
    fileEntrySpecial.identifier[0] = '/';
    fileEntrySpecial.identifier[1] = '/';
    fileEntrySpecial.fileSizeInBytes[0] = '8';
    fileEntrySpecial.fileSizeInBytes[1] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntrySpecial), reinterpret_cast<const uint8_t *>(&fileEntrySpecial + 1));
    arStorage.insert(arStorage.end(), longNames, longNames + longNamesLen);

    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = '/';
    fileEntry0.identifier[1] = '0';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data0, data0 + sizeof(data0));

    ArFileEntryHeader fileEntry1;
    fileEntry1.identifier[0] = '/';
    fileEntry1.identifier[1] = '4';
    fileEntry1.identifier[2] = '1';
    fileEntry1.fileSizeInBytes[0] = '1';
    fileEntry1.fileSizeInBytes[1] = '6';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry1), reinterpret_cast<const uint8_t *>(&fileEntry1 + 1));
    arStorage.insert(arStorage.end(), data1, data1 + sizeof(data1));

    ASSERT_EQ(arMagic.size() + 3 * sizeof(ArFileEntryHeader) + longNamesLen + sizeof(data0) + sizeof(data1), arStorage.size());

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(reinterpret_cast<const char *>(arStorage.data()), ar.magic);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size()), ar.longFileNamesEntry.fullHeader);
    EXPECT_EQ("//", ar.longFileNamesEntry.fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader), ar.longFileNamesEntry.fileData.begin());
    EXPECT_EQ(longNamesLen, ar.longFileNamesEntry.fileData.size());

    ASSERT_EQ(2U, ar.files.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + longNamesLen), ar.files[0].fullHeader);
    EXPECT_EQ("my_identifier_is_longer_than_16_charters", ar.files[0].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + longNamesLen + sizeof(ArFileEntryHeader), ar.files[0].fileData.begin());
    EXPECT_EQ(8U, ar.files[0].fileData.size());

    EXPECT_EQ(reinterpret_cast<ArFileEntryHeader *>(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + longNamesLen + sizeof(ArFileEntryHeader) + sizeof(data0)), ar.files[1].fullHeader);
    EXPECT_EQ("my_identifier_is_even_longer_than_previous_one", ar.files[1].fileName);
    EXPECT_EQ(arStorage.data() + arMagic.size() + sizeof(ArFileEntryHeader) + longNamesLen + sizeof(ArFileEntryHeader) + sizeof(data0) + sizeof(ArFileEntryHeader), ar.files[1].fileData.begin());
    EXPECT_EQ(16U, ar.files[1].fileData.size());
}

TEST(ArDecoderDecodeAr, GivenFilesWithLongFilenamesWhenFileNamesSpecialEntryNotPresentThenDecodingFails) {
    const uint8_t data0[8] = "1234567";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));

    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = '/';
    fileEntry0.identifier[1] = '0';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data0, data0 + sizeof(data0));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, ar.magic);
    EXPECT_EQ(0U, ar.files.size());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Corrupt AR archive - long file name entry has broken identifier : '/0              '", decodeErrors.c_str());
}

TEST(ArDecoderDecodeAr, GivenFilesWithLongFilenamesWhenLongNameIsOutOfBoundsThenDecodingFails) {
    const uint8_t longNames[] = "my_identifier_is_longer_than_16_charters/my_identifier_is_even_longer_than_previous_one/";
    size_t longNamesLen = sizeof(longNames) - 1U; // 88, ignore nullterminate
    const uint8_t data0[8] = "1234567";
    std::vector<uint8_t> arStorage;
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(arMagic.begin()), reinterpret_cast<const uint8_t *>(arMagic.end()));

    ArFileEntryHeader fileEntrySpecial;
    fileEntrySpecial.identifier[0] = '/';
    fileEntrySpecial.identifier[1] = '/';
    fileEntrySpecial.fileSizeInBytes[0] = '8';
    fileEntrySpecial.fileSizeInBytes[1] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntrySpecial), reinterpret_cast<const uint8_t *>(&fileEntrySpecial + 1));
    arStorage.insert(arStorage.end(), longNames, longNames + longNamesLen);

    ArFileEntryHeader fileEntry0;
    fileEntry0.identifier[0] = '/';
    fileEntry0.identifier[1] = '1';
    fileEntry0.identifier[2] = '0';
    fileEntry0.identifier[3] = '0';
    fileEntry0.fileSizeInBytes[0] = '8';
    arStorage.insert(arStorage.end(), reinterpret_cast<const uint8_t *>(&fileEntry0), reinterpret_cast<const uint8_t *>(&fileEntry0 + 1));
    arStorage.insert(arStorage.end(), data0, data0 + sizeof(data0));

    std::string decodeErrors;
    std::string decodeWarnings;
    auto ar = decodeAr(arStorage, decodeErrors, decodeWarnings);
    EXPECT_EQ(nullptr, ar.magic);
    EXPECT_EQ(0U, ar.files.size());
    EXPECT_EQ(nullptr, ar.longFileNamesEntry.fullHeader);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Corrupt AR archive - long file name entry has broken identifier : '/100            '", decodeErrors.c_str());
}
