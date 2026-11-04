/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/linux/sysman_info_log_cper_decoder.h"
#include "level_zero/sysman/test/unit_tests/sources/info_log/linux/mock_cper_record.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint8_t unwritten = 0xcd;

struct ReadOutput {
    std::vector<uint8_t> buffer;
    std::vector<zes_info_log_metadata_ext_t> descriptors;

    void add(const std::vector<uint8_t> &record) {
        zes_info_log_metadata_ext_t descriptor = {};
        descriptor.stype = ZES_STRUCTURE_TYPE_INFO_LOG_METADATA_EXT;
        descriptor.offset = static_cast<uint32_t>(buffer.size());
        descriptor.lengthOfData = static_cast<uint32_t>(record.size());
        descriptors.push_back(descriptor);
        buffer.insert(buffer.end(), record.begin(), record.end());
    }
    uint32_t size() const { return static_cast<uint32_t>(buffer.size()); }
    uint32_t recordCount() const { return static_cast<uint32_t>(descriptors.size()); }
};

std::vector<zes_intel_info_log_decodable_field_info_exp_t> makeFieldInfos(uint32_t count) {
    std::vector<zes_intel_info_log_decodable_field_info_exp_t> fieldInfos(count);
    for (auto &fieldInfo : fieldInfos) {
        fieldInfo.stype = ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_DECODABLE_FIELD_INFO_EXP;
    }
    return fieldInfos;
}

struct DecodedBatch {
    const ReadOutput *pRecords = nullptr;

    ze_result_t decode(const ReadOutput &records) {
        pRecords = &records;
        return CperDecoder::validateRecords(records.size(), records.recordCount(), records.descriptors.data());
    }

    ze_result_t readField(uint32_t field, uint32_t record, void *pOutBuffer, size_t bufferLen, size_t &bytesCopied) const {
        return CperDecoder::readField(pRecords->size(), pRecords->buffer.data(), pRecords->descriptors.data(),
                                      field, record, pOutBuffer, bufferLen, &bytesCopied);
    }

    ze_result_t readSigId(uint32_t record, uint32_t &sigId) const {
        size_t bytesCopied = 0;
        return readField(CperDecoder::sigIdField, record, &sigId, sizeof(sigId), bytesCopied);
    }

    ze_result_t readUuid(uint32_t record, MockCperRecord::Guid &uuid) const {
        size_t bytesCopied = 0;
        return readField(CperDecoder::uuidField, record, uuid.data(), uuid.size(), bytesCopied);
    }
};

