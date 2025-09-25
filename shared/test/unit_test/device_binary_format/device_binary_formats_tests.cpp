/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

TEST(DecodeError, WhenStringRepresentationIsNeededThenAsStringEncodesProperly) {
    EXPECT_STREQ("decoded successfully", NEO::asString(NEO::DecodeError::success));
    EXPECT_STREQ("in undefined status", NEO::asString(NEO::DecodeError::undefined));
    EXPECT_STREQ("with invalid binary", NEO::asString(NEO::DecodeError::invalidBinary));
    EXPECT_STREQ("with unhandled binary", NEO::asString(NEO::DecodeError::unhandledBinary));
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

TEST(IsAnyDeviceBinaryFormat, GivenZebinFormatThenReturnsTrue) {
    ZebinTestData::ValidEmptyProgram zebinProgram;
    EXPECT_TRUE(NEO::isAnyDeviceBinaryFormat(zebinProgram.storage));
}

TEST(GetTargetDeviceTests, givenAILAvailableAndUseLegacyValidationLogicReturningTrueWhenGettingTargetDeviceThenSetApplyValidationWaFlagToTrue) {
    NEO::MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.ailConfiguration.reset(new MockAILConfiguration());
    auto mockAIL = static_cast<MockAILConfiguration *>(rootDeviceEnvironment.ailConfiguration.get());

    auto targetDevice = getTargetDevice(rootDeviceEnvironment);
    EXPECT_FALSE(targetDevice.applyValidationWorkaround);

    mockAIL->fallbackToLegacyValidationLogic = true;
    targetDevice = getTargetDevice(rootDeviceEnvironment);
    EXPECT_TRUE(targetDevice.applyValidationWorkaround);
}

TEST(GetTargetDeviceTests, givenDoNotUseProductConfigForValidationWaFlagSetToTrueWhenGettingTargetDeviceThenSetApplyValidationWaFlagToTrue) {
    DebugManagerStateRestore restore;
    NEO::MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.ailConfiguration.reset(nullptr);

    auto targetDevice = getTargetDevice(rootDeviceEnvironment);
    EXPECT_FALSE(targetDevice.applyValidationWorkaround);

    NEO::debugManager.flags.DoNotUseProductConfigForValidationWa.set(true);
    targetDevice = getTargetDevice(rootDeviceEnvironment);
    EXPECT_TRUE(targetDevice.applyValidationWorkaround);
}

TEST(UnpackSingleDeviceBinary, GivenUnknownBinaryThenReturnError) {
    const uint8_t data[] = "none of known formats";
    auto requestedProductAbbreviation = "unk";
    NEO::TargetDevice requestedTargetDevice;
    std::string outErrors;
    std::string outWarnings;
    auto unpacked = NEO::unpackSingleDeviceBinary(data, requestedProductAbbreviation, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.deviceBinary.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpacked.format);
    EXPECT_EQ(0U, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(0U, unpacked.targetDevice.stepping);
    EXPECT_EQ(4U, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty());
    EXPECT_STREQ("Unknown format", outErrors.c_str());
}

TEST(UnpackSingleDeviceBinary, GivenPatchtoknsBinaryThenReturnSelf) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    auto requestedProductAbbreviation = "unk";
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
    EXPECT_EQ(NEO::DeviceBinaryFormat::patchtokens, unpacked.format);
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

    auto requestedProductAbbreviation = "unk";
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
    EXPECT_EQ(NEO::DeviceBinaryFormat::patchtokens, unpacked.format);
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

    auto requiredProduct = NEO::hardwarePrefix[productFamily];
    std::string requiredStepping = std::to_string(patchtokensProgram.header->SteppingId);
    std::string requiredPointerSize = (patchtokensProgram.header->GPUPointerSizeInBytes == 4) ? "32" : "64";
    NEO::Ar::ArEncoder arEnc(true);
    ASSERT_TRUE(arEnc.appendFileEntry(requiredPointerSize + "." + requiredProduct + "." + requiredStepping, elfData));
    auto arData = arEnc.encode();

    auto unpacked = NEO::unpackSingleDeviceBinary(arData, requiredProduct, requestedTargetDevice, outErrors, outWarnings);
    EXPECT_TRUE(unpacked.buildOptions.empty());
    EXPECT_TRUE(unpacked.debugData.empty());
    EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
    EXPECT_EQ(NEO::DeviceBinaryFormat::patchtokens, unpacked.format);
    EXPECT_EQ(requestedTargetDevice.coreFamily, unpacked.targetDevice.coreFamily);
    EXPECT_EQ(requestedTargetDevice.stepping, unpacked.targetDevice.stepping);
    EXPECT_EQ(patchtokensProgram.header->GPUPointerSizeInBytes, unpacked.targetDevice.maxPointerSizeInBytes);
    EXPECT_TRUE(outWarnings.empty()) << outWarnings;
    EXPECT_TRUE(outErrors.empty()) << outErrors;

    EXPECT_FALSE(unpacked.deviceBinary.empty());
    ASSERT_EQ(patchtokensProgram.storage.size(), unpacked.deviceBinary.size());
    EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), unpacked.deviceBinary.begin(), unpacked.deviceBinary.size()));
}

