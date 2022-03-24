/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

#include <vector>

TEST(ExtractZebinSections, WhenElfDoesNotContainValidStringSectionThenFail) {
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> elfHeader;
    elf.elfFileHeader = &elfHeader;

    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    elfHeader.shStrNdx = NEO::Elf::SHN_UNDEF;
    elf.sectionHeaders.resize(1);
    auto decodeError = NEO::extractZebinSections(elf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid or missing shStrNdx in elf header\n", errors.c_str());
    elf.sectionHeaders.clear();

    errors.clear();
    warnings.clear();
    elfHeader.shStrNdx = elfHeader.shNum + 1;
    decodeError = NEO::extractZebinSections(elf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid or missing shStrNdx in elf header\n", errors.c_str());
}

TEST(ExtractZebinSections, GivenUnknownElfSectionThenFail) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_RESERVED_START, "someSection", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elfDecodeErrors;
    std::string elfDecodeWarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elfDecodeErrors, elfDecodeWarnings);

    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = NEO::extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    auto expectedError = "DeviceBinaryFormat::Zebin : Unhandled ELF section header type : " + std::to_string(NEO::Elf::SHT_OPENCL_RESERVED_START) + "\n";
    EXPECT_STREQ(expectedError.c_str(), errors.c_str());
}

TEST(ExtractZebinSections, GivenUnknownElfProgbitsSectionThenFail) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, "someSection", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elfDecodeErrors;
    std::string elfDecodeWarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elfDecodeErrors, elfDecodeWarnings);

    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = NEO::extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    auto expectedError = "DeviceBinaryFormat::Zebin : Unhandled SHT_PROGBITS section : someSection currently supports only : .text.KERNEL_NAME, .data.const, .data.global and .debug_* .\n";
    EXPECT_STREQ(expectedError, errors.c_str());
}

TEST(ExtractZebinSections, GivenKnownSectionsThenCapturesThemProperly) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "someOtherKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConst, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataGlobal, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConstString, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::debugInfo, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::debugAbbrev, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_GTPIN_INFO, NEO::Elf::SectionsNamesZebin::gtpinInfo, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_VISA_ASM, NEO::Elf::SectionsNamesZebin::vIsaAsmPrefix.str() + "someKernel", std::string{});

    elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + "someKernel", std::string{});

    auto encodedElf = elfEncoder.encode();
    std::string elfDecodeErrors;
    std::string elfDecodeWarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elfDecodeErrors, elfDecodeWarnings);

    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = NEO::extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(2U, sections.textKernelSections.size());
    ASSERT_EQ(1U, sections.globalDataSections.size());
    ASSERT_EQ(1U, sections.constDataSections.size());
    ASSERT_EQ(1U, sections.constDataStringSections.size());
    ASSERT_EQ(1U, sections.zeInfoSections.size());
    ASSERT_EQ(1U, sections.symtabSections.size());
    ASSERT_EQ(1U, sections.spirvSections.size());

    auto stringSection = decodedElf.sectionHeaders[decodedElf.elfFileHeader->shStrNdx];
    const char *strings = stringSection.data.toArrayRef<const char>().begin();
    EXPECT_STREQ((NEO::Elf::SectionsNamesZebin::textPrefix.str() + "someKernel").c_str(), strings + sections.textKernelSections[0]->header->name);
    EXPECT_STREQ((NEO::Elf::SectionsNamesZebin::textPrefix.str() + "someOtherKernel").c_str(), strings + sections.textKernelSections[1]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::dataGlobal.data(), strings + sections.globalDataSections[0]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::dataConst.data(), strings + sections.constDataSections[0]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::dataConstString.data(), strings + sections.constDataStringSections[0]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::zeInfo.data(), strings + sections.zeInfoSections[0]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::symtab.data(), strings + sections.symtabSections[0]->header->name);
    EXPECT_STREQ(NEO::Elf::SectionsNamesZebin::spv.data(), strings + sections.spirvSections[0]->header->name);
}

TEST(ExtractZebinSections, GivenMispelledConstDataSectionThenAllowItButEmitError) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, ".data.global_const", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elfDecodeErrors;
    std::string elfDecodeWarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elfDecodeErrors, elfDecodeWarnings);

    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = NEO::extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, decodeError);
    EXPECT_STREQ("Misspelled section name : .data.global_const, should be : .data.const\n", warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(1U, sections.constDataSections.size());

    auto stringSection = decodedElf.sectionHeaders[decodedElf.elfFileHeader->shStrNdx];
    const char *strings = stringSection.data.toArrayRef<const char>().begin();
    EXPECT_STREQ(".data.global_const", strings + sections.constDataSections[0]->header->name);
}

TEST(ValidateZebinSectionsCount, GivenEmptyZebinThenReturnSuccess) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenCorrectNumberOfSectionsThenReturnSuccess) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.constDataSections.resize(1);
    sections.globalDataSections.resize(1);
    sections.spirvSections.resize(1);
    sections.symtabSections.resize(1);
    sections.textKernelSections.resize(10U);
    sections.zeInfoSections.resize(1U);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoConstDataSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.constDataSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .data.const section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoGlobalDataSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.globalDataSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .data.global section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoZeInfoSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.zeInfoSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .ze_info section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoSymtabSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.symtabSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .symtab section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoSpirvSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.spirvSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .spv section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoIntelGTNoteSectionsThenFail) {
    NEO::ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.noteIntelGTSections.resize(2);
    auto err = NEO::validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of .note.intelgt.compat section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateZeInfoVersion, GivenValidVersionFormatThenParsesItProperly) {
    {
        NEO::ConstStringRef yaml = R"===(---
version: '1.0'
...
)===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_EQ(1U, version.major);
        EXPECT_EQ(0U, version.minor);
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '12.91'
    ...
    )===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_EQ(12U, version.major);
        EXPECT_EQ(91U, version.minor);
    }
}

TEST(PopulateZeInfoVersion, GivenInvalidVersionFormatThenParsesItProperly) {
    {
        NEO::ConstStringRef yaml = R"===(---
version: '100'
...
)===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected \'MAJOR.MINOR\' string, got : \'100\'\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '12.'
    ...
    )===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : '12.'\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '.12'
    ...
    )===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : '.12'\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '.'
    ...
    )===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : '.'\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version:
    ...
    )===";

        NEO::ZeInfoKernelSections kernelSections;
        std::string parserErrors;
        std::string parserWarnings;
        NEO::Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &versionNode = *parser.findNodeWithKeyDfs("version");
        std::string errors;
        std::string warnings;
        NEO::Elf::ZebinKernelMetadata::Types::Version version;
        auto err = NEO::populateZeInfoVersion(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(ExtractZeInfoKernelSections, GivenKnownSectionsThenCapturesThemProperly) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env:   
      actual_kernel_start_offset: 0
    debug_env:
      sip_surface_bti: 0
    payload_arguments: 
      - arg_type:        global_id_offset
        offset:          0
        size:            12
    per_thread_payload_arguments: 
      - arg_type:        local_id
        offset:          0
        size:            192
    binding_table_indices: 
      - bti_value:       0
        arg_index:       0
    per_thread_memory_buffers:
      - type:            scratch
        usage:           single_space
        size:            64
...
)===";

    NEO::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string warnings;
    NEO::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", warnings);
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_FALSE(kernelSections.nameNd.empty());
    ASSERT_FALSE(kernelSections.executionEnvNd.empty());
    ASSERT_FALSE(kernelSections.payloadArgumentsNd.empty());
    ASSERT_FALSE(kernelSections.bindingTableIndicesNd.empty());
    ASSERT_FALSE(kernelSections.perThreadPayloadArgumentsNd.empty());
    ASSERT_FALSE(kernelSections.perThreadMemoryBuffersNd.empty());
    ASSERT_TRUE(kernelSections.experimentalPropertiesNd.empty());

    EXPECT_EQ("name", parser.readKey(*kernelSections.nameNd[0])) << parser.readKey(*kernelSections.nameNd[0]).str();
    EXPECT_EQ("execution_env", parser.readKey(*kernelSections.executionEnvNd[0])) << parser.readKey(*kernelSections.executionEnvNd[0]).str();
    EXPECT_EQ("debug_env", parser.readKey(*kernelSections.debugEnvNd[0])) << parser.readKey(*kernelSections.debugEnvNd[0]).str();
    EXPECT_EQ("payload_arguments", parser.readKey(*kernelSections.payloadArgumentsNd[0])) << parser.readKey(*kernelSections.payloadArgumentsNd[0]).str();
    EXPECT_EQ("per_thread_payload_arguments", parser.readKey(*kernelSections.perThreadPayloadArgumentsNd[0])) << parser.readKey(*kernelSections.perThreadPayloadArgumentsNd[0]).str();
    EXPECT_EQ("binding_table_indices", parser.readKey(*kernelSections.bindingTableIndicesNd[0])) << parser.readKey(*kernelSections.bindingTableIndicesNd[0]).str();
    EXPECT_EQ("per_thread_memory_buffers", parser.readKey(*kernelSections.perThreadMemoryBuffersNd[0])) << parser.readKey(*kernelSections.perThreadMemoryBuffersNd[0]).str();
}

TEST(ExtractZeInfoKernelSections, GivenExperimentalPropertyInKnownSectionsThenSectionIsCapturedProperly) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
      actual_kernel_start_offset: 0
    payload_arguments:
      - arg_type:        global_id_offset
        offset:          0
        size:            12
    per_thread_payload_arguments:
      - arg_type:        local_id
        offset:          0
        size:            192
    binding_table_indices:
      - bti_value:       0
        arg_index:       0
    per_thread_memory_buffers:
      - type:            scratch
        usage:           single_space
        size:            64
    experimental_properties:
      - has_non_kernel_arg_load: 1
        has_non_kernel_arg_store: 1
        has_non_kernel_arg_atomic: 0
...
)===";

    NEO::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string warnings;
    NEO::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", warnings);
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_FALSE(kernelSections.nameNd.empty());
    ASSERT_FALSE(kernelSections.executionEnvNd.empty());
    ASSERT_FALSE(kernelSections.payloadArgumentsNd.empty());
    ASSERT_FALSE(kernelSections.bindingTableIndicesNd.empty());
    ASSERT_FALSE(kernelSections.perThreadPayloadArgumentsNd.empty());
    ASSERT_FALSE(kernelSections.perThreadMemoryBuffersNd.empty());
    ASSERT_FALSE(kernelSections.experimentalPropertiesNd.empty());

    EXPECT_EQ("name", parser.readKey(*kernelSections.nameNd[0])) << parser.readKey(*kernelSections.nameNd[0]).str();
    EXPECT_EQ("execution_env", parser.readKey(*kernelSections.executionEnvNd[0])) << parser.readKey(*kernelSections.executionEnvNd[0]).str();
    EXPECT_EQ("payload_arguments", parser.readKey(*kernelSections.payloadArgumentsNd[0])) << parser.readKey(*kernelSections.payloadArgumentsNd[0]).str();
    EXPECT_EQ("per_thread_payload_arguments", parser.readKey(*kernelSections.perThreadPayloadArgumentsNd[0])) << parser.readKey(*kernelSections.perThreadPayloadArgumentsNd[0]).str();
    EXPECT_EQ("binding_table_indices", parser.readKey(*kernelSections.bindingTableIndicesNd[0])) << parser.readKey(*kernelSections.bindingTableIndicesNd[0]).str();
    EXPECT_EQ("per_thread_memory_buffers", parser.readKey(*kernelSections.perThreadMemoryBuffersNd[0])) << parser.readKey(*kernelSections.perThreadMemoryBuffersNd[0]).str();
    EXPECT_EQ("experimental_properties", parser.readKey(*kernelSections.experimentalPropertiesNd[0])) << parser.readKey(*kernelSections.experimentalPropertiesNd[0]).str();
}

TEST(ExtractZeInfoKernelSections, GivenUnknownSectionThenEmitsAWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    apple : 
        - red
        - green
...
)===";

    NEO::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string warnings;
    NEO::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", warnings);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"apple\" in context of : some_kernel\n", warnings.c_str());
    ASSERT_FALSE(kernelSections.nameNd.empty());
    EXPECT_EQ("name", parser.readKey(*kernelSections.nameNd[0])) << parser.readKey(*kernelSections.nameNd[0]).str();
}