TEST(SysmanInfoLogCperDecoderTest, GivenZeroCountWhenGettingFieldInfoThenTheNumberOfDecodableFieldsIsReturnedAndNothingIsWritten) {
    uint32_t count = 0;
    auto fieldInfos = makeFieldInfos(1);
    fieldInfos[0].fieldId = 0xbadu;

    EXPECT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(0xbadu, fieldInfos[0].fieldId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenFullCountWhenGettingFieldInfoThenSigIdAndUuidAreDescribed) {
    uint32_t count = CperDecoder::fieldCount;
    auto fieldInfos = makeFieldInfos(count);

    ASSERT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    ASSERT_EQ(2u, count);

    EXPECT_STREQ("sig_id", fieldInfos[0].name);
    EXPECT_EQ(CperDecoder::sigIdField, fieldInfos[0].fieldId);
    EXPECT_EQ(ZES_INTEL_INFO_LOG_FIELD_TYPE_EXP_UINT32, fieldInfos[0].type);
    EXPECT_EQ(sizeof(uint32_t), fieldInfos[0].requiredBytes);
    EXPECT_EQ(1u, fieldInfos[0].elementCount);

    EXPECT_STREQ("uuid", fieldInfos[1].name);
    EXPECT_EQ(CperDecoder::uuidField, fieldInfos[1].fieldId);
    EXPECT_EQ(ZES_INTEL_INFO_LOG_FIELD_TYPE_EXP_RAW_BYTES, fieldInfos[1].type);
    EXPECT_EQ(static_cast<size_t>(ZES_MAX_UUID_SIZE), fieldInfos[1].requiredBytes);
    EXPECT_EQ(static_cast<size_t>(ZES_MAX_UUID_SIZE), fieldInfos[1].elementCount);

    for (const auto &fieldInfo : fieldInfos) {
        EXPECT_EQ(ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_DECODABLE_FIELD_INFO_EXP, fieldInfo.stype);
        EXPECT_STREQ("1.0", fieldInfo.versionString);
        EXPECT_LT(0u, strlen(fieldInfo.desc));
    }
}

TEST(SysmanInfoLogCperDecoderTest, GivenCountBelowTheFieldCountWhenGettingFieldInfoThenOnlyThatManyFieldsAreDescribed) {
    uint32_t count = 1;
    auto fieldInfos = makeFieldInfos(2);
    fieldInfos[1].fieldId = 0xbadu;

    ASSERT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    EXPECT_EQ(1u, count);
    EXPECT_STREQ("sig_id", fieldInfos[0].name);
    EXPECT_EQ(0xbadu, fieldInfos[1].fieldId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenCountAboveTheFieldCountWhenGettingFieldInfoThenTheCountIsClamped) {
    uint32_t count = CperDecoder::fieldCount + 5u;
    auto fieldInfos = makeFieldInfos(count);

    ASSERT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    EXPECT_EQ(2u, count);
}

TEST(SysmanInfoLogCperDecoderTest, GivenNullFieldInfosWithANonZeroCountWhenGettingFieldInfoThenNoDescriptorIsWrittenAndTheCountIsReported) {
    uint32_t count = CperDecoder::fieldCount + 5u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, nullptr));
    EXPECT_EQ(2u, count);
}

TEST(SysmanInfoLogCperDecoderTest, GivenFieldInfoWithExtensionStructureWhenGettingFieldInfoThenInvalidArgumentIsReturnedAndNothingIsWritten) {
    uint32_t count = CperDecoder::fieldCount;
    auto fieldInfos = makeFieldInfos(count);
    uint32_t extension = 0;
    fieldInfos[1].pNext = &extension;
    fieldInfos[0].fieldId = 0xbadu;

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    EXPECT_EQ(0xbadu, fieldInfos[0].fieldId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenFieldInfoWithExtensionStructureBeyondTheRequestedCountWhenGettingFieldInfoThenItIsNotInspected) {
    uint32_t count = 1;
    auto fieldInfos = makeFieldInfos(3);
    uint32_t extension = 0;
    fieldInfos[1].pNext = &extension;

    EXPECT_EQ(ZE_RESULT_SUCCESS, CperDecoder::getFieldInfo(&count, fieldInfos.data()));
    EXPECT_EQ(1u, count);
    EXPECT_STREQ("sig_id", fieldInfos[0].name);
}

TEST(SysmanInfoLogCperDecoderTest, GivenTheSampleRecordWhenDecodingThenSigIdAndUuidAreReturned) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, sigId));
    EXPECT_EQ(9u, sigId);

    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);
}

TEST(SysmanInfoLogCperDecoderTest, GivenASectionWhoseHeaderDoesNotMarkTheSigIdValidWhenDecodingThenTheSigIdIsNotAvailable) {
    MockCperRecord builder;
    builder.addSectionWithInvalidSigId(MockCperRecord::acceleratorGuid, 64, 20, 9);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0xbadu;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));
    EXPECT_EQ(0xbadu, sigId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenASectionWhoseHeaderDoesNotMarkTheSigIdValidWhenDecodingThenTheUuidIsStillAvailable) {
    MockCperRecord builder;
    builder.addSectionWithFruIdAndInvalidSigId(MockCperRecord::acceleratorGuid, 64, 20, 9, MockCperRecord::sampleFruId);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));

    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);
}