TEST(UnpackSingleDeviceBinary, GivenZebinThenReturnSelf) {
    ZebinTestData::ValidEmptyProgram zebin64BitProgram;
    ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin32BitProgram;

    auto requestedProductAbbreviation = "unk";
    NEO::TargetDevice requestedTargetDevice;
    requestedTargetDevice.productFamily = static_cast<PRODUCT_FAMILY>(zebin64BitProgram.elfHeader->machine);
    requestedTargetDevice.stepping = 0U;
    requestedTargetDevice.maxPointerSizeInBytes = 8U;
    std::string outErrors;
    std::string outWarnings;

    for (auto zebin : {&zebin64BitProgram.storage, &zebin32BitProgram.storage}) {
        auto unpacked = NEO::unpackSingleDeviceBinary(*zebin, requestedProductAbbreviation, requestedTargetDevice, outErrors, outWarnings);
        EXPECT_TRUE(unpacked.buildOptions.empty());
        EXPECT_TRUE(unpacked.debugData.empty());
        EXPECT_FALSE(unpacked.deviceBinary.empty());
        EXPECT_EQ(zebin->data(), unpacked.deviceBinary.begin());
        EXPECT_EQ(zebin->size(), unpacked.deviceBinary.size());
        EXPECT_TRUE(unpacked.intermediateRepresentation.empty());
        EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, unpacked.format);
        EXPECT_EQ(requestedTargetDevice.coreFamily, unpacked.targetDevice.coreFamily);
        EXPECT_EQ(requestedTargetDevice.stepping, unpacked.targetDevice.stepping);
        EXPECT_EQ(8U, unpacked.targetDevice.maxPointerSizeInBytes);
        EXPECT_TRUE(outWarnings.empty());
        EXPECT_TRUE(outErrors.empty());
    }
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

TEST(IsAnyPackedDeviceBinaryFormat, GivenZebinFormatThenReturnsTrue) {
    ZebinTestData::ValidEmptyProgram zebinProgram;
    EXPECT_TRUE(NEO::isAnyPackedDeviceBinaryFormat(zebinProgram.storage));
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

TEST(IsAnySingleDeviceBinaryFormat, GivenZebinFormatThenReturnsTrue) {
    ZebinTestData::ValidEmptyProgram zebinProgram;
    EXPECT_TRUE(NEO::isAnySingleDeviceBinaryFormat(zebinProgram.storage));
}

TEST(DecodeSingleDeviceBinary, GivenUnknownFormatThenReturnFalse) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    const uint8_t data[] = "none of known formats";
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = data;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Unknown format", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinary, GivenPatchTokensFormatThenDecodingSucceeds) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = patchtokensProgram.storage;
    bin.targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::patchtokens, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_TRUE(decodeErrors.empty());
}

TEST(DecodeSingleDeviceBinary, GivenZebinFormatThenDecodingSucceeds) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebinElf;
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = zebinElf.storage;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, format);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinary, GivenZebinWithExternalFunctionsThenDecodingSucceedsAndLinkerInputIsSet) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ZebinWithExternalFunctionsInfo zebinElf;
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = zebinElf.storage;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, format);
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_NE(nullptr, programInfo.linkerInput.get());
    EXPECT_EQ(1, programInfo.linkerInput->getExportedFunctionsSegmentId());
}

TEST(DecodeSingleDeviceBinary, GivenOclElfFormatThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
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
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::oclElf, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Device binary format is packed", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinary, GivenArFormatThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::Ar::ArEncoder arEnc;
    auto arData = arEnc.encode();
    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = arData;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::archive, format);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_STREQ("Device binary format is packed", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinary, GivenBindlessKernelInZebinWhenDecodingThenKernelDescriptorInitilizesBindlessOffsetToSurfaceIndex) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

    std::string validZeInfo = std::string("version :\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name : kernel_bindless
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            4
          arg_index:       0
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "kernel_bindless", {kernelIsa, sizeof(kernelIsa)});
    zebin.elfHeader->machine = NEO::defaultHwInfo->platform.eProductFamily;

    NEO::ProgramInfo programInfo;
    std::string decodeErrors;
    std::string decodeWarnings;
    NEO::SingleDeviceBinary bin;
    bin.deviceBinary = zebin.storage;
    NEO::DecodeError status;
    NEO::DeviceBinaryFormat format;
    std::tie(status, format) = NEO::decodeSingleDeviceBinary(programInfo, bin, decodeErrors, decodeWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, status);
    EXPECT_EQ(NEO::DeviceBinaryFormat::zebin, format);
    EXPECT_TRUE(decodeWarnings.empty());

    ASSERT_EQ(1u, programInfo.kernelInfos.size());

    EXPECT_TRUE(NEO::KernelDescriptor::isBindlessAddressingKernel(programInfo.kernelInfos[0]->kernelDescriptor));
    EXPECT_EQ(1u, programInfo.kernelInfos[0]->kernelDescriptor.bindlessArgsMap.size());
}

TEST(PackDeviceBinary, GivenRequestToPackThenUsesOclElfFormat) {
    NEO::SingleDeviceBinary deviceBinary;

    std::string packErrors;
    std::string packWarnings;
    auto packed = NEO::packDeviceBinary(deviceBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packed));
}

TEST(PackDeviceBinary, GivenRequestToPackWhenFormatIsAlreadyPackedThenReturnsInput) {
    NEO::SingleDeviceBinary deviceBinary;

    std::string packErrors;
    std::string packWarnings;
    auto packed = NEO::packDeviceBinary(deviceBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packed));
    deviceBinary.deviceBinary = packed;
    auto packed2 = NEO::packDeviceBinary(deviceBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packed));
    EXPECT_EQ(packed, packed2);
}
