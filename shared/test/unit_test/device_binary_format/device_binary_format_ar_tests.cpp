/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/ar/ar.h"
#include "shared/source/device_binary_format/ar/ar_decoder.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"

TEST(IsDeviceBinaryFormatAr, GivenValidBinaryThenReturnTrue) {
    auto emptyArchive = ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Archive>(emptyArchive));
}

TEST(IsDeviceBinaryFormatAr, GivenInvalidBinaryThenReturnTrue) {
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

TEST(UnpackSingleDeviceBinaryAr, WhenBinaryWithProductConfigIsFoundThenChooseItAsABestMatch) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.aotConfig = aotConfig;
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
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBinaryWithProductConfigIsFoundThenPackedTargetDeviceBinaryIsSet) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.aotConfig = aotConfig;
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_NE(0U, unpacked.packedTargetDeviceBinary.size());
}

TEST(UnpackSingleDeviceBinaryAr, WhenMultipleBinariesMatchedThenChooseBestMatch) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);
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
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[3].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[3].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBestMatchWithProductFamilyIsntFullMatchThenChooseBestMatchButEmitWarnings) {
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
    EXPECT_STREQ("Couldn't find perfectly matched binary in AR, using best usable", unpackWarnings.c_str());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(3U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBestMatchWithProductConfigIsntFullMatchThenChooseBestMatchButEmitWarnings) {
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
    EXPECT_STREQ("Couldn't find perfectly matched binary in AR, using best usable", unpackWarnings.c_str());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(3U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToUnpackMatchWithProductConfigThenTryUnpackAnyUsable) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    PatchTokensTestData::ValidEmptyProgram programTokensWrongTokenVersion;

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);

    programTokensWrongTokenVersion.headerMutable->Version -= 1;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, programTokensWrongTokenVersion.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(5U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[3].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[3].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToUnpackBestMatchWithProductFamilyThenTryUnpackingAnyUsable) {
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
    EXPECT_STREQ("Couldn't find perfectly matched binary in AR, using best usable", unpackWarnings.c_str());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(4U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButIrWithProductFamilyAvailableThenUseIr) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, ArrayRef<const uint8_t>(elfData)));

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

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButIrWithProductConfigAvailableThenUseIr) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, ArrayRef<const uint8_t>(elfData)));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;
    target.aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButGenericIrFileAvailableThenUseGenericIr) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const auto spirvFile{ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size())};
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFile);

    const auto elfData = elfEncoder.encode();
    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfData)));

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

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, GivenInvalidGenericIrFileWhenDeviceBinaryNotMatchedButGenericIrFileAvailableThenIrIsEmpty) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const auto elfData = elfEncoder.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfData)));

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

    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryMatchedButHasNoIrAndGenericIrFileAvailableThenUseBinaryWithAssignedGenericIr) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoderIr;
    elfEncoderIr.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const std::string customSprivContent{"\x07\x23\x02\x03This is a custom file, with SPIR-V magic!"};
    const auto spirvFile{ArrayRef<const uint8_t>::fromAny(customSprivContent.data(), customSprivContent.size())};
    elfEncoderIr.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFile);

    const auto elfIrData = elfEncoderIr.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfIrData)));

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

    ASSERT_FALSE(unpacked.intermediateRepresentation.empty());
    ASSERT_EQ(customSprivContent.size(), unpacked.intermediateRepresentation.size());

    const auto isSpirvSameAsInGenericIr = std::memcmp(customSprivContent.data(), unpacked.intermediateRepresentation.begin(), customSprivContent.size()) == 0;
    EXPECT_TRUE(isSpirvSameAsInGenericIr);

    EXPECT_FALSE(unpacked.deviceBinary.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryMatchedAndHasIrAndGenericIrFileAvailableThenUseBinaryAndItsIr) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoderBinary;
    elfEncoderBinary.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;

    const std::string customSprivContentBinary{"\x07\x23\x02\x03This is a binary's IR!"};
    const auto spirvFileBinary{ArrayRef<const uint8_t>::fromAny(customSprivContentBinary.data(), customSprivContentBinary.size())};
    elfEncoderBinary.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFileBinary);
    elfEncoderBinary.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, programTokens.storage);

    NEO::Ar::ArEncoder encoder{true};
    const auto elfBinaryData = elfEncoderBinary.encode();
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, ArrayRef<const uint8_t>(elfBinaryData)));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoderIr;
    elfEncoderIr.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const std::string customSprivContentGenericIr{"\x07\x23\x02\x03This is a generic ir!"};
    const auto spirvFile{ArrayRef<const uint8_t>::fromAny(customSprivContentGenericIr.data(), customSprivContentGenericIr.size())};
    elfEncoderIr.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFile);

    const auto elfIrData = elfEncoderIr.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfIrData)));

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

    ASSERT_FALSE(unpacked.intermediateRepresentation.empty());
    ASSERT_EQ(customSprivContentBinary.size(), unpacked.intermediateRepresentation.size());

    const auto isSpirvSameAsInBinary = std::memcmp(customSprivContentBinary.data(), unpacked.intermediateRepresentation.begin(), customSprivContentBinary.size()) == 0;
    EXPECT_TRUE(isSpirvSameAsInBinary);

    EXPECT_FALSE(unpacked.deviceBinary.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenOnlyIrIsAvailableThenUseOneFromBestMatchedBinary) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64Best;
    elfEnc64Best.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64Best.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::llvmBcMagic.begin(), NEO::llvmBcMagic.size()));
    auto elfDataBest = elfEnc64Best.encode();

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64Second;
    elfEnc64Second.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64Second.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfDataSecond = elfEnc64Second.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, ArrayRef<const uint8_t>(elfDataSecond)));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, ArrayRef<const uint8_t>(elfDataBest)));

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

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
    EXPECT_TRUE(NEO::isLlvmBitcode(unpacked.intermediateRepresentation));
}

TEST(UnpackSingleDeviceBinaryAr, WhenCouldNotFindBinaryWithRightPointerSizeThenUnpackingFails) {
    PatchTokensTestData::ValidEmptyProgram programTokens;
    NEO::Ar::ArEncoder encoder;

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(productFamily);
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = hwInfoConfig.getProductConfigFromHwInfo(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(programTokens.header->SteppingId);
    std::string requiredPointerSize = (programTokens.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    std::string wrongPointerSize = (programTokens.header->GPUPointerSizeInBytes == 8) ? "32" : "64";

    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct + "." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, programTokens.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + requiredProductConfig, programTokens.storage));

    NEO::TargetDevice target;
    target.coreFamily = static_cast<GFXCORE_FAMILY>(programTokens.header->Device);
    target.stepping = programTokens.header->SteppingId;
    target.maxPointerSizeInBytes = programTokens.header->GPUPointerSizeInBytes;
    target.aotConfig = aotConfig;

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