TEST(SysmanInfoLogCperDecoderTest, GivenASectionTooShortToHoldTheValidationBitsWhenDecodingThenTheSigIdIsNotAvailable) {
    MockCperRecord builder;
    builder.addSection(MockCperRecord::acceleratorGuid, 24, 20, 9);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));
}

TEST(SysmanInfoLogCperDecoderTest, GivenAFruIdWhenDecodingThenTheUuidKeepsTheByteOrderTheRecordStoresItIn) {
    MockCperRecord builder;
    builder.addSectionWithFruId(MockCperRecord::acceleratorGuid, 64, 20, 7, MockCperRecord::sampleFruId);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    MockCperRecord::Guid uuid = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);

    MockCperRecord::Guid mixedEndian = MockCperRecord::sampleFruId;
    std::reverse(mixedEndian.begin(), mixedEndian.begin() + 4);
    std::reverse(mixedEndian.begin() + 4, mixedEndian.begin() + 6);
    std::reverse(mixedEndian.begin() + 6, mixedEndian.begin() + 8);
    EXPECT_NE(mixedEndian, uuid);
}

TEST(SysmanInfoLogCperDecoderTest, GivenASectionWithoutAValidFruIdWhenDecodingThenOnlyTheUuidIsNotAvailable) {
    MockCperRecord builder;
    builder.addSection(MockCperRecord::acceleratorGuid, 64, 20, 5);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, sigId));
    EXPECT_EQ(5u, sigId);

    MockCperRecord::Guid uuid;
    uuid.fill(unwritten);
    size_t bytesCopied = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE,
              batch.readField(CperDecoder::uuidField, 0, uuid.data(), uuid.size(), bytesCopied));
    EXPECT_EQ(0u, bytesCopied);
    EXPECT_EQ(unwritten, uuid[0]);
}

TEST(SysmanInfoLogCperDecoderTest, GivenRecordsInAnOrderDifferentFromTheDescriptorsWhenDecodingThenEachRecordIsReadInDescriptorOrder) {
    MockCperRecord first;
    first.addSection(MockCperRecord::acceleratorGuid, 64, 20, 101);
    MockCperRecord second;
    second.addSection(MockCperRecord::acceleratorGuid, 96, 20, 102);

    ReadOutput records;
    records.add(first.build());
    records.add(second.build());
    std::swap(records.descriptors[0], records.descriptors[1]);

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, sigId));
    EXPECT_EQ(102u, sigId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(1, sigId));
    EXPECT_EQ(101u, sigId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenARecordWithSeveralSectionsWhenDecodingThenTheFirstKnownSectionIsUsedForEveryField) {
    MockCperRecord builder;
    builder.addSection(MockCperRecord::unknownGuid, 64, 20, 1);
    builder.addSectionWithFruId(MockCperRecord::acceleratorGuid, 64, 20, 55, MockCperRecord::sampleFruId);
    builder.addSection(MockCperRecord::acceleratorGuid, 64, 20, 66);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, sigId));
    EXPECT_EQ(55u, sigId);

    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);
}