TEST(ValidateZeInfoKernelSectionsCount, GivenCorrectNumberOfSectionsThenReturnSuccess) {
    std::string errors;
    std::string warnings;
    NEO::ZeInfoKernelSections kernelSections;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);

    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    kernelSections.bindingTableIndicesNd.resize(1);
    kernelSections.payloadArgumentsNd.resize(1);
    kernelSections.perThreadPayloadArgumentsNd.resize(1);
    kernelSections.perThreadMemoryBuffersNd.resize(1);
    kernelSections.debugEnvNd.resize(1);

    errors.clear();
    warnings.clear();
    err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, WhenNameSectionIsMissingThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.executionEnvNd.resize(1);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of name section, got : 0\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, WhenExecutionEnvSectionIsMissingThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of execution_env section, got : 0\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoNameSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(2);
    kernelSections.executionEnvNd.resize(1);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of name section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoExecutionEnvSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of execution_env section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoDebugEnvSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.debugEnvNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of debug_env section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoBindingTableIndicesSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.bindingTableIndicesNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of binding_table_indices section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPayloadArgumentsSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.payloadArgumentsNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of payload_arguments section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPerThreadPayloadArgumentsSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.perThreadPayloadArgumentsNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of per_thread_payload_arguments section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPerThreadMemoryBuffersSectionsThenFail) {
    NEO::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.perThreadMemoryBuffersNd.resize(2);
    auto err = NEO::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at most 1 of per_thread_memory_buffers section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ReadZeExperimentalProperties, GivenYamlWithLoadExperimentalPropertyEntryThenExperimentalPropertiesAreCorrectlyRead) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    experimental_properties:
      - has_non_kernel_arg_load: 1
        has_non_kernel_arg_store: 0
        has_non_kernel_arg_atomic: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &experimentalPropertiesNode = *parser.findNodeWithKeyDfs("experimental_properties");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::readZeInfoExperimentalProperties(parser,
                                                     experimentalPropertiesNode,
                                                     experimentalProperties,
                                                     "some_kernel",
                                                     errors,
                                                     warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(experimentalProperties.hasNonKernelArgLoad, 1);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgStore, 0);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgAtomic, 0);
}

TEST(ReadZeExperimentalProperties, GivenYamlWithStoreExperimentalPropertyEntryThenExperimentalPropertiesAreCorrectlyRead) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    experimental_properties:
      - has_non_kernel_arg_load: 0
        has_non_kernel_arg_store: 1
        has_non_kernel_arg_atomic: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &experimentalPropertiesNode = *parser.findNodeWithKeyDfs("experimental_properties");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::readZeInfoExperimentalProperties(parser,
                                                     experimentalPropertiesNode,
                                                     experimentalProperties,
                                                     "some_kernel",
                                                     errors,
                                                     warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(experimentalProperties.hasNonKernelArgLoad, 0);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgStore, 1);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgAtomic, 0);
}

TEST(ReadZeExperimentalProperties, GivenYamlWithAtomicExperimentalPropertyEntryThenExperimentalPropertiesAreCorrectlyRead) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    experimental_properties:
      - has_non_kernel_arg_load: 0
        has_non_kernel_arg_store: 0
        has_non_kernel_arg_atomic: 1
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &experimentalPropertiesNode = *parser.findNodeWithKeyDfs("experimental_properties");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::readZeInfoExperimentalProperties(parser,
                                                     experimentalPropertiesNode,
                                                     experimentalProperties,
                                                     "some_kernel",
                                                     errors,
                                                     warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(experimentalProperties.hasNonKernelArgLoad, 0);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgStore, 0);
    EXPECT_EQ(experimentalProperties.hasNonKernelArgAtomic, 1);
}

TEST(ReadZeExperimentalProperties, GivenYamlWithInvalidExperimentalPropertyEntryValueThenErrorIsReturned) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    experimental_properties:
      - has_non_kernel_arg_load: true
        has_non_kernel_arg_store: 0
        has_non_kernel_arg_atomic: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &experimentalPropertiesNode = *parser.findNodeWithKeyDfs("experimental_properties");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::readZeInfoExperimentalProperties(parser,
                                                     experimentalPropertiesNode,
                                                     experimentalProperties,
                                                     "some_kernel",
                                                     errors,
                                                     warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
}

TEST(ReadZeExperimentalProperties, GivenYamlWithInvalidExperimentalPropertyEntryThenErrorIsReturned) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    experimental_properties:
      - has_non_kernel_arg_load: 1
        has_non_kernel_arg_store: 0
        has_non_kernel_arg_invalid: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &experimentalPropertiesNode = *parser.findNodeWithKeyDfs("experimental_properties");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::readZeInfoExperimentalProperties(parser,
                                                     experimentalPropertiesNode,
                                                     experimentalProperties,
                                                     "some_kernel",
                                                     errors,
                                                     warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
}

TEST(PopulateKernelDescriptor, GivenValidExperimentalPropertiesThenPopulateKernelDescriptorSucceeds) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        local_size
          offset:          16
          size:            12
      experimental_properties:
        - has_non_kernel_arg_load: 0
          has_non_kernel_arg_store: 0
          has_non_kernel_arg_atomic: 0
...
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
}

TEST(PopulateKernelDescriptor, GivenErrorWhileReadingExperimentalPropertiesThenPopulateKernelDescriptorFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        local_size
          offset:          16
          size:            12
      experimental_properties:
        - has_non_kernel_arg_load: true
          has_non_kernel_arg_store: 0
          has_non_kernel_arg_atomic: 0
...
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
}

TEST(ReadZeInfoExecutionEnvironment, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env: 
        actual_kernel_start_offset : 5
        barrier_count : 7
        disable_mid_thread_preemption : true
        grf_count : 13
        has_4gb_buffers : true
        has_dpas : true
        has_fence_for_image_access : true
        has_global_atomics : true
        has_multi_scratch_spaces : true
        has_no_stateless_write : true
        has_stack_calls : true
        require_disable_eufusion : true
        hw_preemption_mode : 2
        inline_data_payload_size : 32
        offset_to_skip_per_thread_data_load : 23
        offset_to_skip_set_ffid_gp : 29
        required_sub_group_size : 16
        simd_size : 32
        slm_size : 1024
        subgroup_independent_forward_progress : true
        required_work_group_size:
          - 8
          - 2
          - 1
        work_group_walk_order_dimensions:
          - 0
          - 1
          - 2 
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &execEnvNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv{};
    auto err = NEO::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(5, execEnv.actualKernelStartOffset);
    EXPECT_EQ(7, execEnv.barrierCount);
    EXPECT_TRUE(execEnv.disableMidThreadPreemption);
    EXPECT_EQ(13, execEnv.grfCount);
    EXPECT_TRUE(execEnv.has4GBBuffers);
    EXPECT_TRUE(execEnv.hasDpas);
    EXPECT_TRUE(execEnv.hasFenceForImageAccess);
    EXPECT_TRUE(execEnv.hasGlobalAtomics);
    EXPECT_TRUE(execEnv.hasMultiScratchSpaces);
    EXPECT_TRUE(execEnv.hasNoStatelessWrite);
    EXPECT_TRUE(execEnv.hasStackCalls);
    EXPECT_EQ(2, execEnv.hwPreemptionMode);
    EXPECT_EQ(32, execEnv.inlineDataPayloadSize);
    EXPECT_EQ(23, execEnv.offsetToSkipPerThreadDataLoad);
    EXPECT_EQ(29, execEnv.offsetToSkipSetFfidGp);
    EXPECT_EQ(16, execEnv.requiredSubGroupSize);
    EXPECT_EQ(32, execEnv.simdSize);
    EXPECT_EQ(1024, execEnv.slmSize);
    EXPECT_TRUE(execEnv.subgroupIndependentForwardProgress);
    EXPECT_EQ(8, execEnv.requiredWorkGroupSize[0]);
    EXPECT_EQ(2, execEnv.requiredWorkGroupSize[1]);
    EXPECT_EQ(1, execEnv.requiredWorkGroupSize[2]);
    EXPECT_EQ(0, execEnv.workgroupWalkOrderDimensions[0]);
    EXPECT_EQ(1, execEnv.workgroupWalkOrderDimensions[1]);
    EXPECT_EQ(2, execEnv.workgroupWalkOrderDimensions[2]);
}

TEST(ReadZeInfoExecutionEnvironment, GivenUnknownEntryThenEmmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env: 
        actual_kernel_start_offset : 17
        something_new : 36
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &execEnvNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"something_new\" in context of some_kernel\n", warnings.c_str());
    EXPECT_EQ(17, execEnv.actualKernelStartOffset);
}

TEST(ReadZeInfoExecutionEnvironment, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env: 
        actual_kernel_start_offset : true
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &execEnvNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read actual_kernel_start_offset from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoExecutionEnvironment, GivenInvalidLengthForKnownCollectionEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env: 
        required_work_group_size:
          - 5
          - 2
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &execEnvNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : wrong size of collection required_work_group_size in context of : some_kernel. Got : 2 expected : 3\n", errors.c_str());
}

TEST(ReadZeInfoDebugEnvironment, givenValidYamlEntryThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    debug_env:
        sip_surface_bti: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("debug_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(0, debugEnv.debugSurfaceBTI);
}

TEST(ReadZeInfoDebugEnvironment, givenUnknownEntryThenEmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    debug_env:
        sip_surface_bti: 0
        different: 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("debug_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"different\" in context of some_kernel\n", warnings.c_str());
}

TEST(ReadZeInfoDebugEnvironment, givenInvalidValueForKnownEntryThenFail) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    debug_env:
        sip_surface_bti: any
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("debug_env");
    std::string errors;
    std::string warnings;
    NEO::Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read sip_surface_bti from : [any] in context of : some_kernel\n", errors.c_str());
}

TEST(PopulateKernelDescriptor, givenValidDebugEnvironmentWithInvalidSipSurfaceBTIThenDoesNotGenerateSSH) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      debug_env:
        sip_surface_bti: 123
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    EXPECT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_TRUE(NEO::isUndefinedOffset(programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.tableOffset));
    ASSERT_EQ(0U, programInfo.kernelInfos[0]->heapInfo.SurfaceStateHeapSize);
    ASSERT_EQ(nullptr, programInfo.kernelInfos[0]->heapInfo.pSsh);
    EXPECT_TRUE(NEO::isUndefinedOffset(programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful));
}
TEST(PopulateKernelDescriptor, givenValidDebugEnvironmentWithSIPSurfaceBTISetThenGeneratesSshAndSetsSystemThreadSurfaceAddress) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      debug_env:
        sip_surface_bti: 0
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    EXPECT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(64U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.tableOffset);
    ASSERT_EQ(128U, programInfo.kernelInfos[0]->heapInfo.SurfaceStateHeapSize);
    ASSERT_NE(nullptr, programInfo.kernelInfos[0]->heapInfo.pSsh);
    EXPECT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful);
    EXPECT_EQ(0U, reinterpret_cast<const uint32_t *>(ptrOffset(programInfo.kernelInfos[0]->heapInfo.pSsh, 64U))[0]);
}

TEST(PopulateKernelDescriptor, givenErrorWhileReadingDebugEnvironmentThenPopulateKernelDescriptorFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      debug_env:
        sip_surface_bti: any
...
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
}

TEST(PopulateKernelDescriptor, GivenArgumentByValueWithMoreThanOneElementWithSourceOffsetsSpecifiedThenSetThemAccordingly) {
    NEO::ConstStringRef yaml = R"===(
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 8
    payload_arguments:
        - arg_type : arg_byvalue
          offset : 40
          size : 1
          arg_index	: 0
          source_offset : 0
        - arg_type : arg_byvalue
          offset : 44
          size : 4
          arg_index	: 0
          source_offset : 1
...
)===";
    namespace Defaults = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::Defaults;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = Defaults::argIndex;
    int32_t maxSmpIndex = Defaults::samplerIndex;
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::ZebinSections zebinSections;
    std::string errors, warnings;

    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(yaml, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    auto readPayloadArgsRes = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, readPayloadArgsRes);
    EXPECT_EQ(args[0].sourceOffset, 0);
    EXPECT_EQ(args[1].sourceOffset, 1);

    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto res = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, res);
    auto elements = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescValue>().elements;
    EXPECT_EQ(0, elements[0].sourceOffset);
    EXPECT_EQ(1, elements[1].sourceOffset);
}

