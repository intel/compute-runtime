/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"
#include "test.h"

TEST(IsDeviceBinaryFormatAr, GivenValidBinaryReturnTrue) {
    auto emptyArchive = ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Archive>(emptyArchive));
}

TEST(IsDeviceBinaryFormatAr, GivenInvalidBinaryReturnTrue) {
    const uint8_t binary[] = "not_ar";
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Archive>(binary));
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToDecodeArThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_STREQ("Not an AR archive - mismatched file signature", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToFindMatchingBinariesThenUnpackingFails) {
    auto emptyArchive = ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size());
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(emptyArchive, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_STREQ("Couldn't find matching binary in AR archive", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryAr, WhenMultipleBinariesMatchedThenChooseBestMatch) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(4U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[2].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[2].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBestMatchIsntFullMatchThenChooseBestMatchButEmitWarnings) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_FALSE(unpackWarnings.empty());
    EXPECT_STREQ("Couldn't find perfectly matched binary (right stepping) in AR, using best usable", unpackWarnings.c_str());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(3U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToUnpackBestMatchThenTryUnpackingAnyUsable) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    PatchTokensTestData::ValidEmptyProgram programTokensWrongTokenVersion;
    programTokensWrongTokenVersion.headerMutable->Version -= 1;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokensWrongTokenVersion.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_FALSE(unpackWarnings.empty());
    EXPECT_STREQ("Couldn't find perfectly matched binary (right stepping) in AR, using best usable", unpackWarnings.c_str());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(4U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenCouldFindBinaryWithRightPointerSizeThenUnpackingFails) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    std::string wrongPointerSize = (programTokens.header->GPUPointerSizeInBytes == 8) ? "32" : "64";
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpacked.format);
    EXPECT_TRUE(unpacked.deviceBinary.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_STREQ("Couldn't find matching binary in AR archive", unpackErrors.c_str());
}