TEST(SysmanInfoLogCperDecoderTest, GivenRecordsWhichCarryNoFieldWhenDecodingThenTheyReportNotAvailableAndTheOtherRecordsAreDecoded) {
    MockCperRecord unknownSection;
    unknownSection.addSection(MockCperRecord::unknownGuid, 64, 20, 1);
    MockCperRecord noSections;
    MockCperRecord shortSection;
    shortSection.addSection(MockCperRecord::acceleratorGuid, 20, 20, 2);
    MockCperRecord good;
    good.addSection(MockCperRecord::acceleratorGuid, 64, 20, 77);

    auto noSignature = good.build();
    noSignature[0] = 'X';
    auto shortHeader = std::vector<uint8_t>(64, 0);
    auto badRecordLength = good.build();
    MockCperRecord::writeU32(badRecordLength, MockCperRecord::recordLengthOffset, 0xffffu);
    auto sectionOutOfBounds = good.build();
    MockCperRecord::writeU32(sectionOutOfBounds, MockCperRecord::recordHeaderSize + MockCperRecord::sectionOffsetOffset, 0xff00u);

    ReadOutput records;
    records.add(unknownSection.build());
    records.add(noSections.build());
    records.add(shortSection.build());
    records.add(noSignature);
    records.add(shortHeader);
    records.add(badRecordLength);
    records.add(sectionOutOfBounds);
    records.add(good.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    for (uint32_t r = 0; r < records.recordCount() - 1; r++) {
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(r, sigId)) << "record " << r;
        MockCperRecord::Guid uuid = {};
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readUuid(r, uuid)) << "record " << r;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(records.recordCount() - 1, sigId));
    EXPECT_EQ(77u, sigId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenARecordWhoseLengthIsBelowTheCperHeaderSizeWhenDecodingThenNoFieldIsAvailable) {
    MockCperRecord builder;
    builder.addSectionWithFruId(MockCperRecord::acceleratorGuid, 64, 20, 77, MockCperRecord::sampleFruId);
    auto record = builder.build();
    MockCperRecord::writeU32(record, MockCperRecord::recordLengthOffset, MockCperRecord::recordHeaderSize - 1u);

    ReadOutput records;
    records.add(record);

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readUuid(0, uuid));
}

TEST(SysmanInfoLogCperDecoderTest, GivenARecordWhoseSectionDescriptorsDoNotFitInItsLengthWhenDecodingThenNoFieldIsAvailable) {
    MockCperRecord builder;
    auto record = builder.build();
    MockCperRecord::writeU16(record, MockCperRecord::sectionCountOffset, 1u);

    ReadOutput records;
    records.add(record);

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readUuid(0, uuid));
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnOutputBufferSmallerThanTheFieldWhenReadingAFieldThenInvalidSizeIsReturnedAndNothingIsWritten) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    std::vector<uint8_t> output(ZES_MAX_UUID_SIZE, unwritten);
    size_t bytesCopied = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE,
              batch.readField(CperDecoder::sigIdField, 0, output.data(), sizeof(uint32_t) - 1, bytesCopied));
    EXPECT_EQ(0u, bytesCopied);
    EXPECT_EQ(unwritten, output[0]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE,
              batch.readField(CperDecoder::uuidField, 0, output.data(), ZES_MAX_UUID_SIZE - 1, bytesCopied));
    EXPECT_EQ(0u, bytesCopied);
    EXPECT_EQ(unwritten, output[0]);
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnOutputBufferLargerThanTheFieldWhenReadingAFieldThenOnlyTheFieldIsWritten) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    std::vector<uint8_t> output(sizeof(uint32_t) * 4, unwritten);
    size_t bytesCopied = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS,
              batch.readField(CperDecoder::sigIdField, 0, output.data(), output.size(), bytesCopied));
    EXPECT_EQ(sizeof(uint32_t), bytesCopied);

    uint32_t sigId = 0;
    std::memcpy(&sigId, output.data(), sizeof(sigId));
    EXPECT_EQ(9u, sigId);
    EXPECT_EQ(unwritten, output[sizeof(uint32_t)]);
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnUnknownFieldIdWhenReadingAFieldThenInvalidArgumentIsReturnedAndNothingIsWritten) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    const std::array<uint32_t, 3> unknownFieldIds = {CperDecoder::fieldCount,
                                                     CperDecoder::fieldCount + 1u,
                                                     std::numeric_limits<uint32_t>::max()};
    for (auto fieldId : unknownFieldIds) {
        std::vector<uint8_t> output(ZES_MAX_UUID_SIZE, unwritten);
        size_t bytesCopied = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT,
                  batch.readField(fieldId, 0, output.data(), output.size(), bytesCopied))
            << "field " << fieldId;
        EXPECT_EQ(0u, bytesCopied) << "field " << fieldId;
        EXPECT_EQ(unwritten, output[0]) << "field " << fieldId;
    }
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnUnknownFieldIdWhenTheBufferIsTooSmallAndTheRecordCarriesNoKnownSectionThenInvalidArgumentIsReturned) {
    MockCperRecord builder;
    builder.addSection(MockCperRecord::unknownGuid, 64, 20, 1);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    std::vector<uint8_t> output(1, unwritten);
    size_t bytesCopied = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT,
              batch.readField(CperDecoder::fieldCount, 0, output.data(), output.size(), bytesCopied));
    EXPECT_EQ(0u, bytesCopied);
    EXPECT_EQ(unwritten, output[0]);
}