TEST(PopulateKernelDescriptor, GiveArgumentByValueWithOneElementWithoutSourceOffsetSpecifiedSetItToZero) {
    NEO::ConstStringRef yaml = R"===(
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 8
    payload_arguments:
        - arg_type : arg_byvalue
          offset : 40
          size : 1
          arg_index	: 0
...
)===";
    namespace Defaults = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::Defaults;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = Defaults::argIndex;
    int32_t maxSmpIndex = Defaults::samplerIndex;
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::ZebinSections zebinSections;
    std::string errors, warnings;

    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(yaml, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    auto readPayloadArgsRes = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, readPayloadArgsRes);
    EXPECT_EQ(args[0].sourceOffset, Defaults::sourceOffset);

    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto res = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, res);
    auto elements = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescValue>().elements;
    EXPECT_EQ(0, elements[0].sourceOffset);
}

TEST(PopulateKernelDescriptor, GivenArgumentByValueWithoutAnySourceOffsetsSpecifiedThenPopulateKernelDescriptorReturnsError) {
    NEO::ConstStringRef yaml = R"===(
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 8
    payload_arguments:
        - arg_type : arg_byvalue
          offset : 40
          size : 1
          arg_index	: 0
        - arg_type : arg_byvalue
          offset : 44
          size : 4
          arg_index	: 0
...
)===";
    namespace Defaults = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::Defaults;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = Defaults::argIndex;
    int32_t maxSmpIndex = Defaults::samplerIndex;
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::ZebinSections zebinSections;
    std::string errors, warnings;

    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(yaml, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    auto readPayloadArgsRes = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, readPayloadArgsRes);
    EXPECT_EQ(args[0].sourceOffset, Defaults::sourceOffset);
    EXPECT_EQ(args[1].sourceOffset, Defaults::sourceOffset);

    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("Missing source offset value for element in argByValue\n", errors.c_str());
}

TEST(ReadEnumCheckedArgType, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::ArgType;
    NEO::Yaml::Token tokPackedLocalIds(packedLocalIds, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokLocalId(localId, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokLocalSize(localSize, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokGroupCount(groupCount, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokGlobalSize(globalSize, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokEnqueuedLocalSize(enqueuedLocalSize, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokGlobalIdOffset(globalIdOffset, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokPrivateBaseStateless(privateBaseStateless, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokArgByValue(argByvalue, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokArgByPointer(argBypointer, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokBufferOffset(bufferOffset, NEO::Yaml::Token::Token::LiteralString);

    using ArgType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgType;
    ArgType enumPackedLocalIds, enumLocalId, enumLocalSize, enumGroupCount, enumGlobalSize,
        enumEnqueuedLocalSize, enumGlobalIdOffset, enumPrivateBaseStateless, enumArgByValue, enumArgByPointer, enumBufferOffset;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokPackedLocalIds, enumPackedLocalIds, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypePackedLocalIds, enumPackedLocalIds);

    success = NEO::readEnumChecked(&tokLocalId, enumLocalId, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeLocalId, enumLocalId);

    success = NEO::readEnumChecked(&tokLocalSize, enumLocalSize, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeLocalSize, enumLocalSize);

    success = NEO::readEnumChecked(&tokGroupCount, enumGroupCount, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeGroupCount, enumGroupCount);

    success = NEO::readEnumChecked(&tokGlobalSize, enumGlobalSize, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeGlobalSize, enumGlobalSize);

    success = NEO::readEnumChecked(&tokEnqueuedLocalSize, enumEnqueuedLocalSize, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeEnqueuedLocalSize, enumEnqueuedLocalSize);

    success = NEO::readEnumChecked(&tokGlobalIdOffset, enumGlobalIdOffset, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeGlobalIdOffset, enumGlobalIdOffset);

    success = NEO::readEnumChecked(&tokPrivateBaseStateless, enumPrivateBaseStateless, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypePrivateBaseStateless, enumPrivateBaseStateless);

    success = NEO::readEnumChecked(&tokArgByValue, enumArgByValue, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeArgByvalue, enumArgByValue);

    success = NEO::readEnumChecked(&tokArgByPointer, enumArgByPointer, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeArgBypointer, enumArgByPointer);

    success = NEO::readEnumChecked(&tokBufferOffset, enumBufferOffset, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(ArgType::ArgTypeBufferOffset, enumBufferOffset);
}

TEST(ReadEnumCheckedArgType, GivenNullTokenThenFail) {
    using ArgType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgType;
    ArgType enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedArgType, GivenUnknownStringRepresentationThenFail) {
    using ArgType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgType;
    ArgType enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" argument type in context of some_kernel\n", errors.c_str());
}

TEST(ReadEnumCheckedMemoryAddressingMode, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::MemoryAddressingMode;

    NEO::Yaml::Token tokStateless(stateless, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokStateful(stateful, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokBindless(bindless, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokSharedLocalMemory(sharedLocalMemory, NEO::Yaml::Token::Token::LiteralString);

    using MemoryAddressingMode = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingMode;
    MemoryAddressingMode enumStateless, enumStateful, enumBindless, enumSharedLocalMemory;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokStateless, enumStateless, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryAddressingMode::MemoryAddressingModeStateless, enumStateless);

    success = NEO::readEnumChecked(&tokStateful, enumStateful, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryAddressingMode::MemoryAddressingModeStateful, enumStateful);

    success = NEO::readEnumChecked(&tokBindless, enumBindless, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryAddressingMode::MemoryAddressingModeBindless, enumBindless);

    success = NEO::readEnumChecked(&tokSharedLocalMemory, enumSharedLocalMemory, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryAddressingMode::MemoryAddressingModeSharedLocalMemory, enumSharedLocalMemory);
}

TEST(ReadEnumCheckedMemoryAddressingMode, GivenNullTokenThenFail) {
    using MemoryAddressingMode = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingMode;
    MemoryAddressingMode enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedMemoryAddressingMode, GivenUnknownStringRepresentationThenFail) {
    using MemoryAddressingMode = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingMode;
    MemoryAddressingMode enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" memory addressing mode in context of some_kernel\n", errors.c_str());
}

TEST(ReadEnumCheckedAddressSpace, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AddrSpace;

    NEO::Yaml::Token tokGlobal(global, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokLocal(local, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokConstant(constant, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokImage(image, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokSampler(sampler, NEO::Yaml::Token::Token::LiteralString);

    using AddressSpace = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpace;
    AddressSpace enumGlobal, enumLocal, enumConstant, enumImage, enumSampler;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokGlobal, enumGlobal, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AddressSpace::AddressSpaceGlobal, enumGlobal);

    success = NEO::readEnumChecked(&tokLocal, enumLocal, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AddressSpace::AddressSpaceLocal, enumLocal);

    success = NEO::readEnumChecked(&tokConstant, enumConstant, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AddressSpace::AddressSpaceConstant, enumConstant);

    success = NEO::readEnumChecked(&tokImage, enumImage, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AddressSpace::AddressSpaceImage, enumImage);

    success = NEO::readEnumChecked(&tokSampler, enumSampler, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AddressSpace::AddressSpaceSampler, enumSampler);
}

TEST(ReadEnumCheckedAddressSpace, GivenNullTokenThenFail) {
    using AddressSpace = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpace;
    AddressSpace enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedAddressSpace, GivenUnknownStringRepresentationThenFail) {
    using AddressSpace = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpace;
    AddressSpace enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" address space in context of some_kernel\n", errors.c_str());
}

TEST(ReadEnumCheckedAccessType, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AccessType;

    NEO::Yaml::Token tokReadOnly(readonly, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokWriteOnly(writeonly, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokReadWrite(readwrite, NEO::Yaml::Token::Token::LiteralString);

    using AccessType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessType;
    AccessType enumReadOnly, enumWriteOnly, enumReadWrite;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokReadOnly, enumReadOnly, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AccessType::AccessTypeReadonly, enumReadOnly);

    success = NEO::readEnumChecked(&tokWriteOnly, enumWriteOnly, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AccessType::AccessTypeWriteonly, enumWriteOnly);

    success = NEO::readEnumChecked(&tokReadWrite, enumReadWrite, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AccessType::AccessTypeReadwrite, enumReadWrite);
}

TEST(ReadEnumCheckedAccessType, GivenNullTokenThenFail) {
    using AccessType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessType;
    AccessType enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedAccessType, GivenUnknownStringRepresentationThenFail) {
    using AccessType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessType;
    AccessType enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" access type in context of some_kernel\n", errors.c_str());
}

TEST(ReadEnumCheckedAllocationType, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::AllocationType;

    NEO::Yaml::Token tokGlobal(global, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokScratch(scratch, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokSlm(slm, NEO::Yaml::Token::Token::LiteralString);

    using AllocationType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationType;
    AllocationType enumGlobal, enumScratch, enumSlm;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokGlobal, enumGlobal, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AllocationType::AllocationTypeGlobal, enumGlobal);

    success = NEO::readEnumChecked(&tokScratch, enumScratch, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AllocationType::AllocationTypeScratch, enumScratch);

    success = NEO::readEnumChecked(&tokSlm, enumSlm, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(AllocationType::AllocationTypeSlm, enumSlm);
}

TEST(ReadEnumCheckedAllocationType, GivenNullTokenThenFail) {
    using AllocationType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationType;
    AllocationType enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedAllocationType, GivenUnknownStringRepresentationThenFail) {
    using AllocationType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationType;
    AllocationType enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" per-thread memory buffer allocation type in context of some_kernel\n", errors.c_str());
}

TEST(ReadEnumCheckedMemoryUsage, GivenValidStringRepresentationThenParseItCorrectly) {
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;

    NEO::Yaml::Token tokPrivateSpace(privateSpace, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokSpillFillSpace(spillFillSpace, NEO::Yaml::Token::Token::LiteralString);
    NEO::Yaml::Token tokSingleSpace(singleSpace, NEO::Yaml::Token::Token::LiteralString);

    using MemoryUsage = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
    MemoryUsage enumPrivateSpace, enumSpillFillSpace, enumSingleSpace;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(&tokPrivateSpace, enumPrivateSpace, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryUsage::MemoryUsagePrivateSpace, enumPrivateSpace);

    success = NEO::readEnumChecked(&tokSpillFillSpace, enumSpillFillSpace, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryUsage::MemoryUsageSpillFillSpace, enumSpillFillSpace);

    success = NEO::readEnumChecked(&tokSingleSpace, enumSingleSpace, "some_kernel", errors);
    EXPECT_TRUE(success);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(MemoryUsage::MemoryUsageSingleSpace, enumSingleSpace);
}

TEST(ReadEnumCheckedMemoryUsage, GivenNullTokenThenFail) {
    using MemoryUsage = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
    MemoryUsage enumRepresentation;
    std::string errors;
    bool success;

    success = NEO::readEnumChecked(nullptr, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumCheckedMemoryUsage, GivenUnknownStringRepresentationThenFail) {
    using MemoryUsage = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsage;
    MemoryUsage enumRepresentation;
    std::string errors;
    bool success;

    NEO::Yaml::Token someEntry("some_entry", NEO::Yaml::Token::Token::LiteralString);
    success = NEO::readEnumChecked(&someEntry, enumRepresentation, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled \"some_entry\" per-thread memory buffer usage type in context of some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoPerThreadPayloadArguments, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_payload_arguments: 
        - arg_type : packed_local_ids
          offset : 8
          size : 16
        - arg_type : local_id
          offset : 32
          size : 192
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("per_thread_payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadPayloadArguments args;
    auto err = NEO::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, args.size());
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypePackedLocalIds, args[0].argType);
    EXPECT_EQ(8, args[0].offset);
    EXPECT_EQ(16, args[0].size);

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeLocalId, args[1].argType);
    EXPECT_EQ(32, args[1].offset);
    EXPECT_EQ(192, args[1].size);
}

TEST(ReadZeInfoPerThreadPayloadArguments, GivenUnknownEntryThenEmmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_payload_arguments: 
        - arg_type : packed_local_ids
          offset : 8
          size : 16
          something_new : 256
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("per_thread_payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadPayloadArguments args;
    auto err = NEO::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"something_new\" for per-thread payload argument in context of some_kernel\n", warnings.c_str());

    ASSERT_EQ(1U, args.size());
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypePackedLocalIds, args[0].argType);
    EXPECT_EQ(8, args[0].offset);
    EXPECT_EQ(16, args[0].size);
}

TEST(ReadZeInfoPerThreadPayloadArguments, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_payload_arguments: 
        - arg_type : packed_local_ids
          offset : true
          size : 16
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("per_thread_payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadPayloadArguments args;
    auto err = NEO::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read offset from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoPerThreadPayloadArguments, GivenZeroSizeEntryThenSkipsItAndEmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_payload_arguments: 
        - arg_type : packed_local_ids
          offset : 16
          size : 0
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("per_thread_payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadPayloadArguments args;
    auto err = NEO::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Skippinig 0-size per-thread argument of type : packed_local_ids in context of some_kernel\n", warnings.c_str());
    EXPECT_TRUE(args.empty());
}

TEST(ReadZeInfoPayloadArguments, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    payload_arguments: 
        - arg_type : arg_bypointer
          offset : 16
          size : 8	
          arg_index	: 1
          addrmode : stateless
          addrspace	: global
          access_type : readwrite
        - arg_type : arg_byvalue
          offset : 24
          size : 4
          arg_index	: 2
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = -1;
    int32_t maxSmpIndex = -1;
    auto err = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(2, maxArgIndex);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, args.size());

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgBypointer, args[0].argType);
    EXPECT_EQ(16, args[0].offset);
    EXPECT_EQ(8, args[0].size);
    EXPECT_EQ(1, args[0].argIndex);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingModeStateless, args[0].addrmode);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceGlobal, args[0].addrspace);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessTypeReadwrite, args[0].accessType);

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgByvalue, args[1].argType);
    EXPECT_EQ(24, args[1].offset);
    EXPECT_EQ(4, args[1].size);
}

TEST(ReadZeInfoPayloadArguments, GivenUnknownEntryThenEmmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    payload_arguments: 
        - arg_type : arg_byvalue
          offset : 24
          size : 4
          arg_index	: 2
          something_new : 7
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = -1;
    int32_t maxSmpIndex = -1;
    auto err = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"something_new\" for payload argument in context of some_kernel\n", warnings.c_str());

    ASSERT_EQ(1U, args.size());
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgByvalue, args[0].argType);
    EXPECT_EQ(24, args[0].offset);
    EXPECT_EQ(4, args[0].size);
    EXPECT_EQ(2, args[0].argIndex);
}

TEST(ReadZeInfoPayloadArguments, GivenArgByPointerWithSlmAddresingModeAndSlmAlignmentThenSetSlmAlignmentAccordingly) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    payload_arguments:
        - arg_type : arg_bypointer
          offset : 24
          size : 4
          arg_index	: 2
          addrmode	: slm
        - arg_type : arg_bypointer
          offset : 16
          size : 8
          arg_index	: 1
          addrmode	: slm
          slm_alignment: 8
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = -1;
    int32_t maxSmpIndex = -1;
    auto err = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(2U, args.size());
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgBypointer, args[0].argType);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgBypointer, args[1].argType);
    EXPECT_EQ(16, args[0].slmArgAlignment);
    EXPECT_EQ(8, args[1].slmArgAlignment);
}

TEST(ReadZeInfoBindingTableIndices, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    payload_arguments: 
        - arg_type : arg_byvalue
          offset : 24
          size : true
          arg_index	: 2
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("payload_arguments");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPayloadArguments args;
    int32_t maxArgIndex = -1;
    int32_t maxSmpIndex = -1;
    auto err = NEO::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, maxSmpIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read size from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoBindingTableIndices, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    binding_table_indices: 
        - bti_value : 1
          arg_index : 7
        - bti_value : 5
          arg_index : 13
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &btisNode = *parser.findNodeWithKeyDfs("binding_table_indices");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoBindingTableIndices btis;
    NEO::ZeInfoBindingTableIndices::value_type maxBindingTableEntry = {};
    auto err = NEO::readZeInfoBindingTableIndices(parser, btisNode, btis, maxBindingTableEntry, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, btis.size());
    EXPECT_EQ(5, maxBindingTableEntry.btiValue);
    EXPECT_EQ(13, maxBindingTableEntry.argIndex);

    EXPECT_EQ(1, btis[0].btiValue);
    EXPECT_EQ(7, btis[0].argIndex);

    EXPECT_EQ(5, btis[1].btiValue);
    EXPECT_EQ(13, btis[1].argIndex);
}

TEST(ReadZeInfoBindingTableIndices, GivenUnknownEntryThenEmmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    binding_table_indices: 
        - bti_value : 1
          arg_index : 7
          something_new : true
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("binding_table_indices");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoBindingTableIndices btis;
    NEO::ZeInfoBindingTableIndices::value_type maxBindingTableEntry = {};
    auto err = NEO::readZeInfoBindingTableIndices(parser, argsNode, btis, maxBindingTableEntry, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"something_new\" for binding table index in context of some_kernel\n", warnings.c_str());

    ASSERT_EQ(1U, btis.size());
    EXPECT_EQ(1, btis[0].btiValue);
    EXPECT_EQ(7, btis[0].argIndex);
}

TEST(ReadZeInfoPayloadArguments, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    binding_table_indices: 
        - bti_value : 1
          arg_index : any
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("binding_table_indices");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoBindingTableIndices btis;
    NEO::ZeInfoBindingTableIndices::value_type maxBindingTableEntry = {};
    auto err = NEO::readZeInfoBindingTableIndices(parser, argsNode, btis, maxBindingTableEntry, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read arg_index from : [any] in context of : some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoPerThreadMemoryBuffers, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_memory_buffers: 
      - type:            scratch
        usage:           single_space
        size:            64
      - type:            global
        usage:           private_space
        size:            128
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &buffersNode = *parser.findNodeWithKeyDfs("per_thread_memory_buffers");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadMemoryBuffers buffers;
    auto err = NEO::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, buffers.size());

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeScratch, buffers[0].allocationType);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsageSingleSpace, buffers[0].memoryUsage);
    EXPECT_FALSE(buffers[0].isSimtThread);
    EXPECT_EQ(64, buffers[0].size);

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeGlobal, buffers[1].allocationType);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsagePrivateSpace, buffers[1].memoryUsage);
    EXPECT_EQ(128, buffers[1].size);
    EXPECT_FALSE(buffers[1].isSimtThread);
}

