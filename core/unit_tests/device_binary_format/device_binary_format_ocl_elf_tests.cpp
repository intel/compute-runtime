/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/intermediate_representations.h"
#include "core/device_binary_format/device_binary_formats.h"
#include "core/device_binary_format/elf/elf_decoder.h"
#include "core/device_binary_format/elf/elf_encoder.h"
#include "core/device_binary_format/elf/ocl_elf.h"
#include "core/program/program_info.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "test.h"

#include <algorithm>
#include <tuple>

TEST(IsDeviceBinaryFormatOclElf, GivenElfThenReturnsTrueIfProperElfFileTypeDetected) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_DEBUG;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_SOURCE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_NONE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_EXEC;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode()));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_32> elfEnc32;
    elfEnc32.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(elfEnc32.encode()));
}

TEST(UnpackSingleDeviceBinaryOclElf, WhenFailedToDecodeElfThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Invalid or missing ELF header", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenNotOclElfThenUnpackingFails) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Not OCL ELF file type", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfThenSetsProperOutputFormat) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfData, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::OclLibrary, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(NEO::isSpirVBitcode(unpackResult.intermediateRepresentation));
    EXPECT_EQ(NEO::spirvMagic.size(), unpackResult.intermediateRepresentation.size());

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_LLVM_BINARY, NEO::Elf::SectionNamesOpenCl::llvmObject, ArrayRef<const uint8_t>::fromAny(NEO::llvmBcMagic.begin(), NEO::llvmBcMagic.size()));
    elfData = elfEnc64.encode();
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfData, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::OclCompiledObject, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(NEO::isLlvmBitcode(unpackResult.intermediateRepresentation));
    EXPECT_EQ(NEO::llvmBcMagic.size(), unpackResult.intermediateRepresentation.size());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenValidOclElfExecutableThenReadsAllSectionProperly) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    targetDevice.stepping = patchtokensProgram.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = patchtokensProgram.header->GPUPointerSizeInBytes;

    const uint8_t intermediateRepresentation[] = "235711";
    const uint8_t debugData[] = "313739";
    std::string buildOptions = "buildOpts";
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, patchtokensProgram.storage);
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions, buildOptions);
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_DEBUG, NEO::Elf::SectionNamesOpenCl::buildOptions, debugData);
    {
        auto encWithLlvm = elfEnc64;
        encWithLlvm.appendSection(NEO::Elf::SHT_OPENCL_LLVM_BINARY, NEO::Elf::SectionNamesOpenCl::llvmObject, intermediateRepresentation);
        auto elfData = encWithLlvm.encode();
        auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfData, "", targetDevice, unpackErrors, unpackWarnings);
        EXPECT_TRUE(unpackWarnings.empty());
        EXPECT_TRUE(unpackErrors.empty());
        EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpackResult.format);
        ASSERT_EQ(patchtokensProgram.storage.size(), unpackResult.deviceBinary.size());
        ASSERT_EQ(sizeof(debugData), unpackResult.debugData.size());
        ASSERT_EQ(buildOptions.size() + 1, unpackResult.buildOptions.size());
        ASSERT_EQ(sizeof(intermediateRepresentation), unpackResult.intermediateRepresentation.size());

        EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), unpackResult.deviceBinary.begin(), unpackResult.deviceBinary.size()));
        EXPECT_STREQ(buildOptions.c_str(), unpackResult.buildOptions.begin());
        EXPECT_EQ(0, memcmp(debugData, unpackResult.debugData.begin(), unpackResult.debugData.size()));
        EXPECT_EQ(0, memcmp(intermediateRepresentation, unpackResult.intermediateRepresentation.begin(), unpackResult.intermediateRepresentation.size()));
    }
    {
        auto encWithSpirV = elfEnc64;
        encWithSpirV.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, intermediateRepresentation);
        auto elfData = encWithSpirV.encode();
        auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfData, "", targetDevice, unpackErrors, unpackWarnings);
        EXPECT_TRUE(unpackWarnings.empty());
        EXPECT_TRUE(unpackErrors.empty());
        EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpackResult.format);
        ASSERT_EQ(patchtokensProgram.storage.size(), unpackResult.deviceBinary.size());
        ASSERT_EQ(sizeof(debugData), unpackResult.debugData.size());
        ASSERT_EQ(buildOptions.size() + 1, unpackResult.buildOptions.size());
        ASSERT_EQ(sizeof(intermediateRepresentation), unpackResult.intermediateRepresentation.size());

        EXPECT_EQ(0, memcmp(patchtokensProgram.storage.data(), unpackResult.deviceBinary.begin(), unpackResult.deviceBinary.size()));
        EXPECT_STREQ(buildOptions.c_str(), unpackResult.buildOptions.begin());
        EXPECT_EQ(0, memcmp(debugData, unpackResult.debugData.begin(), unpackResult.debugData.size()));
        EXPECT_EQ(0, memcmp(intermediateRepresentation, unpackResult.intermediateRepresentation.begin(), unpackResult.intermediateRepresentation.size()));
    }
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfExecutableWithUnhandledSectionThenUnpackingFails) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64.appendSection(NEO::Elf::SHT_NOBITS, "my_data", {});
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::Unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unhandled ELF section", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfExecutableWhenPatchtokensBinaryIsBrokenThenReadsAllSectionProperly) {
    const uint8_t intermediateRepresentation[] = "235711";
    const uint8_t debugData[] = "313739";
    const uint8_t deviceBinary[] = "not_patchtokens";
    std::string buildOptions = "buildOpts";
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, deviceBinary);
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions, buildOptions);
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_DEBUG, NEO::Elf::SectionNamesOpenCl::buildOptions, debugData);
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, intermediateRepresentation);

    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Invalid program header", unpackErrors.c_str());
    EXPECT_EQ(NEO::DeviceBinaryFormat::Patchtokens, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    EXPECT_FALSE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
}

