/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>
#include <tuple>

TEST(IsDeviceBinaryFormatOclElf, GivenElfThenReturnsTrueIfProperElfFileTypeDetected) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_OBJECTS;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    EXPECT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_DEBUG;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_SOURCE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_NONE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_EXEC;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode()));

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_32> elfEnc32;
    elfEnc32.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    EXPECT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(elfEnc32.encode()));
}

TEST(UnpackSingleDeviceBinaryOclElf, WhenFailedToDecodeElfThenUnpackingFails) {
    std::string unpackErrors;
    std::string unpackWarnings;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>({}, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
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
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unsupported OCL ELF file type", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfThenSetsProperOutputFormat) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    auto elfData = elfEnc64.encode();
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(elfData, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::oclLibrary, unpackResult.format);
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
    unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(elfData, "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::oclCompiledObject, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_TRUE(unpackErrors.empty());
    EXPECT_FALSE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(NEO::isLlvmBitcode(unpackResult.intermediateRepresentation));
    EXPECT_EQ(NEO::llvmBcMagic.size(), unpackResult.intermediateRepresentation.size());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfExecutableThenUnpackingFails) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unsupported OCL ELF file type", unpackErrors.c_str());
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfWithBuildOptionsThenBuildOptionsAreExtracted) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    const std::string buildOptionsStr = "-cl-opt-disable -DFOO=1";
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, NEO::Elf::SectionNamesOpenCl::buildOptions,
                           ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(buildOptionsStr.c_str()), buildOptionsStr.size()));
    auto encodedElf = elfEnc64.encode();
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(encodedElf, "", {}, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    EXPECT_FALSE(unpackResult.buildOptions.empty());
    EXPECT_EQ(0, memcmp(buildOptionsStr.c_str(), unpackResult.buildOptions.begin(), buildOptionsStr.size()));
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfWithDebugDataThenDebugDataIsExtracted) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, NEO::Elf::SectionNamesOpenCl::spirvObject, ArrayRef<const uint8_t>::fromAny(NEO::spirvMagic.begin(), NEO::spirvMagic.size()));
    const uint8_t debugPayload[] = {0x01, 0x02, 0x03, 0x04};
    elfEnc64.appendSection(NEO::Elf::SHT_OPENCL_DEV_DEBUG, NEO::Elf::SectionNamesOpenCl::deviceDebug,
                           ArrayRef<const uint8_t>(debugPayload, sizeof(debugPayload)));
    auto encodedElf = elfEnc64.encode();
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(encodedElf, "", {}, unpackErrors, unpackWarnings);
    EXPECT_TRUE(unpackErrors.empty()) << unpackErrors;
    EXPECT_TRUE(unpackWarnings.empty()) << unpackWarnings;
    ASSERT_EQ(sizeof(debugPayload), unpackResult.debugData.size());
    EXPECT_EQ(0, memcmp(debugPayload, unpackResult.debugData.begin(), sizeof(debugPayload)));
}

TEST(UnpackSingleDeviceBinaryOclElf, GivenOclElfWithUnhandledSectionThenUnpackingFails) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    std::string unpackErrors;
    std::string unpackWarnings;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_LIBRARY;
    elfEnc64.appendSection(NEO::Elf::SHT_NOBITS, "my_data", {});
    auto unpackResult = NEO::unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(elfEnc64.encode(), "", {}, unpackErrors, unpackWarnings);
    EXPECT_EQ(NEO::DeviceBinaryFormat::unknown, unpackResult.format);
    EXPECT_TRUE(unpackResult.deviceBinary.empty());
    EXPECT_TRUE(unpackResult.debugData.empty());
    EXPECT_TRUE(unpackResult.intermediateRepresentation.empty());
    EXPECT_TRUE(unpackResult.buildOptions.empty());
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_STREQ("Unhandled ELF section", unpackErrors.c_str());
}