TEST(ReadZeInfoPerThreadMemoryBuffers, GivenPerSimtThreadPrivateMemoryThenSetsProperFlag) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    execution_env:   
      simd_size: 16
    per_thread_memory_buffers: 
      - type:            global
        usage:           private_space
        size:            128
        is_simt_thread:  True
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &buffersNode = *parser.findNodeWithKeyDfs("per_thread_memory_buffers");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadMemoryBuffers buffers;
    auto err = NEO::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, buffers.size());

    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeGlobal, buffers[0].allocationType);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsagePrivateSpace, buffers[0].memoryUsage);
    EXPECT_EQ(128, buffers[0].size);
    EXPECT_TRUE(buffers[0].isSimtThread);
}

TEST(ReadZeInfoPerThreadMemoryBuffers, GivenUnknownEntryThenEmmitsWarning) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_memory_buffers: 
      - type:            scratch
        usage:           single_space
        size:            64
        something_new : 256
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &buffersNode = *parser.findNodeWithKeyDfs("per_thread_memory_buffers");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadMemoryBuffers buffers;
    auto err = NEO::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"something_new\" for per-thread memory buffer in context of some_kernel\n", warnings.c_str());

    ASSERT_EQ(1U, buffers.size());
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeScratch, buffers[0].allocationType);
    EXPECT_EQ(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::MemoryUsageSingleSpace, buffers[0].memoryUsage);
    EXPECT_EQ(64, buffers[0].size);
}

TEST(ReadZeInfoPerThreadMemoryBuffers, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:         
  - name:            some_kernel
    per_thread_memory_buffers: 
      - type:            scratch
        usage:           single_space
        size:            eight
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &buffersNode = *parser.findNodeWithKeyDfs("per_thread_memory_buffers");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoPerThreadMemoryBuffers args;
    auto err = NEO::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read size from : [eight] in context of : some_kernel\n", errors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenInvalidElfThenReturnError) {
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(decodeWarnings.empty());
    EXPECT_FALSE(decodeErrors.empty());
    EXPECT_STREQ("Invalid or missing ELF header", decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, WhenFailedToExtractZebinSectionsThenDecodingFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->shStrNdx = NEO::Elf::SHN_UNDEF;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_FALSE(decodeErrors.empty());

    std::string elfErrors;
    std::string elfWarnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, elfErrors, elfWarnings);
    ASSERT_TRUE((nullptr != elf.elfFileHeader) && elfErrors.empty() && elfWarnings.empty());
    NEO::ZebinSections sections;
    std::string extractErrors;
    std::string extractWarnings;
    auto extractErr = NEO::extractZebinSections(elf, sections, extractErrors, extractWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, extractErr);
    EXPECT_STREQ(extractErrors.c_str(), decodeErrors.c_str());
    EXPECT_STREQ(extractWarnings.c_str(), decodeWarnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, WhenValidationOfZebinSectionsCountFailsThenDecodingFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, {});
    zebin.appendSection(NEO::Elf::SHT_ZEBIN_SPIRV, NEO::Elf::SectionsNamesZebin::spv, {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_FALSE(decodeErrors.empty());

    std::string elfErrors;
    std::string elfWarnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, elfErrors, elfWarnings);
    ASSERT_TRUE((nullptr != elf.elfFileHeader) && elfErrors.empty() && elfWarnings.empty());
    NEO::ZebinSections sections;
    std::string extractErrors;
    std::string extractWarnings;
    auto extractErr = NEO::extractZebinSections(elf, sections, extractErrors, extractWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, extractErr);
    EXPECT_TRUE(extractErrors.empty()) << extractErrors;
    EXPECT_TRUE(extractWarnings.empty()) << extractWarnings;

    std::string validateErrors;
    std::string validateWarnings;
    auto validateErr = NEO::validateZebinSectionsCount(sections, validateErrors, validateWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, validateErr);
    EXPECT_STREQ(validateErrors.c_str(), decodeErrors.c_str());
    EXPECT_STREQ(validateWarnings.c_str(), decodeWarnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenGlobalDataSectionThenSetsUpInitDataAndSize) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataGlobal, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_EQ(sizeof(data), programInfo.globalVariables.size);
    EXPECT_EQ(0, memcmp(programInfo.globalVariables.initData, data, sizeof(data)));
    EXPECT_EQ(0U, programInfo.globalConstants.size);
    EXPECT_EQ(nullptr, programInfo.globalConstants.initData);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenConstDataSectionThenSetsUpInitDataAndSize) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConst, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_EQ(sizeof(data), programInfo.globalConstants.size);
    EXPECT_EQ(0, memcmp(programInfo.globalConstants.initData, data, sizeof(data)));
    EXPECT_EQ(0U, programInfo.globalVariables.size);
    EXPECT_EQ(nullptr, programInfo.globalVariables.initData);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenConstDataStringsSectionThenSetsUpInitDataAndSize) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', '!'};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::dataConstString, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_EQ(sizeof(data), programInfo.globalStrings.size);
    EXPECT_NE(nullptr, programInfo.globalStrings.initData);
    EXPECT_EQ(sizeof(data), programInfo.globalStrings.size);
    EXPECT_EQ(0, memcmp(programInfo.globalStrings.initData, data, sizeof(data)));
}