TEST(DecodeSingleDeviceBinaryOclElf, WhenUsedAsSingleDeviceBinaryThenDecodingFails) {
    PatchTokensTestData::ValidEmptyProgram patchtokensProgram;
    ;

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, NEO::Elf::SectionNamesOpenCl::deviceBinary, patchtokensProgram.storage);
    auto elfData = elfEnc64.encode();

    NEO::TargetDevice targetDevice;
    targetDevice.coreFamily = static_cast<GFXCORE_FAMILY>(patchtokensProgram.header->Device);
    targetDevice.stepping = patchtokensProgram.header->SteppingId;
    targetDevice.maxPointerSizeInBytes = patchtokensProgram.header->GPUPointerSizeInBytes;

    NEO::SingleDeviceBinary deviceBinary;
    deviceBinary.targetDevice = targetDevice;
    deviceBinary.deviceBinary = elfData;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::ProgramInfo programInfo;
    NEO::DecodeError error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(programInfo, deviceBinary, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Device binary format is packed", unpackErrors.c_str());
}

TEST(PackDeviceBinaryOclElf, WhenPackingEmptyDataThenEmptyOclElfIsEmitted) {
    NEO::SingleDeviceBinary singleBinary;

    std::string packErrors;
    std::string packWarnings;
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(singleBinary, packErrors, packWarnings);
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(packErrors.empty());

    ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(packedData));

    std::string decodeElfErrors;
    std::string decodeElfWarnings;
    auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
    EXPECT_TRUE(decodeElfErrors.empty());
    EXPECT_TRUE(decodeElfWarnings.empty());
    ASSERT_NE(nullptr, elf.elfFileHeader);

    EXPECT_EQ(NEO::Elf::ET_OPENCL_EXECUTABLE, elf.elfFileHeader->type);
    EXPECT_EQ(0U, elf.elfFileHeader->shNum);
}

TEST(PackDeviceBinaryOclElf, WhenPackingBinaryWithUnknownIntermediateRepresentationThenFail) {
    const uint8_t intermediateRepresentation[] = "not_llvm_and_not_spirv";
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.intermediateRepresentation = intermediateRepresentation;

    std::string packErrors;
    std::string packWarnings;
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(singleBinary, packErrors, packWarnings);
    EXPECT_TRUE(packedData.empty());
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_FALSE(packErrors.empty());
    EXPECT_STREQ("Unknown intermediate representation format", packErrors.c_str());
}