TEST(SysmanInfoLogCperDecoderTest, GivenADescriptorOutsideTheBufferWhenValidatingThenInvalidArgumentIsReturned) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());
    records.add(MockCperRecord::sampleRecord());
    records.descriptors[1].lengthOfData = records.size();

    DecodedBatch batch;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, batch.decode(records));
}

TEST(SysmanInfoLogCperDecoderTest, GivenADescriptorOutsideTheBufferWhenReadingAFieldThenNotAvailableIsReturned) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));
    records.descriptors[0].lengthOfData = records.size() + 1u;

    uint32_t sigId = 0xbadu;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));
    EXPECT_EQ(0xbadu, sigId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenZeroRecordsWhenValidatingThenSuccessIsReturned) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, CperDecoder::validateRecords(0, 0, nullptr));
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnUnalignedRecordAndOutputBufferWhenReadingAFieldThenTheValuesAreCorrect) {
    ReadOutput records;
    records.buffer.push_back(0);
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    std::vector<uint8_t> output(sizeof(uint32_t) + 1u, unwritten);
    size_t bytesCopied = 0;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.readField(CperDecoder::sigIdField, 0, output.data() + 1, sizeof(uint32_t), bytesCopied));
    EXPECT_EQ(sizeof(uint32_t), bytesCopied);

    uint32_t sigId = 0;
    std::memcpy(&sigId, output.data() + 1, sizeof(sigId));
    EXPECT_EQ(9u, sigId);
}

TEST(SysmanInfoLogCperDecoderTest, GivenAnUnmodifiedBufferWhenReadingTheSameFieldRepeatedlyThenEveryReadReturnsTheSameValue) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    for (uint32_t attempt = 0; attempt < 4u; attempt++) {
        uint32_t sigId = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, sigId)) << "attempt " << attempt;
        EXPECT_EQ(9u, sigId) << "attempt " << attempt;

        MockCperRecord::Guid uuid = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid)) << "attempt " << attempt;
        EXPECT_EQ(MockCperRecord::sampleFruId, uuid) << "attempt " << attempt;
    }
}

TEST(SysmanInfoLogCperDecoderTest, GivenASectionTooShortForTheSigIdButWithAValidFruIdWhenReadingThenOnlyTheUuidIsAvailable) {
    MockCperRecord builder;
    builder.addSectionWithFruId(MockCperRecord::acceleratorGuid, 16, 20, 9, MockCperRecord::sampleFruId);

    ReadOutput records;
    records.add(builder.build());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t sigId = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, batch.readSigId(0, sigId));

    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);
}

TEST(SysmanInfoLogCperDecoderTest, GivenADecodedBatchWhenReadingTheSameFieldTwiceInAnyOrderThenTheSameValuesAreReturned) {
    ReadOutput records;
    records.add(MockCperRecord::sampleRecord());

    DecodedBatch batch;
    ASSERT_EQ(ZE_RESULT_SUCCESS, batch.decode(records));

    uint32_t firstSigId = 0;
    uint32_t secondSigId = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, firstSigId));
    MockCperRecord::Guid uuid = {};
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readUuid(0, uuid));
    EXPECT_EQ(ZE_RESULT_SUCCESS, batch.readSigId(0, secondSigId));

    EXPECT_EQ(firstSigId, secondSigId);
    EXPECT_EQ(9u, firstSigId);
    EXPECT_EQ(MockCperRecord::sampleFruId, uuid);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