TEST(DecodeSingleDeviceBinaryZebin, GivenIntelGTNoteSectionThenAddsItToZebinSections) {
    ZebinTestData::ValidEmptyProgram zebin;
    NEO::Elf::IntelGTNote note = {};
    note.nameSize = 8;
    note.descSize = 4;
    note.type = NEO::Elf::IntelGTSectionType::GfxCore;
    strcpy_s(const_cast<char *>(note.ownerName), note.nameSize, NEO::Elf::IntelGtNoteOwnerName.str().c_str());
    note.desc = GFXCORE_FAMILY::IGFX_GEN9_CORE;
    zebin.appendSection(NEO::Elf::SHT_NOTE, NEO::Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(&note, 1));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;
    EXPECT_EQ(1u, zebinSections.noteIntelGTSections.size());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenNoteSectionDifferentThanIntelGTThenEmitsWarningAndSkipsIt) {
    ZebinTestData::ValidEmptyProgram zebin;
    struct NoteSection : NEO::Elf::ElfNoteSection {
        const char ownerName[4] = "xxx";
        uint32_t desc = 0x12341234;
    } note;
    note.nameSize = 4;
    note.descSize = 4;
    note.type = 0;
    ConstStringRef sectionName = ".note.example";
    zebin.appendSection(NEO::Elf::SHT_NOTE, sectionName, ArrayRef<uint8_t>::fromAny(&note, 1));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr);
    EXPECT_EQ(0u, zebinSections.noteIntelGTSections.size());
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(warnings.empty());
    auto expectedWarning = "DeviceBinaryFormat::Zebin : Unhandled SHT_NOTE section : " + sectionName.str() + " currently supports only : " + NEO::Elf::SectionsNamesZebin::noteIntelGT.str() + ".\n";
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenSymtabSectionThenEmitsWarningAndSkipsIt) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab, data).entsize = sizeof(NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64>);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Ignoring symbol table\n", decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenSymtabWithInvalidSymEntriesThenFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab, data).entsize = sizeof(NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64>) - 1;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_STREQ("Invalid symbol table entries size - expected : 24, got : 23\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoSectionIsEmptyThenEmitsWarning) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected at least one .ze_info section, got 0\n", decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenYamlParserForZeInfoFailsThenDecodingFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = NEO::ConstStringRef("unterminated_string : \"");
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;

    NEO::Yaml::YamlParser parser;
    std::string parserErrors;
    std::string parserWarnings;
    bool validYaml = parser.parse((brokenZeInfo), parserErrors, parserWarnings);
    EXPECT_FALSE(validYaml);
    EXPECT_STREQ(parserWarnings.c_str(), decodeWarnings.c_str());
    EXPECT_STREQ(parserErrors.c_str(), decodeErrors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenEmptyInZeInfoThenEmitsWarning) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = NEO::ConstStringRef("#no data\n");
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ("NEO::Yaml : Text has no data\nDeviceBinaryFormat::Zebin : Empty kernels metadata section (.ze_info)\n", decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenUnknownEntryInZeInfoGlobalScopeThenEmitsWarning) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = std::string("some_entry : a\nkernels : \nversion:\'") + toString(zeInfoDecoderVersion) + "\'\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"some_entry\" in global scope of .ze_info\n", decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoDoesNotContainKernelsSectionThenEmitsWarning) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + toString(zeInfoDecoderVersion) + "\'\na:b\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"a\" in global scope of .ze_info\nDeviceBinaryFormat::Zebin::.ze_info : Expected one kernels entry in global scope of .ze_info, got : 0\n", decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoContainsMultipleKernelSectionsThenFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + toString(zeInfoDecoderVersion) + "\'\nkernels:\nkernels:\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Expected at most one kernels entry in global scope of .ze_info, got : 2\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoContainsMultipleVersionSectionsThenFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + toString(zeInfoDecoderVersion) + "\'\nversion:\'5.4\'\nkernels:\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Expected at most one version entry in global scope of .ze_info, got : 2\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoDoesNotContainVersionSectionsThenEmitsWarnings) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto zeInfo = ConstStringRef("kernels:\n");
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    auto version = NEO::zeInfoDecoderVersion;
    auto expectedWarning = "DeviceBinaryFormat::Zebin::.ze_info : No version info provided (i.e. no version entry in global scope of DeviceBinaryFormat::Zebin::.ze_info) - will use decoder's default : '" +
                           std::to_string(version.major) + "." + std::to_string(version.minor) + "'\n";

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ(expectedWarning.c_str(), decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoVersionIsInvalidThenFails) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto zeInfo = ConstStringRef("version:\'1a\'\nkernels:\n");
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : '1a'\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoMinorVersionIsNewerThenEmitsWarning) {
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    auto version = NEO::zeInfoDecoderVersion;
    std::string expectedWarning = "DeviceBinaryFormat::Zebin::.ze_info : Minor version : " + std::to_string(version.minor + 1) + " is newer than available in decoder : " + std::to_string(version.minor) + " - some features may be skipped\n";
    version.minor += 1;
    auto zeInfo = std::string("version:\'") + toString(version) + "\'\nkernels:\n";
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_STREQ(expectedWarning.c_str(), decodeWarnings.c_str());
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoMajorVersionIsMismatchedThenFails) {
    {
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
        auto version = NEO::zeInfoDecoderVersion;
        version.major += 1;
        auto zeInfo = std::string("version:\'") + toString(version) + "\'\nkernels:\n";
        zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string decodeErrors;
        std::string decodeWarnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
        EXPECT_EQ(NEO::DecodeError::UnhandledBinary, error);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled major version : 2, decoder is at : 1\n", decodeErrors.c_str());
        EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    }

    {
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
        auto version = NEO::zeInfoDecoderVersion;
        version.major -= 1;
        auto zeInfo = std::string("version:\'") + toString(version) + "\'\nkernels:\n";
        zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string decodeErrors;
        std::string decodeWarnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
        EXPECT_EQ(NEO::DecodeError::UnhandledBinary, error);
        EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Unhandled major version : 0, decoder is at : 1\n", decodeErrors.c_str());
        EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    }
}

TEST(DecodeSingleDeviceBinaryZebin, WhenDecodeZeInfoFailsThenDecodingFails) {
    std::string brokenZeInfo = "version : \'" + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - 
)===";
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);

    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of name section, got : 0\nDeviceBinaryFormat::Zebin : Expected exactly 1 of execution_env section, got : 0\n", decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoThenPopulatesKernelDescriptorProperly) {
    std::string validZeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
    - name : some_other_kernel
      execution_env :
        simd_size : 32
)===";
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_other_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;

    ASSERT_EQ(2U, programInfo.kernelInfos.size());
    EXPECT_STREQ("some_kernel", programInfo.kernelInfos[0]->kernelDescriptor.kernelMetadata.kernelName.c_str());
    EXPECT_STREQ("some_other_kernel", programInfo.kernelInfos[1]->kernelDescriptor.kernelMetadata.kernelName.c_str());
    EXPECT_EQ(8, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(32, programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(DeviceBinaryFormat::Zebin, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.binaryFormat);
    EXPECT_EQ(DeviceBinaryFormat::Zebin, programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.binaryFormat);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoAndExternalFunctionsMetadataThenPopulatesExternalFunctionMetadataProperly) {
    std::string validZeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
functions:
    - name: fun1
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 1
)===";

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;

    ASSERT_EQ(1U, programInfo.externalFunctions.size());
    auto &funInfo = programInfo.externalFunctions[0];
    EXPECT_STREQ("fun1", funInfo.functionName.c_str());
    EXPECT_EQ(128U, funInfo.numGrfRequired);
    EXPECT_EQ(8U, funInfo.simdSize);
    EXPECT_EQ(1U, funInfo.barrierCount);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoAndInvalidExternalFunctionsMetadataThenFail) {
    std::string validZeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
functions:
    - name: fun
      execution_env:
        grf_count: abc
        simd_size: defgas
)===";

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    const std::string expectedError = "DeviceBinaryFormat::Zebin::.ze_info : could not read grf_count from : [abc] in context of : external functions\nDeviceBinaryFormat::Zebin::.ze_info : could not read simd_size from : [defgas] in context of : external functions\n";
    EXPECT_STREQ(expectedError.c_str(), decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenZeInfoWithTwoExternalFunctionsEntriesThenFail) {
    std::string validZeInfo = std::string("version :\'") + toString(zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
functions:
    - name: fun
      execution_env:
        grf_count: 128
        simd_size: 8
functions:
    - name: fun
      execution_env:
        grf_count: 128
        simd_size: 8 
)===";

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    const std::string expectedError = "DeviceBinaryFormat::Zebin::.ze_info : Expected at most one functions entry in global scope of .ze_info, got : 2\n";
    EXPECT_STREQ(expectedError.c_str(), decodeErrors.c_str());
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
}

TEST(PopulateKernelDescriptor, GivenMinimalExecutionEnvThenPopulateKernelDescriptorWithDefaults) {
    std::string zeinfo = R"===(
    kernels:
        - name : some_kernel
          execution_env:   
            simd_size: 32
            grf_count: 128
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    namespace Defaults = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::Defaults;
    const auto &kernelDescriptor = programInfo.kernelInfos[0]->kernelDescriptor;
    EXPECT_EQ(kernelDescriptor.entryPoints.skipPerThreadDataLoad, static_cast<NEO::InstructionsSegmentOffset>(Defaults::offsetToSkipPerThreadDataLoad));
    EXPECT_EQ(kernelDescriptor.entryPoints.skipSetFFIDGP, static_cast<NEO::InstructionsSegmentOffset>(Defaults::offsetToSkipSetFfidGp));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.passInlineData, (Defaults::inlineDataPayloadSize != 0));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption, Defaults::disableMidThreadPreemption);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress, Defaults::subgroupIndependentForwardProgress);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, Defaults::hasGlobalAtomics);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.useStackCalls, Defaults::hasStackCalls);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages, Defaults::hasFenceForImageAccess);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode, Defaults::hasDpas);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesStatelessWrites, (false == Defaults::hasNoStatelessWrite));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.barrierCount, static_cast<uint8_t>(Defaults::barrierCount));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.binaryFormat, DeviceBinaryFormat::Zebin);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.bufferAddressingMode, (Defaults::has4GBBuffers) ? KernelDescriptor::Stateless : KernelDescriptor::BindfulAndStateless);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.inlineDataPayloadSize, static_cast<uint16_t>(Defaults::inlineDataPayloadSize));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[0]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[1]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[2]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.slmInlineSize, static_cast<uint32_t>(Defaults::slmSize));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[0], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[0]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[1], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[1]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[2], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[2]));
    EXPECT_EQ(kernelDescriptor.kernelMetadata.requiredSubGroupSize, static_cast<uint8_t>(Defaults::requiredSubGroupSize));
}

TEST(PopulateKernelDescriptor, WhenValidationOfZeinfoSectionsCountFailsThenDecodingFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Expected exactly 1 of name section, got : 0\nDeviceBinaryFormat::Zebin : Expected exactly 1 of execution_env section, got : 0\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenInvalidExecutionEnvironmentThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env :
        simd_size : true
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read simd_size from : [true] in context of : some_kernel\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenInvalidPerThreadPayloadArgYamlEntriesThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        actual_kernel_start_offset: 0
      per_thread_payload_arguments: 
        - arg_type:        local_id
          offset:          aaa
          size:            8
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read offset from : [aaa] in context of : some_kernel\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenInvalidPayloadArgYamlEntriesThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        actual_kernel_start_offset: 0
      payload_arguments: 
        - arg_type:        global_id_offset
          offset:          aaa
          size:            12
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read offset from : [aaa] in context of : some_kernel\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenInvalidPerThreadMemoryBufferYamlEntriesThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        actual_kernel_start_offset: 0
      per_thread_memory_buffers: 
        - type:        scratch
          usage:       spill_fill_space
          size:        eight
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read size from : [eight] in context of : some_kernel\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenInvalidSimdSizeThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 7
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid simd size : 7 in context of : some_kernel. Expected 1, 8, 16 or 32. Got : 7\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenValidSimdSizeThenSetsItCorrectly) {
    uint32_t validSimdSizes[] = {1, 8, 16, 32};
    for (auto simdSize : validSimdSizes) {
        std::string zeinfo = R"===(
    kernels:
        - name : some_kernel
          execution_env:   
            simd_size: )===" +
                             std::to_string(simdSize) + "\n";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        EXPECT_EQ(simdSize, (*programInfo.kernelInfos.begin())->kernelDescriptor.kernelAttributes.simdSize);
    }
}

TEST(PopulateKernelDescriptor, GivenInvalidPerThreadArgThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_payload_arguments:
        - arg_type:        local_size
          offset:          0
          size:            8
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid arg type in per-thread data section in context of : some_kernel.\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenValidLocalIdThenAlignUpChannelSizeToGrfSize) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 16
      per_thread_payload_arguments: 
        - arg_type:        local_id
          offset:          0
          size:            192
)===";
    NEO::ProgramInfo programInfo;
    programInfo.grfSize = 64;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(3U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.numLocalIdChannels);
    EXPECT_EQ(192U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perThreadDataSize);
}

TEST(PopulateKernelDescriptor, GivenValidPerThreadArgThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 32
      per_thread_payload_arguments: 
        - arg_type:        local_id
          offset:          0
          size:            192
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(3U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.numLocalIdChannels);
}

