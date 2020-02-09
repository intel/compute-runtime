/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/device_binary_formats.h"
#include "core/program/program_info.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "test.h"

TEST(IsDeviceBinaryFormatPatchtokens, GivenValidBinaryReturnTrue) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Patchtokens>(programTokens.storage));
}

TEST(IsDeviceBinaryFormatPatchtokens, GivenInvalidBinaryReturnTrue) {
    const uint8_t binary[] = "not_patchtokens";
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Patchtokens>(binary));
}

TEST(UnpackSingleDeviceBinaryPatchtokens, WhenFailedToDecodeHeaderThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Invalid program header", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryPatchtokens, WhenValidBinaryAndMatchedWithRequestedTargetDeviceThenReturnSelf) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    targetDevice.stepping = programTokens.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programTokens.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpackResult.format);
    EXPECT_EQ(targetDevice.coreFamily, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(targetDevice.stepping, unpackResult.targetDevice.stepping);
    EXPECT_EQ(targetDevice.maxPointerSizeInBytes, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_FALSE(unpackResult.deviceBinary.empty());
    EXPECT_EQ(programTokens.storage.data(), unpackResult.deviceBinary.begin());
    EXPECT_EQ(programTokens.storage.size(), unpackResult.deviceBinary.size());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
}

TEST(UnpackSingleDeviceBinaryPatchtokens, WhenValidBinaryForDifferentCoreFamilyDeviceThenUnpackingFails) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device + 1);
    targetDevice.stepping = programTokens.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programTokens.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryPatchtokens, WhenValidBinaryWithUnsupportedPatchTokensVerionThenUnpackingFails) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    programTokens.headerMutable->Version += 1;
    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    targetDevice.stepping = programTokens.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programTokens.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryPatchtokens, WhenValidBinaryWithUnsupportedPointerSizeThenUnpackingFails) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    programTokens.headerMutable->GPUPointerSizeInBytes = 8U;
    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    targetDevice.stepping = programTokens.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = 4U;

    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programTokens.storage, "", targetDevice, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, unpackResult.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpackResult.targetDevice.stepping);
    EXPECT_EQ(4U, unpackResult.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Unhandled target device", unpackErrors.c_str());
}

TEST(DecodeSingleDeviceBinaryPatchtokens, GivenInvalidBinaryThenReturnError) {
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("ProgramFromPatchtokens wasn't successfully decoded", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinaryPatchtokens, GivenValidBinaryThenOutputIsProperlyPopulated) {
    PatchTokensTestData::ValidProgramWithKernel programTokens;
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = programTokens.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
}
