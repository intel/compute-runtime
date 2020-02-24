/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/program/program_info.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

#include "test.h"

TEST(DecodeError, WhenStringRepresentationIsNeededThenAsStringEncodesProperly) {
    EXPECT_STREQ("decoded successfully", NEO::asString(NEO::DecodeError::Success));
    EXPECT_STREQ("in undefined status", NEO::asString(NEO::DecodeError::Undefined));
    EXPECT_STREQ("with invalid binary", NEO::asString(NEO::DecodeError::InvalidBinary));
    EXPECT_STREQ("with unhandled binary", NEO::asString(NEO::DecodeError::UnhandledBinary));
}

TEST(IsAnyDeviceBinaryFormat, GivenNoneOfKnownFormatsThenReturnsFalse) {
    const uint8_t data[] = "none of known formats";
    EXPECT_FALSE(NEO::isAnyDeviceBinaryFormat(data));
}

TEST(IsAnyDeviceBinaryFormat, GivenPatchTokensFormatThenReturnsTrue) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    EXPECT_TRUE(NEO::isAnyDeviceBinaryFormat(patchtokensProgram.storage));
}

TEST(IsAnyDeviceBinaryFormat, GivenOclElfFormatThenReturnsTrue) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_TRUE(NEO::isAnyDeviceBinaryFormat(elfEnc.encode()));
}

TEST(IsAnyDeviceBinaryFormat, GivenArFormatThenReturnsTrue) {
    EXPECT_TRUE(NEO::isAnyDeviceBinaryFormat(ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size())));
}

TEST(UnpackSingleDeviceBinary, GivenUnknownBinaryThenReturnError) {
    const uint8_t data[] = "none of known formats";
    ConstStringRef requestedProductAbbreviation = "unk";
    NEO::TargetDevice requestedTargetDevice;
    std::string outErrors;
    std::string outWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary(data, requestedProductAbbreviation, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.deviceBinary.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpacked.format);
    EXPECT_EQ(0U, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpacked.targetDevice.stepping);
    EXPECT_EQ(4U, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty());
    EXPECT_STREQ("Unknown format", outErrors.c_str());
}

TEST(UnpackSingleDeviceBinary, GivenPatchtoknsBinaryThenReturnSelf) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    ConstStringRef requestedProductAbbreviation = "unk";
    NEO::TargetDevice requestedTargetDevice;
    requestedTargetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    requestedTargetDevice.stepping = patchtokensProgram.header->SteppingId;
    requestedTargetDevice.maxPointerSizeInBytes = patchtokensProgram.header->GPUPointerSizeInBytes;
    std::string outErrors;
    std::string outWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary(patchtokensProgram.storage, requestedProductAbbreviation, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_FALSE(unpacked.deviceBinary.empty());
    EXPECT_EQ(patchtokensProgram.storage.data(), unpacked.deviceBinary.begin());
    EXPECT_EQ(patchtokensProgram.storage.size(), unpacked.deviceBinary.size());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
    EXPECT_EQ(requestedTargetDevice.coreFamily, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(requestedTargetDevice.stepping, unpacked.targetDevice.stepping);
    EXPECT_EQ(patchtokensProgram.header->GPUPointerSizeInBytes, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty());
    EXPECT_TRUE(outErrors.empty());
}

TEST(UnpackSingleDeviceBinary, GivenOclElfBinaryThenReturnPatchtokensBinary) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, patchtokensProgram.storage);

    ConstStringRef requestedProductAbbreviation = "unk";
    NEO::TargetDevice requestedTargetDevice;
    requestedTargetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    requestedTargetDevice.stepping = patchtokensProgram.header->SteppingId;
    requestedTargetDevice.maxPointerSizeInBytes = patchtokensProgram.header->GPUPointerSizeInBytes;
    std::string outErrors;
    std::string outWarnings;
    auto elfData = elfEnc.encode();
    auto unpacked = NEO::unpackSingleDeviceBinary(elfData, requestedProductAbbreviation, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
    EXPECT_EQ(requestedTargetDevice.coreFamily, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(requestedTargetDevice.stepping, unpacked.targetDevice.stepping);
    EXPECT_EQ(patchtokensProgram.header->GPUPointerSizeInBytes, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty());
    EXPECT_TRUE(outErrors.empty());

    EXPECT_FALSE(unpacked.deviceBinary.empty());
    ASSERT_EQ(patchtokensProgram.storage.size(), unpacked.deviceBinary.size());
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), unpacked.deviceBinary.begin(), unpacked.deviceBinary.size()));
}