TEST(PopulateKernelDescriptor, GivenInvalidPayloadArgThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        local_id
          offset:          0
          size:            12
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid arg type in cross thread data section in context of : some_kernel.\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenZebinAppendElwsThenInjectsElwsArg) {
    DebugManagerStateRestore dbgRestore;
    NEO::DebugManager.flags.ZebinAppendElws.set(true);
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        local_size
          offset:          16
          size:            12
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(64, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(16, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[0]);
    EXPECT_EQ(20, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[1]);
    EXPECT_EQ(24, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[2]);
    EXPECT_EQ(32, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0]);
    EXPECT_EQ(36, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1]);
    EXPECT_EQ(40, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2]);
}

TEST(PopulateKernelDescriptor, GivenInvalidBindingTableYamlEntriesThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        arg_byvalue
          offset:          0
          size:            12
          arg_index:       0
      binding_table_indices:
        - arg_index: 0
          bti_value:true
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : could not read bti_value from : [true] in context of : some_kernel\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenValidBindingTableEntriesThenGeneratesSsh) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        arg_bypointer
          offset:          0
          size:            8
          arg_index:       0
          addrmode : stateful
        - arg_type:        arg_bypointer
          offset:          8
          size:            8
          arg_index:       1
          addrmode : stateful
          addrspace : image
      binding_table_indices:
        - arg_index: 0
          bti_value:2
        - arg_index: 1
          bti_value:7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    ASSERT_EQ(2U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    EXPECT_EQ(128, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>().bindful);
    EXPECT_EQ(448, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[1].as<ArgDescImage>().bindful);
    EXPECT_EQ(8U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(512U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.bindingTable.tableOffset);
    ASSERT_EQ(576U, programInfo.kernelInfos[0]->heapInfo.SurfaceStateHeapSize);
    ASSERT_NE(nullptr, programInfo.kernelInfos[0]->heapInfo.pSsh);
    EXPECT_EQ(128U, reinterpret_cast<const uint32_t *>(ptrOffset(programInfo.kernelInfos[0]->heapInfo.pSsh, 512U))[2]);
    EXPECT_EQ(448U, reinterpret_cast<const uint32_t *>(ptrOffset(programInfo.kernelInfos[0]->heapInfo.pSsh, 512U))[7]);
}

TEST(PopulateKernelDescriptor, GivenBtiEntryForWrongArgTypeThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        arg_byvalue
          offset:          0
          size:            12
          arg_index:       0
      binding_table_indices:
        - arg_index: 0
          bti_value:0
)===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin::.ze_info : Invalid binding table entry for non-pointer and non-image argument idx : 0.\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenValidArgSamplerThenGeneratesDsh) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type:        arg_bypointer
          size:            8
          addrspace:       sampler
          addrmode:        stateful
          arg_index:       0
          sampler_index:   0
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    const auto &kernelDescriptor = programInfo.kernelInfos[0]->kernelDescriptor;
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesSamplers);

    const auto &samplerTable = kernelDescriptor.payloadMappings.samplerTable;
    EXPECT_EQ(1U, samplerTable.numSamplers);
    EXPECT_EQ(0U, samplerTable.borderColor);
    EXPECT_EQ(64U, samplerTable.tableOffset);

    const auto &args = kernelDescriptor.payloadMappings.explicitArgs;
    ASSERT_EQ(1U, args.size());
    EXPECT_EQ(64U, args[0].as<ArgDescSampler>().bindful);

    const auto &heapInfo = programInfo.kernelInfos[0]->heapInfo;
    ASSERT_EQ(128U, heapInfo.DynamicStateHeapSize);
}
TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenTypeIsGlobalAndUsageIsNotPrivateThenFails) {
    {
        NEO::ConstStringRef zeinfo = R"===(
  kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            global
            usage:           spill_fill_space
            size:            64
    ...
    )===";
        NEO::ProgramInfo programInfo;
        NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
        NEO::Yaml::YamlParser parser;
        std::string parseErrors;
        std::string parseWarnings;
        bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
        ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
        NEO::ZebinSections zebinSections;
        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        std::string decodeErrors;
        std::string decodeWarnings;
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
        EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer memory usage type for global allocation type in context of : some_kernel. Expected : private_space.\n", decodeErrors.c_str());
    }

    {
        NEO::ConstStringRef zeinfo = R"===(
  kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            global
            usage:           single_space
            size:            64
    ...
    )===";
        NEO::ProgramInfo programInfo;
        NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
        NEO::Yaml::YamlParser parser;
        std::string parseErrors;
        std::string parseWarnings;
        bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
        ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
        NEO::ZebinSections zebinSections;
        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        std::string decodeErrors;
        std::string decodeWarnings;
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
        EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer memory usage type for global allocation type in context of : some_kernel. Expected : private_space.\n", decodeErrors.c_str());
    }
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenTypeIsSlmThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
  kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            slm
            usage:           spill_fill_space
            size:            64
    ...
    )===";
    NEO::ProgramInfo programInfo;
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Yaml::YamlParser parser;
    std::string parseErrors;
    std::string parseWarnings;
    bool parseSuccess = parser.parse(zeinfo, parseErrors, parseWarnings);
    ASSERT_TRUE(parseSuccess) << parseErrors << " " << parseWarnings;
    NEO::ZebinSections zebinSections;
    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer allocation type in context of : some_kernel.\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenTypeIsGlobalAndUsageIsPrivateThenSetsProperFieldsInDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            global
            usage:           private_space
            size:            256
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(256U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize);
}

TEST(PopulateKernelDescriptor, givenPerSimtThreadBufferWhenPopulatingThenCalculatesCorrectSize) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            global
            usage:           private_space
            size:            256
            is_simt_thread:  true
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(256U * 8, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize);
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenTypeIsScratchThenSetsProperFieldsInDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            scratch
            usage:           private_space
            size:            512
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(512U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perThreadScratchSize[1]);
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenSlotIsProvidedThenSetsProperFieldsInDescriptorInCorrectSlot) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            scratch
            usage:           private_space
            size:            512
            slot : 1
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    EXPECT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(512U, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.perThreadScratchSize[1]);
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWhenSlotIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            scratch
            usage:           private_space
            size:            512
            slot : 2
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid scratch buffer slot 2 in context of : some_kernel. Expected 0 or 1.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateKernelDescriptor, GivenPerThreadMemoryBufferWithMultipleScratchEntriesForTheSameSlotThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:         
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_memory_buffers: 
          - type:            scratch
            usage:           private_space
            size:            512
          - type:            scratch
            usage:           spill_fill_space
            size:            128
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string decodeErrors;
    std::string decodeWarnings;
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_TRUE(decodeWarnings.empty()) << decodeWarnings;
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid duplicated scratch buffer entry 0 in context of : some_kernel.\n", decodeErrors.c_str());
}

