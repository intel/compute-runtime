/*
 * Copyright (C) 2020-2026 Intel Corporation
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
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

TEST(IsDeviceBinaryFormatAr, GivenValidBinaryThenReturnTrue) {
    auto emptyArchive = ArrayRef<const uint8_t>::fromAny(NEO::Ar::arMagic.begin(), NEO::Ar::arMagic.size());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::archive>(emptyArchive));
}

TEST(IsDeviceBinaryFormatAr, GivenInvalidBinaryThenReturnTrue) {
    const uint8_t binary[] = "not_ar";
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::archive>(binary));
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToDecodeArThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
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
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(emptyArchive, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_STREQ("Couldn't find matching binary in AR archive", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryAr, WhenBinaryWithProductConfigIsFoundThenChooseItAsABestMatch) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.aotConfig = aotConfig;
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(4U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[1].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[1].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBinaryWithProductConfigIsFoundThenPackedTargetDeviceBinaryIsSet) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.aotConfig = aotConfig;
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_NE(0U, unpacked.packedTargetDeviceBinary.size());
}

TEST(UnpackSingleDeviceBinaryAr, WhenMultipleBinariesMatchedThenChooseBestMatch) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);

    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(4U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[3].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[3].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBestMatchWithProductFamilyIsntFullMatchThenChooseBestMatchButEmitWarnings) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
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
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenBestMatchWithProductConfigIsntFullMatchThenChooseBestMatchButEmitWarnings) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
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
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToUnpackMatchWithProductConfigThenTryUnpackAnyUsable) {
    ZebinTestData::ValidEmptyProgram zebin;
    // invalidBinary must be padded to 8 bytes in the AR so that the subsequent zebin entry
    // (file index 3) lands at an 8-byte aligned offset, satisfying ElfFileHeader alignment.
    const uint8_t invalidBinary[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);

    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, invalidBinary));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty());

    unpackErrors.clear();
    unpackWarnings.clear();
    auto decodedAr = NEO::Ar::decodeAr(arData, unpackErrors, unpackWarnings);
    EXPECT_NE(nullptr, decodedAr.magic);
    ASSERT_EQ(5U, decodedAr.files.size());
    EXPECT_EQ(unpacked.deviceBinary.begin(), decodedAr.files[3].fileData.begin());
    EXPECT_EQ(unpacked.deviceBinary.size(), decodedAr.files[3].fileData.size());
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenFailedToUnpackBestMatchWithProductFamilyThenTryUnpackingAnyUsable) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t invalidBinary[] = {0xFF, 0xFF, 0xFF};
    NEO::Ar::ArEncoder encoder;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, invalidBinary));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
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
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButIrWithProductFamilyAvailableThenUseIr) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredPointerSize = "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, ArrayRef<const uint8_t>(elfData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButIrWithProductConfigAvailableThenUseIr) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredPointerSize = "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProductConfig, ArrayRef<const uint8_t>(elfData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;
    target.aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryNotMatchedButGenericIrFileAvailableThenUseGenericIr) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredPointerSize = "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const auto spirvFile{ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size())};
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFile);

    const auto elfData = elfEncoder.encode();
    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, GivenInvalidGenericIrFileWhenDeviceBinaryNotMatchedButGenericIrFileAvailableThenIrIsEmpty) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const auto elfData = elfEncoder.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryMatchedButHasNoIrAndGenericIrFileAvailableThenUseBinaryWithAssignedGenericIr) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoderIr;
    elfEncoderIr.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;

    const std::string customSprivContent{"\x07\x23\x02\x03This is a custom file, with SPIR-V magic!"};
    const auto spirvFile{ArrayRef<const uint8_t>::fromAny(customSprivContent.data(), customSprivContent.size())};
    elfEncoderIr.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, spirvFile);

    const auto elfIrData = elfEncoderIr.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfIrData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    ASSERT_FALSE(unpacked.intermediateRepresentation.empty());
    ASSERT_EQ(customSprivContent.size(), unpacked.intermediateRepresentation.size());

    const auto isSpirvSameAsInGenericIr = std::memcmp(customSprivContent.data(), unpacked.intermediateRepresentation.begin(), customSprivContent.size()) == 0;
    EXPECT_TRUE(isSpirvSameAsInGenericIr);

    EXPECT_FALSE(unpacked.deviceBinary.empty());
}

TEST(UnpackSingleDeviceBinaryAr, WhenDeviceBinaryMatchedAlreadyHasIrAndGenericIrFileAvailableThenGenericIrIsIgnored) {
    ZebinTestData::ValidEmptyProgram zebin;
    const auto spirvContent = ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size());
    zebin.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, "spirv", spirvContent);

    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoderIr;
    elfEncoderIr.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;
    const std::string differentSpirv{"\x07\x23\x02\x03 different-spirv-in-generic-ir"};
    elfEncoderIr.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject,
                               ArrayRef<const uint8_t>::fromAny(differentSpirv.data(), differentSpirv.size()));
    auto elfIrData = elfEncoderIr.encode();
    ASSERT_TRUE(encoder.appendFileEntry("generic_ir", ArrayRef<const uint8_t>(elfIrData)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    ASSERT_FALSE(unpacked.deviceBinary.empty());
    ASSERT_FALSE(unpacked.intermediateRepresentation.empty());
    ASSERT_EQ(NEO::spirvMagic.size(), unpacked.intermediateRepresentation.size());
    EXPECT_EQ(0, memcmp(NEO::spirvMagic.begin(), unpacked.intermediateRepresentation.begin(), NEO::spirvMagic.size()));
}

TEST(UnpackSingleDeviceBinaryAr, WhenOnlyIrIsAvailableThenUseOneFromBestMatchedBinary) {
    ZebinTestData::ValidEmptyProgram zebin;
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64Best;
    elfEnc64Best.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64Best.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::llvmBcMagic.begin(), NEO::llvmBcMagic.size()));
    auto elfDataBest = elfEnc64Best.encode();

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64Second;
    elfEnc64Second.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64Second.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfDataSecond = elfEnc64Second.encode();

    NEO::Ar::ArEncoder encoder{true};
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct, ArrayRef<const uint8_t>(elfDataSecond)));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, ArrayRef<const uint8_t>(elfDataBest)));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;

    EXPECT_FALSE(unpacked.intermediateRepresentation.empty());
    EXPECT_TRUE(NEO::isLlvmBitcode(unpacked.intermediateRepresentation));
}

TEST(UnpackSingleDeviceBinaryAr, WhenCouldNotFindBinaryWithRightPointerSizeThenUnpackingFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::Ar::ArEncoder encoder;

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &compilerProductHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::CompilerProductHelper>();
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = compilerProductHelper.getHwIpVersion(hwInfo);

    std::string requiredProductConfig = ProductConfigHelper::parseMajorMinorRevisionValue(aotConfig);
    std::string requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = "0";
    std::string requiredPointerSize = "64";
    std::string wrongPointerSize = "32";

    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + "." + requiredProduct + "." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(requiredPointerSize + "unk." + requiredStepping, zebin.storage));
    ASSERT_TRUE(encoder.appendFileEntry(wrongPointerSize + requiredProductConfig, zebin.storage));

    NEO::TargetDevice target;
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;
    target.aotConfig = aotConfig;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requiredProduct, target, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpacked.format);
    EXPECT_TRUE(unpacked.deviceBinary.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_STREQ("Couldn't find matching binary in AR archive", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryAr, WhenRequestedDeviceHasCompatibleFallbackThenUseFallbackDevice) {
    ZebinTestData::ValidEmptyProgram zebin;

    std::string requestedProduct = "lnl";
    std::string fallbackProduct = "bmg";

    {
        auto compatibleDevices = ProductConfigHelper::getCompatibilityFallbackProductAbbreviations(requestedProduct);

        if (compatibleDevices.empty() ||
            std::find(compatibleDevices.begin(), compatibleDevices.end(), fallbackProduct) == compatibleDevices.end()) {
            GTEST_SKIP();
        }
    }

    NEO::Ar::ArEncoder encoder{true};
    std::string pointerSize = "64";

    ASSERT_TRUE(encoder.appendFileEntry(pointerSize + "." + fallbackProduct, zebin.storage));

    NEO::TargetDevice target{};
    target.productFamily = static_cast<PRODUCT_FAMILY>(zebin.elfHeader->machine);
    target.stepping = 0U;
    target.maxPointerSizeInBytes = 8U;

    auto arData = encoder.encode();
    std::string unpackErrors;
    std::string unpackWarnings;

    auto unpacked = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::archive>(arData, requestedProduct, target, unpackErrors, unpackWarnings);

    EXPECT_TRUE(unpackErrors.empty());
    EXPECT_FALSE(unpacked.deviceBinary.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
    EXPECT_STREQ("Couldn't find perfectly matched binary in AR, using best usable", unpackWarnings.c_str());
}

TEST(ProductConfigHelper, GivenUnknownDeviceWhenGettingCompatibilityFallbacksThenReturnEmpty) {
    const std::string requestedProduct = "nonexistent_device_acronym_xyz";

    auto result = ProductConfigHelper::getCompatibilityFallbackProductAbbreviations(requestedProduct);

    EXPECT_TRUE(result.empty());
}