TEST(UnpackSingleDeviceBinary, GivenArBinaryWithOclElfThenReturnPatchtokensBinary) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, patchtokensProgram.storage);

    NEO::TargetDevice requestedTargetDevice;
    requestedTargetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    requestedTargetDevice.stepping = patchtokensProgram.header->SteppingId;
    requestedTargetDevice.maxPointerSizeInBytes = patchtokensProgram.header->GPUPointerSizeInBytes;
    std::string outErrors;
    std::string outWarnings;
    auto elfData = elfEnc.encode();

    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(patchtokensProgram.header->SteppingId);
    std::string requiredPointerSize = (patchtokensProgram.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    NEO::Ar::ArEncoder arEnc(true);
    ASSERT_TRUE(arEnc.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, elfData));
    auto arData = arEnc.encode();

    auto unpacked = NEO::unpackSingleDeviceBinary(arData, requiredProduct, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
    EXPECT_EQ(requestedTargetDevice.coreFamily, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(requestedTargetDevice.stepping, unpacked.targetDevice.stepping);
    EXPECT_EQ(patchtokensProgram.header->GPUPointerSizeInBytes, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty()) << outWarnings;
    EXPECT_TRUE(outErrors.empty()) << outErrors;

    EXPECT_FALSE(unpacked.deviceBinary.empty());
    ASSERT_EQ(patchtokensProgram.storage.size(), unpacked.deviceBinary.size());
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), unpacked.deviceBinary.begin(), unpacked.deviceBinary.size()));
}

TEST(IsAnyPackedDeviceBinaryFormat, GivenUnknownFormatThenReturnFalse) {
    const uint8_t data[] = "none of known formats";
    EXPECT_FALSE(NEO::isAnyPackedDeviceBinaryFormat(data));
}

TEST(IsAnyPackedDeviceBinaryFormat, GivenPatchTokensFormatThenReturnsFalse) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    EXPECT_FALSE(NEO::isAnyPackedDeviceBinaryFormat(patchtokensProgram.storage));
}

TEST(IsAnyPackedDeviceBinaryFormat, GivenOclElfFormatThenReturnsTrue) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_TRUE(NEO::isAnyPackedDeviceBinaryFormat(elfEnc.encode()));
}

TEST(IsAnyPackedDeviceBinaryFormat, GivenArFormatThenReturnsTrue) {
    EXPECT_TRUE(NEO::isAnyPackedDeviceBinaryFormat(ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size())));
}

TEST(IsAnySingleDeviceBinaryFormat, GivenUnknownFormatThenReturnFalse) {
    const uint8_t data[] = "none of known formats";
    EXPECT_FALSE(NEO::isAnySingleDeviceBinaryFormat(data));
}

TEST(IsAnySingleDeviceBinaryFormat, GivenPatchTokensFormatThenReturnsTrue) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    EXPECT_TRUE(NEO::isAnySingleDeviceBinaryFormat(patchtokensProgram.storage));
}

TEST(IsAnySingleDeviceBinaryFormat, GivenOclElfFormatThenReturnsFalse) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_FALSE(NEO::isAnySingleDeviceBinaryFormat(elfEnc.encode()));
}

TEST(IsAnySingleDeviceBinaryFormat, GivenArFormatThenReturnsFalse) {
    EXPECT_FALSE(NEO::isAnySingleDeviceBinaryFormat(ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size())));
}

TEST(DecodeSingleDeviceBinary, GivenUnknownFormatThenReturnFalse) {
    const uint8_t data[] = "none of known formats";
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = data;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Unknown format", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinary, GivenPatchTokensFormatThenDecodingSucceeds) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = patchtokensProgram.storage;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
}

TEST(DecodeSingleDeviceBinary, GivenOclElfFormatThenDecodingFails) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc;
    elfEnc.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, patchtokensProgram.storage);

    auto elfData = elfEnc.encode();
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = elfData;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::OclElf, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Device binary format is packed", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinary, GivenArFormatThenDecodingFails) {
    NEO::Ar::ArEncoder arEnc;
    auto arData = arEnc.encode();
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = arData;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Archive, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Device binary format is packed", decodeErrors.c_str());
}

TEST(PackDeviceBinary, GivenRequestToPackThenUsesOclElfFormat) {
    NEO::SingleDeviceBinary deviceBinary;

    std::string packErrors;
    std::string packWarnings;
    auto packed = NEO::packDeviceBinary(deviceBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(packed));
}