TEST(PopulateKernelDescriptor, GivenKernelWithoutCorrespondingTextSectionThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Could not find text section for kernel some_kernel\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateKernelDescriptor, GivenValidExecutionEnvironmentThenPopulateKernelDescriptorProperly) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        actual_kernel_start_offset : 5
        barrier_count : 7
        disable_mid_thread_preemption : true
        grf_count : 13
        has_4gb_buffers : true
        has_fence_for_image_access : true
        has_global_atomics : true
        has_multi_scratch_spaces : true
        has_no_stateless_write : true
        hw_preemption_mode : 2
        inline_data_payload_size : 32
        offset_to_skip_per_thread_data_load : 23
        offset_to_skip_set_ffid_gp : 29
        required_sub_group_size : 16
        simd_size : 32
        slm_size : 1024
        subgroup_independent_forward_progress : true
        required_work_group_size:
          - 8
          - 2
          - 1
        work_group_walk_order_dimensions:
          - 0
          - 1
          - 2 
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());

    auto &kernelDescriptor = programInfo.kernelInfos[0]->kernelDescriptor;
    EXPECT_EQ(7U, kernelDescriptor.kernelAttributes.barrierCount);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.usesBarriers());
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption);
    EXPECT_EQ(13U, kernelDescriptor.kernelAttributes.numGrfRequired);
    EXPECT_EQ(KernelDescriptor::Stateless, kernelDescriptor.kernelAttributes.bufferAddressingMode);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.useGlobalAtomics);
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesStatelessWrites);
    EXPECT_EQ(32, kernelDescriptor.kernelAttributes.inlineDataPayloadSize);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.passInlineData);
    EXPECT_EQ(23U, kernelDescriptor.entryPoints.skipPerThreadDataLoad);
    EXPECT_EQ(29U, kernelDescriptor.entryPoints.skipSetFFIDGP);
    EXPECT_EQ(16U, kernelDescriptor.kernelMetadata.requiredSubGroupSize);
    EXPECT_EQ(32U, kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(1024U, kernelDescriptor.kernelAttributes.slmInlineSize);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress);
    EXPECT_EQ(8U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[0]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[1]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]);
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypeLocalIdWhenOffsetIsNonZeroThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_payload_arguments: 
        - arg_type:        local_id
          offset:          4
          size:            192
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid offset for argument of type local_id in context of : some_kernel. Expected 0.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypeLocalIdWhenSizeIsInvalidThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      per_thread_payload_arguments: 
        - arg_type:        local_id
          offset:          0
          size:            7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type local_id in context of : some_kernel. For simd=8 expected : 32 or 64 or 96. Got : 7 \n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypeLocalIdWhenSizeIsValidThenCalculateNumChannelAndSetEmitLocalIdAccordingly) {
    uint32_t simdSizes[] = {8, 16, 32};
    uint32_t numChannelsOpts[] = {1, 2, 3};

    for (auto simdSize : simdSizes) {
        for (auto numChannels : numChannelsOpts) {
            std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: )===" +
                                 std::to_string(simdSize) + R"===(
              per_thread_payload_arguments: 
                - arg_type:        local_id
                  offset:          0
                  size:            )===" +
                                 std::to_string(((simdSize == 32) ? 32 : 16) * numChannels * sizeof(short)) + R"===(
        )===";
            NEO::ProgramInfo programInfo;
            ZebinTestData::ValidEmptyProgram zebin;
            zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
            std::string errors, warnings;
            auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
            ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

            NEO::Yaml::YamlParser parser;
            bool parseSuccess = parser.parse(zeinfo, errors, warnings);
            ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

            NEO::ZebinSections zebinSections;
            auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
            ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

            auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
            auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
            EXPECT_EQ(NEO::DecodeError::Success, err) << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(errors.empty()) << errors << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(warnings.empty()) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            ASSERT_EQ(1U, programInfo.kernelInfos.size());
            EXPECT_EQ(numChannels, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.numLocalIdChannels) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(simdSize, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.simdSize) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;

            const auto &emitLocalId = programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.localId;
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 0), emitLocalId[0]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 1), emitLocalId[1]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 2), emitLocalId[2]);
        }
    }
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypePackedLocalIdWhenOffsetIsNonZeroThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 1
      per_thread_payload_arguments: 
        - arg_type:        packed_local_ids
          offset:          4
          size:            6
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Unhandled offset for argument of type packed_local_ids in context of : some_kernel. Expected 0.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypePackedLocalIdWhenSizeIsInvalidThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 1
      per_thread_payload_arguments: 
        - arg_type:        packed_local_ids
          offset:          0
          size:            1
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type packed_local_ids in context of : some_kernel. Expected : 2 or 4 or 6. Got : 1 \n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorPerThreadPayload, GivenArgTypePackedLocalIdWhenSizeIsValidThenCalculateNumChannelAndSetEmitLocalIdAccordingly) {
    uint32_t simdSizes[] = {1};
    uint32_t numChannelsOpts[] = {1, 2, 3};

    for (auto simdSize : simdSizes) {
        for (auto numChannels : numChannelsOpts) {
            std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: )===" +
                                 std::to_string(simdSize) + R"===(
              per_thread_payload_arguments: 
                - arg_type:        packed_local_ids
                  offset:          0
                  size:            )===" +
                                 std::to_string(numChannels * sizeof(short)) + R"===(
        )===";
            NEO::ProgramInfo programInfo;
            ZebinTestData::ValidEmptyProgram zebin;
            zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
            std::string errors, warnings;
            auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
            ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

            NEO::Yaml::YamlParser parser;
            bool parseSuccess = parser.parse(zeinfo, errors, warnings);
            ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

            NEO::ZebinSections zebinSections;
            auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
            ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

            auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
            auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
            EXPECT_EQ(NEO::DecodeError::Success, err) << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(errors.empty()) << errors << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(warnings.empty()) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            ASSERT_EQ(1U, programInfo.kernelInfos.size());
            EXPECT_EQ(numChannels, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.numLocalIdChannels) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(simdSize, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.simdSize) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;

            const auto &emitLocalId = programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.localId;
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 0), emitLocalId[0]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 1), emitLocalId[1]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 2), emitLocalId[2]);
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenBufferPointerArgWhenAddressSpaceIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AddressSpace = NEO::KernelArgMetadata::AddressSpaceQualifier;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AddrSpace;
    std::pair<NEO::ConstStringRef, AddressSpace> addressSpaces[] = {
        {global, AddressSpace::AddrGlobal},
        {local, AddressSpace::AddrLocal},
        {constant, AddressSpace::AddrConstant},
        {"", AddressSpace::AddrUnknown},
    };

    for (auto addressSpace : addressSpaces) {
        std::string zeinfo = R"===(
        kernels:
            - name : 'some_kernel'
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : arg_bypointer
                  offset : 16
                  size : 8	
                  arg_index	: 0
                  addrmode : stateless
                  )===" + (addressSpace.first.empty() ? "" : ("addrspace	: " + addressSpace.first.str())) +
                             R"===(
                  access_type : readwrite
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        EXPECT_TRUE(programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_EQ(addressSpace.second, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].getTraits().getAddressQualifier());
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenPointerArgWhenAddressSpaceIsImageThenPopulatesArgDescriptorAccordingly) {
    using AddressSpace = NEO::KernelArgMetadata::AddressSpaceQualifier;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AddrSpace;

    std::string zeinfo = R"===(
    kernels:
        - name : 'some_kernel'
            execution_env:   
                simd_size: 32
            payload_arguments: 
              - arg_type : arg_bypointer
                arg_index	: 0
                addrspace:       image
                access_type:     readwrite
                addrmode: stateful
    )===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    ASSERT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTImage>());
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenPointerArgWhenAddressSpaceIsSamplerThenPopulatesArgDescriptorAccordingly) {
    using AddressSpace = NEO::KernelArgMetadata::AddressSpaceQualifier;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AddrSpace;

    std::string zeinfo = R"===(
    kernels:
        - name : 'some_kernel'
            execution_env:   
                simd_size: 32
            payload_arguments: 
                - arg_type : arg_bypointer
                  arg_index	: 0
                  addrspace:       sampler
                  access_type:     readwrite
                  addrmode: stateful
    )===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    ASSERT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    EXPECT_TRUE(programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::ArgTSampler>());
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenPointerArgWhenAccessQualifierIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AccessQualifier = NEO::KernelArgMetadata::AccessQualifier;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AccessType;
    std::pair<NEO::ConstStringRef, AccessQualifier> accessQualifiers[] = {
        {readonly, AccessQualifier::AccessReadOnly},
        {writeonly, AccessQualifier::AccessWriteOnly},
        {readwrite, AccessQualifier::AccessReadWrite},
        {"", AccessQualifier::AccessUnknown},
    };

    for (auto accessQualifier : accessQualifiers) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : arg_bypointer
                  offset : 16
                  size : 8	
                  arg_index	: 0
                  addrmode : stateless
                  )===" + (accessQualifier.first.empty() ? "" : ("access_type	: " + accessQualifier.first.str())) +
                             R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        EXPECT_EQ(accessQualifier.second, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].getTraits().getAccessQualifier()) << accessQualifier.first.str();
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenNonPointerArgWhenAddressSpaceIsStatelessThenFails) {
    using AccessQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::AddrSpace;
    NEO::ConstStringRef nonPtrAddrSpace[] = {image, sampler};

    for (auto addrSpace : nonPtrAddrSpace) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : arg_bypointer
                  offset : 16
                  size : 8	
                  arg_index	: 0
                  addrmode : stateless
                  addrspace : )===" +
                             addrSpace.str() +
                             R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        EXPECT_STREQ("Invalid or missing memory addressing stateless for arg idx : 0 in context of : some_kernel.\n", errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenPointerArgWhenMemoryAddressingModeIsUknownThenFail) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:   
        simd_size: 8
      payload_arguments: 
        - arg_type : arg_bypointer
            offset : 16
            size : 8	
            arg_index	: 0
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("Invalid or missing memory addressing mode for arg idx : 0 in context of : some_kernel.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenPointerArgWhenMemoryAddressingModeIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AddressingMode = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingMode;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::MemoryAddressingMode;
    std::pair<NEO::ConstStringRef, AddressingMode> addressingModes[] = {{stateful, AddressingMode::MemoryAddressingModeStateful},
                                                                        {stateless, AddressingMode::MemoryAddressingModeStateless},
                                                                        {bindless, AddressingMode::MemoryAddressingModeBindless},
                                                                        {sharedLocalMemory, AddressingMode::MemoryAddressingModeSharedLocalMemory}};

    for (auto addressingMode : addressingModes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : arg_bypointer
                  offset : 16
                  size : 8	
                  arg_index	: 0
                  )===" + (addressingMode.first.empty() ? "" : ("addrmode	: " + addressingMode.first.str())) +
                             R"===(
        )===";
        uint32_t expectedArgsCount = 1U;
        bool statefulOrBindlessAdressing = (AddressingMode::MemoryAddressingModeStateful == addressingMode.second) || (AddressingMode::MemoryAddressingModeBindless == addressingMode.second);
        if (statefulOrBindlessAdressing) {
            zeinfo += R"===(
                -arg_type : arg_bypointer
                    offset : 24
                    size : 8
                    arg_index : 1
                    addrspace:       image
                    )===" +
                      (addressingMode.first.empty() ? "" : ("addrmode	: " + addressingMode.first.str())) + R"===(
                -arg_type : arg_bypointer
                    offset : 32
                    size : 8
                    arg_index : 2
                    addrspace:       sampler
                    )===" +
                      (addressingMode.first.empty() ? "" : ("addrmode	: " + addressingMode.first.str())) + R"===(
        )===";
            expectedArgsCount += 2;
        }
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(expectedArgsCount, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        auto &argAsPointer = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
        switch (addressingMode.second) {
        default:
            EXPECT_EQ(AddressingMode::MemoryAddressingModeStateful, addressingMode.second);
            break;
        case AddressingMode::MemoryAddressingModeStateless:
            EXPECT_EQ(16, argAsPointer.stateless);
            EXPECT_EQ(8, argAsPointer.pointerSize);
            break;
        case AddressingMode::MemoryAddressingModeBindless:
            EXPECT_EQ(16, argAsPointer.bindless);
            break;
        case AddressingMode::MemoryAddressingModeSharedLocalMemory:
            EXPECT_EQ(16, argAsPointer.slmOffset);
            EXPECT_EQ(16, argAsPointer.requiredSlmAlignment);
            break;
        }

        if (statefulOrBindlessAdressing) {
            auto &argAsImage = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[1].as<NEO::ArgDescImage>();
            auto &argAsSampler = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[2].as<NEO::ArgDescSampler>();
            switch (addressingMode.second) {
            default:
                ASSERT_FALSE(true);
                break;
            case AddressingMode::MemoryAddressingModeStateful:
                break;
            case AddressingMode::MemoryAddressingModeBindless:
                EXPECT_EQ(24U, argAsImage.bindless);
                EXPECT_EQ(32U, argAsSampler.bindless);
                break;
            }
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeLocalSizeWhenArgSizeIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : local_size
                  offset : 16
                  size : 7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type local_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeLocalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
    uint32_t vectorSizes[] = {4, 8, 12};

    for (auto vectorSize : vectorSizes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : local_size
                  offset : 16
                  size : )===" +
                             std::to_string(vectorSize) + R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGlobaIdOffsetWhenArgSizeIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : global_id_offset
                  offset : 16
                  size : 7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type global_id_offset in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossThreadPayload, GivenArgTypePrivateBaseStatelessWhenArgSizeValidThenPopulatesKernelDescriptor) {
    std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : private_base_stateless
                  offset : 16
                  size : 8
    )===";

    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos.size());
    ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    ASSERT_EQ(16U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.stateless);
    ASSERT_EQ(8U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.pointerSize);
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGlobaIdOffsetWhenArgSizeValidThenPopulatesKernelDescriptor) {
    uint32_t vectorSizes[] = {4, 8, 12};

    for (auto vectorSize : vectorSizes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : global_id_offset
                  offset : 16
                  size : )===" +
                             std::to_string(vectorSize) + R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkOffset[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGroupCountWhenArgSizeIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : group_count
                  offset : 16
                  size : 7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type group_count in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGroupCountWhenArgSizeValidThenPopulatesKernelDescriptor) {
    uint32_t vectorSizes[] = {4, 8, 12};

    for (auto vectorSize : vectorSizes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : group_count
                  offset : 16
                  size : )===" +
                             std::to_string(vectorSize) + R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.numWorkGroups[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeEnqueuedLocalSizeWhenArgSizeIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : enqueued_local_size
                  offset : 16
                  size : 7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type enqueued_local_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeByPointerWithSlmAlignmentSetThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
               - arg_type : arg_bypointer
                 offset : 0
                 size : 8
                 arg_index	: 0
                 addrmode	: slm
                 slm_alignment: 8
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto res = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, res);
    ASSERT_EQ(1u, programInfo.kernelInfos.size());
    ASSERT_EQ(1u, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    const auto &arg = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(8, arg.requiredSlmAlignment);
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeEnqueuedLocalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
    uint32_t vectorSizes[] = {4, 8, 12};

    for (auto vectorSize : vectorSizes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : enqueued_local_size
                  offset : 16
                  size : )===" +
                             std::to_string(vectorSize) + R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGlobalSizeWhenArgSizeIsInvalidThenFails) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : global_size
                  offset : 16
                  size : 7
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::Zebin : Invalid size for argument of type global_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(PopulateArgDescriptorCrossthreadPalyoad, GivenArgTypeGlobalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
    uint32_t vectorSizes[] = {4, 8, 12};

    for (auto vectorSize : vectorSizes) {
        std::string zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type : global_size
                  offset : 16
                  size : )===" +
                             std::to_string(vectorSize) + R"===(
        )===";
        NEO::ProgramInfo programInfo;
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
        ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

        NEO::Yaml::YamlParser parser;
        bool parseSuccess = parser.parse(zeinfo, errors, warnings);
        ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

        NEO::ZebinSections zebinSections;
        auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
        ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

        auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
        auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::Success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, programInfo.kernelInfos.size());
        ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.globalWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeBufferOffsetWhenOffsetAndSizeValidThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type: buffer_offset
                  offset: 8
                  size: 4
                  arg_index: 0
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
    const auto &arg = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(8, arg.bufferOffset);
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenNoArgsThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(0U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.explicitArgs.size());
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeBufferOffsetWhenSizeIsInvalidThenPopulateArgDescriptorFails) {
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT bufferOffsetArg;
    bufferOffsetArg.argType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeBufferOffset;
    bufferOffsetArg.offset = 8;
    bufferOffsetArg.argIndex = 0;

    for (auto size : {1, 2, 8}) {
        bufferOffsetArg.size = size;

        uint32_t crossThreadData = 0;
        std::string errors, warnings;

        auto err = NEO::populateArgDescriptor(bufferOffsetArg, kernelDescriptor, crossThreadData, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        auto expectedError = "DeviceBinaryFormat::Zebin : Invalid size for argument of type buffer_offset in context of : some_kernel. Expected 4. Got : " + std::to_string(size) + "\n";
        EXPECT_STREQ(expectedError.c_str(), errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeWorkDimensionsWhenSizeIsValidThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:   
                simd_size: 32
              payload_arguments: 
                - arg_type: work_dimensions
                  offset: 32
                  size: 4
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(32U, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.dispatchTraits.workDim);
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeImplicitArgBufferWhenPopulatingKernelDescriptorThenProperOffsetIsSetAndImplicitArgsAreRequired) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      payload_arguments:
        - arg_type:        implicit_arg_buffer
          offset:          4
          size:            8
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(4u, programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    EXPECT_TRUE(programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeWorkDimensionsWhenSizeIsInvalidThenPopulateKernelDescriptorFails) {
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT workDimensionsArg;
    workDimensionsArg.argType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeWorkDimensions;
    workDimensionsArg.offset = 0x20;

    for (auto size : {1, 2, 8}) {
        workDimensionsArg.size = size;

        uint32_t crossThreadData = 0;
        std::string errors, warnings;

        auto err = NEO::populateArgDescriptor(workDimensionsArg, kernelDescriptor, crossThreadData, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, err);
        auto expectedError = "DeviceBinaryFormat::Zebin : Invalid size for argument of type work_dimensions in context of : some_kernel. Expected 4. Got : " + std::to_string(size) + "\n";
        EXPECT_STREQ(expectedError.c_str(), errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypePrintfBufferWhenOffsetAndSizeIsValidThenPopulatesKernelDescriptor) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: printf_buffer
                  offset: 32
                  size: 8
)===";
    NEO::ProgramInfo programInfo;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseSuccess) << errors << " " << warnings;

    NEO::ZebinSections zebinSections;
    auto extractErr = NEO::extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::Success, extractErr) << errors << " " << warnings;

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    auto err = NEO::populateKernelDescriptor(programInfo, elf, zebinSections, parser, kernelNode, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto printfSurfaceAddress = programInfo.kernelInfos[0]->kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress;
    ASSERT_EQ(32U, printfSurfaceAddress.stateless);
    EXPECT_EQ(8U, printfSurfaceAddress.pointerSize);
}
class IntelGTNotesFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        zebin.elfHeader->type = Elf::ET_REL;
        zebin.elfHeader->machine = Elf::ELF_MACHINE::EM_INTELGT;

        for (int i = 0; i < 3; ++i) {
            notes[i].nameSize = 8;
            notes[i].descSize = 4;
            strcpy_s(notes[i].ownerName, notes[i].nameSize, Elf::IntelGtNoteOwnerName.str().c_str());
        }
    }
    ZebinTestData::ValidEmptyProgram zebin;
    Elf::IntelGTNote notes[3];
};

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenValidIntelGTNotesSectionThenReturnsIntelGTNotes) {
    notes[0].type = Elf::IntelGTSectionType::ProductFamily;
    notes[0].desc = IGFX_SKYLAKE;

    notes[1].type = Elf::IntelGTSectionType::GfxCore;
    notes[1].desc = IGFX_GEN9_CORE;

    Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = 5;
    targetMetadata.maxHwRevisionId = 7;
    notes[2].type = Elf::IntelGTSectionType::TargetMetadata;
    notes[2].desc = targetMetadata.packed;

    zebin.appendSection(Elf::SHT_NOTE, Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    auto intelGTNotesRead = getIntelGTNotes(elf);
    EXPECT_EQ(3U, intelGTNotesRead.size());

    auto validNotes = true;
    for (size_t i = 0; i < intelGTNotesRead.size(); ++i) {
        validNotes &= (0 == memcmp(intelGTNotesRead[i], &notes[i], sizeof(Elf::IntelGTNote)));
    }
    EXPECT_TRUE(validNotes);
}

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenInvalidIntelGTNotesSectionNameThenSectionsIsSkipped) {
    zebin.appendSection(Elf::SHT_NOTE, ".note.wrong.name", ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    auto intelGTNotesRead = getIntelGTNotes(elf);
    EXPECT_EQ(0U, intelGTNotesRead.size());
}

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenInvalidIntelGTNotesThenTheNoteIsSkipped) {
    notes[0].nameSize = 0xbad;
    notes[1].descSize = 0xbad;
    strcpy_s(notes[2].ownerName, notes[2].nameSize, "badName");
    zebin.appendSection(Elf::SHT_NOTE, Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    auto intelGTNotesRead = getIntelGTNotes(elf);
    EXPECT_EQ(0U, intelGTNotesRead.size());
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndValidNotesThenReturnTrue) {
    TargetDevice targetDevice;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.coreFamily = IGFX_GEN9_CORE;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = 6;
    notes[0].type = Elf::IntelGTSectionType::ProductFamily;
    notes[0].desc = targetDevice.productFamily;

    notes[1].type = Elf::IntelGTSectionType::GfxCore;
    notes[1].desc = targetDevice.coreFamily;

    Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = targetDevice.stepping - 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 1;
    notes[2].type = Elf::IntelGTSectionType::TargetMetadata;
    notes[2].desc = targetMetadata.packed;

    zebin.appendSection(Elf::SHT_NOTE, Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    EXPECT_TRUE(validateTargetDevice(elf, targetDevice));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndNoNotesThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.coreFamily = IGFX_GEN9_CORE;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = 6;

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenInvalidTargetDeviceAndValidNotesThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.coreFamily = IGFX_GEN9_CORE;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = 6;
    notes[0].type = Elf::IntelGTSectionType::ProductFamily;
    notes[0].desc = targetDevice.productFamily + 2;

    notes[1].type = Elf::IntelGTSectionType::GfxCore;
    notes[1].desc = targetDevice.coreFamily + 2;

    Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = targetDevice.stepping + 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 3;
    notes[2].type = Elf::IntelGTSectionType::TargetMetadata;
    notes[2].desc = targetMetadata.packed;

    zebin.appendSection(Elf::SHT_NOTE, Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(notes, 3));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndInvalidNoteTypeThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = IGFX_SKYLAKE;
    targetDevice.coreFamily = IGFX_GEN9_CORE;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = 6;
    notes[0].type = 4;
    notes[0].desc = 0;
    zebin.appendSection(Elf::SHT_NOTE, Elf::SectionsNamesZebin::noteIntelGT, ArrayRef<uint8_t>::fromAny(&notes[0], 1));

    std::string outErrReason, outWarning;
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    ASSERT_TRUE(outWarning.empty());
    ASSERT_TRUE(outErrReason.empty());

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice));
}