TEST(DecodeSingleDeviceBinaryOclElf, WhenUsedAsSingleDeviceBinaryThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEnc64;
    elfEnc64.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    auto elfData = elfEnc64.encode();

    NEO::SingleDeviceBinary deviceBinary;
    deviceBinary.deviceBinary = elfData;

    std::string unpackErrors;
    std::string unpackWarnings;
    NEO::ProgramInfo programInfo;
    NEO::DecodeError error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(programInfo, deviceBinary, unpackErrors, unpackWarnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_TRUE(unpackWarnings.empty());
    EXPECT_FALSE(unpackErrors.empty());
    EXPECT_STREQ("Device binary format is packed", unpackErrors.c_str());
}

TEST(PackDeviceBinaryOclElf, WhenPackingEmptyDataThenEmptyOclElfIsEmitted) {
    NEO::SingleDeviceBinary singleBinary;

    std::string packErrors;
    std::string packWarnings;
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(singleBinary, packErrors, packWarnings);
    EXPECT_TRUE(packWarnings.empty());
    EXPECT_TRUE(packErrors.empty());

    ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packedData));

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
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(singleBinary, packErrors, packWarnings);
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
        auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(singleBinary, packErrors, packWarnings);
        EXPECT_FALSE(packedData.empty());
        EXPECT_TRUE(packWarnings.empty());
        EXPECT_TRUE(packErrors.empty());

        ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packedData));

        std::string decodeElfErrors;
        std::string decodeElfWarnings;
        auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
        EXPECT_TRUE(decodeElfErrors.empty());
        EXPECT_TRUE(decodeElfWarnings.empty());
        ASSERT_NE(nullptr, elf.elfFileHeader);

        EXPECT_EQ(NEO::Elf::ET_OPENCL_EXECUTABLE, elf.elfFileHeader->type);
        EXPECT_EQ(3U, elf.elfFileHeader->shNum);

        auto spirVSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                         [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_SPIRV; });
        ASSERT_NE(nullptr, spirVSection);
        ASSERT_EQ(spirV.size(), spirVSection->data.size());
        EXPECT_EQ(0, memcmp(spirV.begin(), spirVSection->data.begin(), spirV.size()));
    }

    {
        singleBinary.intermediateRepresentation = ArrayRef<const uint8_t>::fromAny(llvmBc.begin(), llvmBc.size());

        std::string packErrors;
        std::string packWarnings;
        auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(singleBinary, packErrors, packWarnings);
        EXPECT_FALSE(packedData.empty());
        EXPECT_TRUE(packWarnings.empty());
        EXPECT_TRUE(packErrors.empty());

        ASSERT_TRUE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::oclElf>(packedData));

        std::string decodeElfErrors;
        std::string decodeElfWarnings;
        auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
        EXPECT_TRUE(decodeElfErrors.empty());
        EXPECT_TRUE(decodeElfWarnings.empty());
        ASSERT_NE(nullptr, elf.elfFileHeader);

        EXPECT_EQ(NEO::Elf::ET_OPENCL_EXECUTABLE, elf.elfFileHeader->type);
        EXPECT_EQ(3U, elf.elfFileHeader->shNum);

        auto llvmSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                        [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_LLVM_BINARY; });
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
    auto packedData = NEO::packDeviceBinary<NEO::DeviceBinaryFormat::oclElf>(singleBinary, packErrors, packWarnings);
    EXPECT_TRUE(packErrors.empty());
    EXPECT_TRUE(packWarnings.empty());

    std::string decodeElfErrors;
    std::string decodeElfWarnings;
    auto elf = NEO::Elf::decodeElf(packedData, decodeElfErrors, decodeElfWarnings);
    EXPECT_TRUE(decodeElfErrors.empty());
    EXPECT_TRUE(decodeElfWarnings.empty());
    ASSERT_NE(nullptr, elf.elfFileHeader);

    auto spirvSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                     [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_SPIRV; });

    auto deviceBinarySection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                            [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_DEV_BINARY; });

    auto deviceDebugSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                           [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_DEV_DEBUG; });

    auto buildOptionsSection = std::find_if(elf.sectionHeaders.begin(), elf.sectionHeaders.end(),
                                            [](const SectionHeaderAndData<> &section) { return section.header->type == NEO::Elf::SHT_OPENCL_OPTIONS; });

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