TEST(PackDeviceBinaryOclElf, WhenPackingBinaryWitIntermediateRepresentationThenChoosesProperSectionBasedOnMagic) {
    using namespace NEO::Elf;
    auto spirV = NEO::spirvMagic;
    auto llvmBc = NEO::llvmBcMagic;
    NEO::SingleDeviceBinary singleBinary;

    {
        singleBinary.intermediateRepresentation = ArrayRef<const uint8_t>::fromAny(spirV.begin(), spirV.size());

        std::string packErrors;
        std::string packWarnings;
        auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(singleBinary, packErrors, packWarnings);
        EXPECT_FALSE(packedData.empty());
        EXPECT_TRUE(packWarnings.empty());
        EXPECT_TRUE(packErrors.empty());

        ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(packedData));

        std::string decodeElfErrors;
        std::string decodeElfWarnings;
        auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
        EXPECT_TRUE(decodeElfErrors.empty());
        EXPECT_TRUE(decodeElfWarnings.empty());
        ASSERT_NE(nullptr, elf.elfFileHeader);

        EXPECT_EQ(NEO::Elf::ET_OPENCL_EXECUTABLE, elf.elfFileHeader->type);
        EXPECT_EQ(3U, elf.elfFileHeader->shNum);

        auto spirVSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                         [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_SPIRV; });
        ASSERT_NE(nullptr, spirVSection);
        ASSERT_EQ(spirV.size(), spirVSection->data.size());
        EXPECT_EQ(0, memcmp(spirV.begin(), spirVSection->data.begin(), spirV.size()));
    }

    {
        singleBinary.intermediateRepresentation = ArrayRef<const uint8_t>::fromAny(llvmBc.begin(), llvmBc.size());

        std::string packErrors;
        std::string packWarnings;
        auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(singleBinary, packErrors, packWarnings);
        EXPECT_FALSE(packedData.empty());
        EXPECT_TRUE(packWarnings.empty());
        EXPECT_TRUE(packErrors.empty());

        ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(packedData));

        std::string decodeElfErrors;
        std::string decodeElfWarnings;
        auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
        EXPECT_TRUE(decodeElfErrors.empty());
        EXPECT_TRUE(decodeElfWarnings.empty());
        ASSERT_NE(nullptr, elf.elfFileHeader);

        EXPECT_EQ(NEO::Elf::ET_OPENCL_EXECUTABLE, elf.elfFileHeader->type);
        EXPECT_EQ(3U, elf.elfFileHeader->shNum);

        auto llvmSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                        [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_LLVM_BINARY; });
        ASSERT_NE(nullptr, llvmSection);
        ASSERT_EQ(llvmBc.size(), llvmSection->data.size());
        EXPECT_EQ(0, memcmp(llvmBc.begin(), llvmSection->data.begin(), llvmBc.size()));
    }
}

TEST(PackDeviceBinaryOclElf, WhenPackingBinaryThenSectionsAreProperlyPopulated) {
    using namespace NEO::Elf;
    NEO::SingleDeviceBinary singleBinary;

    auto spirV = NEO::spirvMagic;
    const uint8_t debugData[] = "313739";
    const uint8_t deviceBinary[] = "23571113";
    std::string buildOptions = "buildOpts";
    singleBinary.intermediateRepresentation = ArrayRef<const uint8_t>::fromAny(spirV.begin(), spirV.size());
    singleBinary.debugData = debugData;
    singleBinary.deviceBinary = deviceBinary;
    singleBinary.buildOptions = buildOptions;

    std::string packErrors;
    std::string packWarnings;
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(singleBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());

    std::string decodeElfErrors;
    std::string decodeElfWarnings;
    auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
    EXPECT_TRUE(decodeElfErrors.empty());
    EXPECT_TRUE(decodeElfWarnings.empty());
    ASSERT_NE(nullptr, elf.elfFileHeader);

    auto spirvSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                     [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_SPIRV; });

    auto deviceBinarySection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                            [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_DEV_BINARY; });

    auto deviceDebugSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                           [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_DEV_DEBUG; });

    auto buildOptionsSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                            [](const Elf<>::SectionHeaderAndData &section) { return section.header->type == NEO::Elf::SHT_OPENCL_OPTIONS; });

    ASSERT_NE(nullptr, spirvSection);
    ASSERT_EQ(spirV.size(), spirvSection->data.size());
    EXPECT_EQ(0, memcmp(spirV.begin(), spirvSection->data.begin(), spirV.size()));

    ASSERT_NE(nullptr, deviceBinarySection);
    ASSERT_EQ(sizeof(deviceBinary), deviceBinarySection->data.size());
    EXPECT_EQ(0, memcmp(deviceBinary, deviceBinarySection->data.begin(), sizeof(deviceBinary)));

    ASSERT_NE(nullptr, deviceDebugSection);
    ASSERT_EQ(sizeof(debugData), deviceDebugSection->data.size());
    EXPECT_EQ(0, memcmp(debugData, deviceDebugSection->data.begin(), sizeof(debugData)));

    ASSERT_NE(nullptr, buildOptionsSection);
    ASSERT_EQ(buildOptions.size(), buildOptionsSection->data.size());
    EXPECT_EQ(0, memcmp(buildOptions.c_str(), buildOptionsSection->data.begin(), buildOptions.size()));
}