TEST(PopulateGlobalDeviceHostNameMapping, givenValidZebinWithGlobalHostAccessTableSectionThenPopulateHostDeviceNameMapCorrectly) {
    NEO::ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
        global_host_access_table:
            - device_name:     int_var
              host_name:       IntVarName
            - device_name:     bool_var
              host_name:       BoolVarName
    )===";

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
    zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string decodeErrors;
    std::string decodeWarnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
    EXPECT_EQ(NEO::DecodeError::Success, error);
    EXPECT_TRUE(decodeErrors.empty()) << decodeErrors;

    EXPECT_EQ(2u, programInfo.globalsDeviceToHostNameMap.size());
    EXPECT_STREQ("IntVarName", programInfo.globalsDeviceToHostNameMap["int_var"].c_str());
    EXPECT_STREQ("BoolVarName", programInfo.globalsDeviceToHostNameMap["bool_var"].c_str());
}

TEST(PopulateGlobalDeviceHostNameMapping, givenZebinWithGlobalHostAccessTableSectionAndInvalidValuesThenReturnInvalidBinaryError) {
    std::vector<NEO::ConstStringRef> invalidZeInfos{R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
        global_host_access_table:
            - device_name:     2
              host_name:       HostSymboName
    )===",
                                                    R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
        global_host_access_table:
            - device_name:     DeviceSymbolName
              host_name:       0xddd
    )==="};
    for (auto &zeInfo : invalidZeInfos) {
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.removeSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo);
        zebin.appendSection(NEO::Elf::SHT_ZEBIN::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "some_kernel", {});

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string decodeErrors;
        std::string decodeWarnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(programInfo, singleBinary, decodeErrors, decodeWarnings);
        EXPECT_EQ(NEO::DecodeError::InvalidBinary, error);
    }
}

TEST(PopulateGlobalDeviceHostNameMapping, givenZebinWithGlobalHostAccessTableSectionAndUnrecognizableKeyThenEmitWarning) {
    NEO::ConstStringRef yaml = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
        global_host_access_table:
            - device_name:     int_var
              host_name:       IntVarName
              banana_type:     slight_curve
    )===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &tableNode = *parser.findNodeWithKeyDfs("global_host_access_table");
    std::string errors;
    std::string warnings;
    NEO::ZeInfoGlobalHostAccessTables tables;
    auto err = NEO::readZeInfoGlobalHostAceessTable(parser, tableNode, tables, "global_host_access_table", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;

    std::string expectedWarning("DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"banana_type\" for payload argument in context of global_host_access_table\n");
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
}

TEST(PopulateZeInfoExternalFunctionsMetadata, GivenValidExternalFunctionsMetadataThenParsesItProperly) {
    {
        NEO::ConstStringRef yaml = R"===(---
functions:
  - name: fun0
    execution_env:
      grf_count:       128
      simd_size:       8
      barrier_count:   1
  - name: fun1
    execution_env:
      grf_count:       128
      simd_size:       8
...
)===";

        std::string parserErrors;
        std::string parserWarnings;
        Yaml::YamlParser parser;
        bool success = parser.parse(yaml, parserErrors, parserWarnings);
        EXPECT_TRUE(parserErrors.empty()) << parserErrors;
        EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
        ASSERT_TRUE(success);

        auto &functionsNode = *parser.findNodeWithKeyDfs(Elf::ZebinKernelMetadata::Tags::functions);
        std::string errors;
        std::string warnings;
        ProgramInfo programInfo;
        for (const auto &functionNd : parser.createChildrenRange(functionsNode)) {
            auto err = populateExternalFunctionsMetadata(programInfo, parser, functionNd, errors, warnings);
            EXPECT_EQ(DecodeError::Success, err);
            EXPECT_TRUE(errors.empty()) << errors;
            EXPECT_TRUE(warnings.empty()) << warnings;
        }

        ASSERT_EQ(2U, programInfo.externalFunctions.size());
        auto &fun0Info = programInfo.externalFunctions[0];
        EXPECT_STREQ("fun0", fun0Info.functionName.c_str());
        EXPECT_EQ(128U, fun0Info.numGrfRequired);
        EXPECT_EQ(8U, fun0Info.simdSize);
        EXPECT_EQ(1U, fun0Info.barrierCount);

        auto &fun1Info = programInfo.externalFunctions[1];
        EXPECT_STREQ("fun1", fun1Info.functionName.c_str());
        EXPECT_EQ(128U, fun1Info.numGrfRequired);
        EXPECT_EQ(8U, fun1Info.simdSize);
        EXPECT_EQ(0U, fun1Info.barrierCount);
    }
}

TEST(PopulateZeInfoExternalFunctionsMetadata, GivenValidExternalFunctionsMetadataWithUnknownEntriesThenParsesItProperly) {
    NEO::ConstStringRef yaml = R"===(---
functions:
  - name: fun0
    execution_env:
      grf_count:       128
      simd_size:       8
      barrier_count:   1
    unknown: 12345
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &functionsNode = *parser.findNodeWithKeyDfs(Elf::ZebinKernelMetadata::Tags::functions);
    auto &functionNode = *parser.createChildrenRange(functionsNode).begin();
    std::string errors;
    std::string warnings;
    ProgramInfo programInfo;
    auto err = populateExternalFunctionsMetadata(programInfo, parser, functionNode, errors, warnings);
    EXPECT_EQ(DecodeError::Success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    const auto expectedWarning = "DeviceBinaryFormat::Zebin::.ze_info : Unknown entry \"unknown\" in context of : external functions\n";
    EXPECT_STREQ(expectedWarning, warnings.c_str());

    ASSERT_EQ(1U, programInfo.externalFunctions.size());
    auto &fun0Info = programInfo.externalFunctions[0];
    EXPECT_STREQ("fun0", fun0Info.functionName.c_str());
    EXPECT_EQ(128U, fun0Info.numGrfRequired);
    EXPECT_EQ(8U, fun0Info.simdSize);
    EXPECT_EQ(1U, fun0Info.barrierCount);
}

TEST(PopulateZeInfoExternalFunctionsMetadata, GivenInvalidExternalFunctionsMetadataThenFail) {
    NEO::ConstStringRef yaml = R"===(---
functions:
  - name: fun0
    execution_env:
      grf_count:       abc
      simd_size:       def
      barrier_count:   ghi
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &functionsNode = *parser.findNodeWithKeyDfs(Elf::ZebinKernelMetadata::Tags::functions);
    auto &functionNode = *parser.createChildrenRange(functionsNode).begin();
    std::string errors;
    std::string warnings;
    ProgramInfo programInfo;
    auto err = populateExternalFunctionsMetadata(programInfo, parser, functionNode, errors, warnings);
    EXPECT_EQ(DecodeError::InvalidBinary, err);
    EXPECT_EQ(0U, programInfo.externalFunctions.size());
    const auto expectedError = "DeviceBinaryFormat::Zebin::.ze_info : could not read grf_count from : [abc] in context of : external functions\nDeviceBinaryFormat::Zebin::.ze_info : could not read simd_size from : [def] in context of : external functions\nDeviceBinaryFormat::Zebin::.ze_info : could not read barrier_count from : [ghi] in context of : external functions\n";
    EXPECT_STREQ(expectedError, errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}
