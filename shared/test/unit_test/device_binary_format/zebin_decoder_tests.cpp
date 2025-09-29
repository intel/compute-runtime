/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder_ext.h"
#include "shared/source/device_binary_format/zebin/zeinfo_enum_lookup.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "neo_aot_platforms.h"

#include <numeric>
#include <vector>

extern PRODUCT_FAMILY productFamily;
extern GFXCORE_FAMILY renderCoreFamily;

class DecodeZeInfoKernelEntryFixture {
  public:
    DecodeZeInfoKernelEntryFixture() {
    }

    DecodeError decodeZeInfoKernelEntry(ConstStringRef zeinfo) {
        return decodeZeInfoKernelEntryForSpecificVersion(zeinfo, NEO::Zebin::ZeInfo::zeInfoDecoderVersion);
    }

    DecodeError decodeZeInfoKernelEntryForSpecificVersion(ConstStringRef zeinfo, const NEO::Zebin::ZeInfo::Types::Version &zeInfoVersion) {
        kernelDescriptor = std::make_unique<KernelDescriptor>();
        yamlParser = std::make_unique<NEO::Yaml::YamlParser>();
        errors.clear();
        warnings.clear();

        bool parseSuccess = yamlParser->parse(zeinfo, errors, warnings);
        if (false == parseSuccess) {
            return DecodeError::invalidBinary;
        }

        auto &kernelNode = *yamlParser->createChildrenRange(*yamlParser->findNodeWithKeyDfs("kernels")).begin();
        return NEO::Zebin::ZeInfo::decodeZeInfoKernelEntry(*kernelDescriptor, *yamlParser, kernelNode,
                                                           grfSize, minScratchSpaceSize, samplerStateSize, samplerBorderColorStateSize, errors, warnings, zeInfoVersion);
    }

  protected:
    void setUp() {}
    void tearDown() {}
    std::unique_ptr<KernelDescriptor> kernelDescriptor;
    uint32_t grfSize = 32U;
    uint32_t minScratchSpaceSize = 1024U;
    uint32_t samplerStateSize = 16u;
    uint32_t samplerBorderColorStateSize = 64u;
    std::string errors, warnings;

  private:
    std::unique_ptr<Yaml::YamlParser> yamlParser;
};
using namespace NEO::Zebin;

TEST(ZeInfoVersionSupportTest, whenCheckingSupportedZeInfoVersionThenProperValueIsReturned) {
    ZeInfo::Types::Version srcZeInfoVersion{3, 14};
    EXPECT_TRUE(ZeInfo::isAtLeastZeInfoVersion(srcZeInfoVersion, {3, 14}));
    EXPECT_FALSE(ZeInfo::isAtLeastZeInfoVersion(srcZeInfoVersion, {3, 15}));
    EXPECT_TRUE(ZeInfo::isAtLeastZeInfoVersion(srcZeInfoVersion, {3, 13}));
}

TEST(ZebinValidateTargetTest, givenTargetDeviceCreatedUsingHelperFunctionWhenValidatingAgainstAdjustedHwInfoForIgcThenSuccessIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    compilerProductHelper.adjustHwInfoForIgc(hwInfo);

    auto targetDevice = getTargetDevice(rootDeviceEnvironment);

    EXPECT_TRUE(validateTargetDevice(targetDevice, NEO::Elf::EI_CLASS_32, hwInfo.platform.eProductFamily, hwInfo.platform.eRenderCoreFamily, AOT::UNKNOWN_ISA, {}));
}

using decodeZeInfoKernelEntryTest = Test<DecodeZeInfoKernelEntryFixture>;

TEST(ExtractZebinSections, WhenElfDoesNotContainValidStringSectionThenFail) {
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> elfHeader;
    elf.elfFileHeader = &elfHeader;

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    elfHeader.shStrNdx = NEO::Elf::SHN_UNDEF;
    elf.sectionHeaders.resize(1);
    auto decodeError = extractZebinSections(elf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid or missing shStrNdx in elf header\n", errors.c_str());
    elf.sectionHeaders.clear();

    errors.clear();
    warnings.clear();
    elfHeader.shStrNdx = elfHeader.shNum + 1;
    decodeError = extractZebinSections(elf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid or missing shStrNdx in elf header\n", errors.c_str());
}

TEST(ExtractZebinSections, GivenUnknownElfSectionThenFail) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_RESERVED_START, "someSection", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    auto expectedError = "DeviceBinaryFormat::zebin : Unhandled ELF section header type : " + std::to_string(NEO::Elf::SHT_OPENCL_RESERVED_START) + "\n";
    EXPECT_STREQ(expectedError.c_str(), errors.c_str());
}

TEST(ExtractZebinSections, GivenUnknownElfProgbitsSectionThenFail) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, "someSection", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    auto expectedError = "DeviceBinaryFormat::zebin : Unhandled SHT_PROGBITS section : someSection currently supports only : .text (aliased to .text.Intel_Symbol_Table_Void_Program), .text.KERNEL_NAME, .data.const, .data.global and .debug_* .\n";
    EXPECT_STREQ(expectedError, errors.c_str());
}

TEST(ExtractZebinSections, GivenKnownSectionsThenCapturesThemProperly) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::text, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someOtherKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConst, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataGlobal, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConstString, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::debugInfo, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::debugAbbrev, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Zebin::Elf::SectionNames::symtab, std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, NEO::Zebin::Elf::SectionNames::spv, std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_GTPIN_INFO, NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_GTPIN_INFO, NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "someOtherKernel", std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_VISA_ASM, NEO::Zebin::Elf::SectionNames::vIsaAsmPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_MISC, NEO::Zebin::Elf::SectionNames::buildOptions, std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_NOBITS, NEO::Zebin::Elf::SectionNames::dataConstZeroInit.str(), std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_NOBITS, NEO::Zebin::Elf::SectionNames::dataGlobalZeroInit.str(), std::string{});

    elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + "someKernel", std::string{});

    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(1U, sections.textSections.size());
    ASSERT_EQ(2U, sections.textKernelSections.size());
    ASSERT_EQ(2U, sections.gtpinInfoSections.size());

    ASSERT_EQ(1U, sections.globalDataSections.size());
    ASSERT_EQ(1U, sections.constDataSections.size());
    ASSERT_EQ(1U, sections.constDataStringSections.size());
    ASSERT_EQ(1U, sections.zeInfoSections.size());
    ASSERT_EQ(1U, sections.symtabSections.size());
    ASSERT_EQ(1U, sections.spirvSections.size());
    ASSERT_EQ(1U, sections.buildOptionsSection.size());
    ASSERT_EQ(1U, sections.constZeroInitDataSections.size());
    ASSERT_EQ(1U, sections.globalZeroInitDataSections.size());

    auto stringSection = decodedElf.sectionHeaders[decodedElf.elfFileHeader->shStrNdx];
    const char *strings = stringSection.data.toArrayRef<const char>().begin();
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::text.data(), strings + sections.textSections[0]->header->name);
    EXPECT_STREQ((NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someKernel").c_str(), strings + sections.textKernelSections[0]->header->name);
    EXPECT_STREQ((NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someOtherKernel").c_str(), strings + sections.textKernelSections[1]->header->name);
    EXPECT_STREQ((NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "someKernel").c_str(), strings + sections.gtpinInfoSections[0]->header->name);
    EXPECT_STREQ((NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "someOtherKernel").c_str(), strings + sections.gtpinInfoSections[1]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::dataGlobal.data(), strings + sections.globalDataSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::dataConst.data(), strings + sections.constDataSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::dataConstString.data(), strings + sections.constDataStringSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::zeInfo.data(), strings + sections.zeInfoSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::symtab.data(), strings + sections.symtabSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::spv.data(), strings + sections.spirvSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::dataConstZeroInit.data(), strings + sections.constZeroInitDataSections[0]->header->name);
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::dataGlobalZeroInit.data(), strings + sections.globalZeroInitDataSections[0]->header->name);
}

TEST(ExtractZebinSections, GivenEmptyTextSectionThenIgnoresIt) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::text, ArrayRef<const uint8_t>{});

    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    EXPECT_EQ(0U, sections.textSections.size());
}

TEST(ExtractZebinSections, GivenMispelledConstDataSectionThenAllowItButEmitWarning) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, ".data.global_const", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    EXPECT_STREQ("Misspelled section name : .data.global_const, should be : .data.const\n", warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(1U, sections.constDataSections.size());

    auto stringSection = decodedElf.sectionHeaders[decodedElf.elfFileHeader->shStrNdx];
    const char *strings = stringSection.data.toArrayRef<const char>().begin();
    EXPECT_STREQ(".data.global_const", strings + sections.constDataSections[0]->header->name);
}

TEST(ExtractZebinSections, GivenUnknownZebinGtpinInfoSectionThenEmitWarning) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_GTPIN_INFO, ".unknown", std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    std::string expectedWarning{"DeviceBinaryFormat::zebin : Unhandled SHT_ZEBIN_GTPIN_INFO section : .unknown, currently supports only : " + NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "KERNEL_NAME\n"};
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(ExtractZebinSections, GivenUnknownMiscSectionThenEmitWarning) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someKernel", std::string{});
    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Zebin::Elf::SectionNames::symtab, std::string{});
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, std::string{});
    ConstStringRef unknownMiscSectionName = "unknown_misc_section";
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_MISC, unknownMiscSectionName, std::string{});

    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    EXPECT_TRUE(errors.empty()) << errors;
    const auto expectedWarning = "DeviceBinaryFormat::zebin : unhandled SHT_ZEBIN_MISC section : " + unknownMiscSectionName.str() + " currently supports only : " + NEO::Zebin::Elf::SectionNames::buildOptions.str() + ".\n";
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
}

TEST(ExtractZebinSections, GivenUnknownElfNobitsSectionThenEmitWarning) {
    NEO::Elf::ElfEncoder<> elfEncoder;
    ConstStringRef unknownNobitsSectionName = "unknown_bss_section";
    elfEncoder.appendSection(NEO::Elf::SHT_NOBITS, unknownNobitsSectionName, std::string{});
    auto encodedElf = elfEncoder.encode();
    std::string elferrors;
    std::string elfwarnings;
    auto decodedElf = NEO::Elf::decodeElf(encodedElf, elferrors, elfwarnings);

    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto decodeError = extractZebinSections(decodedElf, sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, decodeError);
    EXPECT_TRUE(errors.empty()) << errors;
    auto expectedWarning = "DeviceBinaryFormat::zebin : unhandled SHT_NOBITS section : " + unknownNobitsSectionName.str() + " currently supports only : .bss.const and .bss.global.\n";
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
}

TEST(ValidateZebinSectionsCount, GivenEmptyZebinThenReturnSuccess) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenCorrectNumberOfSectionsThenReturnSuccess) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.constDataSections.resize(1);
    sections.globalDataSections.resize(1);
    sections.spirvSections.resize(1);
    sections.symtabSections.resize(1);
    sections.textKernelSections.resize(10U);
    sections.zeInfoSections.resize(1U);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoConstDataSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.constDataSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .data.const section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoGlobalDataSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.globalDataSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .data.global section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoZeInfoSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.zeInfoSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .ze_info section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoSymtabSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.symtabSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .symtab section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoSpirvSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.spirvSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .spv section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenTwoIntelGTNoteSectionsThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.noteIntelGTSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .note.intelgt.compat section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenMoreThanOneConstZeroInitDataSectionThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.constZeroInitDataSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .bss.const section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZebinSectionsCount, GivenMoreThanOneGlobalZeroInitDataSectionThenFail) {
    ZebinSections sections;
    std::string errors;
    std::string warnings;
    sections.globalZeroInitDataSections.resize(2);
    auto err = validateZebinSectionsCount(sections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at most 1 of .bss.global section, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoSectionIsEmptyThenEmitsWarning) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Expected at least one .ze_info section, got 0\n", warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(PopulateZeInfoVersion, GivenValidVersionFormatThenParsesItProperly) {
    {
        NEO::ConstStringRef yaml = R"===(---
version: '1.0'
...
)===";

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::success, err);
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

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::success, err);
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

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected \'MAJOR.MINOR\' string, got : 100\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '12.'
    ...
    )===";

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : 12.\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '.12'
    ...
    )===";

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : .12\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version: '.'
    ...
    )===";

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : .\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        NEO::ConstStringRef yaml = R"===(---
    version:
    ...
    )===";

        NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
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
        NEO::Zebin::ZeInfo::Types::Version version;
        auto err = NEO::Zebin::ZeInfo::readZeInfoVersionFromZeInfo(version, parser, versionNode, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string\n", errors.c_str()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(ExtractZeInfoKernelSections, GivenKnownSectionsThenCapturesThemProperly) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
      grf_count: 128
      simd_size: 32
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

    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string errors;
    std::string warnings;
    auto errCode = NEO::Zebin::ZeInfo::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", errors, warnings);
    EXPECT_EQ(DecodeError::success, errCode);
    EXPECT_TRUE(errors.empty()) << errors;
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

TEST(DecodeKernelMiscInfo, givenValidKernelMiscInfoSecionThenExplicitArgsExtendedMetadataIsProperlyPopulated) {
    ConstStringRef zeinfo = R"===(---
version:         '1.19'
kernels:
  - name:            kernel1
    execution_env:
      simd_size:       32
    payload_arguments:
      // SOME PAYLOAD ARGUMENTS
      // IT SHOULD NOT GET PARSED
  - name:            kernel2
    execution_env:
      simd_size:       32
    payload_arguments:
      // SOME PAYLOAD ARGUMENTS
      // IT SHOULD NOT GET PARSED
kernels_misc_info:
  - name:            kernel1
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: restrict const volatile
  - name:            kernel2
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
      - index:           1
        name:            b
        address_qualifier: __private
        access_qualifier: NONE
        type_name:       'int;4'
        type_qualifiers: NONE
      - index:           2
        name:            c
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'uint*;8'
        type_qualifiers: const
      - index:           3
        name:            imageA
        address_qualifier: __global
        access_qualifier: __read_only
        type_name:       'image2d_t;8'
        type_qualifiers: NONE
...
)===";
    NEO::ProgramInfo programInfo;
    setKernelMiscInfoPosition(zeinfo, programInfo);
    EXPECT_NE(std::string::npos, programInfo.kernelMiscInfoPos);

    auto kernel1Info = new KernelInfo();
    kernel1Info->kernelDescriptor.kernelMetadata.kernelName = "kernel1";
    auto kernel2Info = new KernelInfo();
    kernel2Info->kernelDescriptor.kernelMetadata.kernelName = "kernel2";

    programInfo.kernelInfos.reserve(2);
    programInfo.kernelInfos.push_back(kernel1Info);
    programInfo.kernelInfos.push_back(kernel2Info);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, zeinfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::success, res);
    EXPECT_TRUE(outErrors.empty());
    EXPECT_TRUE(outWarnings.empty());

    EXPECT_EQ(1u, kernel1Info->kernelDescriptor.explicitArgsExtendedMetadata.size());
    const auto &kernel1ArgInfo1 = kernel1Info->kernelDescriptor.explicitArgsExtendedMetadata.at(0);
    EXPECT_STREQ(kernel1ArgInfo1.argName.c_str(), "a");
    EXPECT_STREQ(kernel1ArgInfo1.addressQualifier.c_str(), "__global");
    EXPECT_STREQ(kernel1ArgInfo1.accessQualifier.c_str(), "NONE");
    EXPECT_STREQ(kernel1ArgInfo1.type.c_str(), "int*");
    EXPECT_STREQ(kernel1ArgInfo1.typeQualifiers.c_str(), "restrict const volatile");

    const auto &kernel1ArgTraits1 = kernel1Info->kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AccessNone, kernel1ArgTraits1.accessQualifier);
    EXPECT_EQ(KernelArgMetadata::AddrGlobal, kernel1ArgTraits1.addressQualifier);
    KernelArgMetadata::TypeQualifiers qual = {};
    qual.restrictQual = true;
    qual.constQual = true;
    qual.volatileQual = true;
    EXPECT_EQ(qual.packed, kernel1ArgTraits1.typeQualifiers.packed);

    EXPECT_EQ(4u, kernel2Info->kernelDescriptor.explicitArgsExtendedMetadata.size());
    const auto &kernel2ArgInfo1 = kernel2Info->kernelDescriptor.explicitArgsExtendedMetadata.at(0);
    EXPECT_STREQ(kernel2ArgInfo1.argName.c_str(), "a");
    EXPECT_STREQ(kernel2ArgInfo1.addressQualifier.c_str(), "__global");
    EXPECT_STREQ(kernel2ArgInfo1.accessQualifier.c_str(), "NONE");
    EXPECT_STREQ(kernel2ArgInfo1.type.c_str(), "int*");
    EXPECT_STREQ(kernel2ArgInfo1.typeQualifiers.c_str(), "NONE");

    const auto &kernel2ArgTraits1 = kernel2Info->kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AccessNone, kernel2ArgTraits1.accessQualifier);
    EXPECT_EQ(KernelArgMetadata::AddrGlobal, kernel2ArgTraits1.addressQualifier);
    qual = {};
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, kernel2ArgTraits1.typeQualifiers.packed);

    const auto &kernel2ArgInfo2 = kernel2Info->kernelDescriptor.explicitArgsExtendedMetadata.at(1);
    EXPECT_STREQ(kernel2ArgInfo2.argName.c_str(), "b");
    EXPECT_STREQ(kernel2ArgInfo2.addressQualifier.c_str(), "__private");
    EXPECT_STREQ(kernel2ArgInfo2.accessQualifier.c_str(), "NONE");
    EXPECT_STREQ(kernel2ArgInfo2.type.c_str(), "int");
    EXPECT_STREQ(kernel2ArgInfo2.typeQualifiers.c_str(), "NONE");

    const auto &kernel2ArgTraits2 = kernel2Info->kernelDescriptor.payloadMappings.explicitArgs.at(1).getTraits();
    EXPECT_EQ(KernelArgMetadata::AccessNone, kernel2ArgTraits2.accessQualifier);
    EXPECT_EQ(KernelArgMetadata::AddrPrivate, kernel2ArgTraits2.addressQualifier);
    qual = {};
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, kernel2ArgTraits2.typeQualifiers.packed);

    const auto &kernel2ArgInfo3 = kernel2Info->kernelDescriptor.explicitArgsExtendedMetadata.at(2);
    EXPECT_STREQ(kernel2ArgInfo3.argName.c_str(), "c");
    EXPECT_STREQ(kernel2ArgInfo3.addressQualifier.c_str(), "__global");
    EXPECT_STREQ(kernel2ArgInfo3.accessQualifier.c_str(), "NONE");
    EXPECT_STREQ(kernel2ArgInfo3.type.c_str(), "uint*");
    EXPECT_STREQ(kernel2ArgInfo3.typeQualifiers.c_str(), "const");

    const auto &kernel2ArgTraits3 = kernel2Info->kernelDescriptor.payloadMappings.explicitArgs.at(2).getTraits();
    EXPECT_EQ(KernelArgMetadata::AccessNone, kernel2ArgTraits3.accessQualifier);
    EXPECT_EQ(KernelArgMetadata::AddrGlobal, kernel2ArgTraits3.addressQualifier);
    qual = {};
    qual.constQual = true;
    EXPECT_EQ(qual.packed, kernel2ArgTraits3.typeQualifiers.packed);

    const auto &kernel2ArgInfo4 = kernel2Info->kernelDescriptor.explicitArgsExtendedMetadata.at(3);
    EXPECT_STREQ(kernel2ArgInfo4.argName.c_str(), "imageA");
    EXPECT_STREQ(kernel2ArgInfo4.addressQualifier.c_str(), "__global");
    EXPECT_STREQ(kernel2ArgInfo4.accessQualifier.c_str(), "__read_only");
    EXPECT_STREQ(kernel2ArgInfo4.type.c_str(), "image2d_t");
    EXPECT_STREQ(kernel2ArgInfo4.typeQualifiers.c_str(), "NONE");

    const auto &kernel2ArgTraits4 = kernel2Info->kernelDescriptor.payloadMappings.explicitArgs.at(3).getTraits();
    EXPECT_EQ(KernelArgMetadata::AccessReadOnly, kernel2ArgTraits4.accessQualifier);
    EXPECT_EQ(KernelArgMetadata::AddrGlobal, kernel2ArgTraits4.addressQualifier);
    qual = {};
    qual.unknownQual = true;
    EXPECT_EQ(qual.packed, kernel2ArgTraits4.typeQualifiers.packed);
}

TEST(DecodeKernelMiscInfo, givenUnrecognizedEntryInKernelsMiscInfoSectionWhenDecodingItThenEmitWarning) {
    NEO::ConstStringRef kernelMiscInfoUnrecognized = R"===(---
kernels_misc_info:
  - name:            some_kernel.0
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
    pickle:          pickle
...
)===";
    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel.0";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoUnrecognized, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::success, res);
    EXPECT_TRUE(outErrors.empty()) << outErrors;

    auto expectedWarning = "DeviceBinaryFormat::zebin : Unrecognized entry: pickle in kernels_misc_info zeInfo's section.\n";
    EXPECT_STREQ(outWarnings.c_str(), expectedWarning);
}

TEST(DecodeKernelMiscInfo, givenUnrecognizedEntryInArgsInfoWhenDecodingKernelsMiscInfoSectionThenEmitWarning) {
    NEO::ConstStringRef kernelMiscInfoUnrecognizedArgInfo = R"===(---
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
        pickle:          pickle
...
)===";
    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoUnrecognizedArgInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::success, res);
    EXPECT_TRUE(outErrors.empty());

    auto expectedWarning = "DeviceBinaryFormat::zebin : KernelMiscInfo : Unrecognized argsInfo member pickle\n";
    EXPECT_STREQ(outWarnings.c_str(), expectedWarning);
}

TEST(DecodeKernelMiscInfo, givenKeysWithInvalidValuesInKernelsMiscInfoWhenDecodingKernelsMiscInfoSectionThenReturnErrorForEachInvalidValue) {
    NEO::ConstStringRef kernelMiscInfoUnrecognizedArgInfo = R"===(---
kernels_misc_info:
  - name:            -
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
  - name:            -
    args_info:
      - index:           0
        name:            b
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
...
)===";
    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoUnrecognizedArgInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);

    EXPECT_NE(std::string::npos, outErrors.find("DeviceBinaryFormat::zebin::.ze_info : could not read name from : [-] in context of : kernels_misc_info\n"));
    EXPECT_NE(std::string::npos, outErrors.find("DeviceBinaryFormat::zebin::.ze_info : could not read name from : [-] in context of : kernels_misc_info\n"));
}

TEST(DecodeKernelMiscInfo, givenKeysWithInvalidValuesInArgsInfoWhenDecodingKernelsMiscInfoSectionThenReturnErrorForEachInvalidValue) {
    NEO::ConstStringRef kernelMiscInfoUnrecognizedArgInfo = R"===(---
kernels_misc_info:
  - name:            kernel_1
    args_info:
      - index:           0
        name:            a
        address_qualifier: -
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
  - name:            kernel_2
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: -
        type_name:       'int*;8'
        type_qualifiers: NONE
...
)===";
    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoUnrecognizedArgInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);

    EXPECT_NE(std::string::npos, outErrors.find("DeviceBinaryFormat::zebin::.ze_info : could not read address_qualifier from : [-] in context of : kernels_misc_info\n"));
    EXPECT_NE(std::string::npos, outErrors.find("DeviceBinaryFormat::zebin::.ze_info : could not read access_qualifier from : [-] in context of : kernels_misc_info\n"));
}

TEST(DecodeKernelMiscInfo, givenArgsInfoEntryWithMissingMembersOtherThanArgIndexWhenDecodingKernelsMiscInfoSectionThenEmitWarningForEachMissingMember) {
    NEO::ConstStringRef kernelMiscInfoEmptyArgsInfo = R"===(---
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - index:           0
        invalid_value:   0
...
)===";

    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoEmptyArgsInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::success, res);
    EXPECT_TRUE(outErrors.empty());
    std::array<std::string, 5> missingMembers = {
        "name",
        "address_qualifier",
        "access_qualifier",
        "type_name",
        "type_qualifiers"};
    for (const auto &missingMember : missingMembers) {
        auto expectedWarning = "DeviceBinaryFormat::zebin : KernelMiscInfo : ArgInfo member \"" + missingMember + "\" missing. Ignoring.\n";
        EXPECT_NE(std::string::npos, outWarnings.find(expectedWarning));
    }
}

TEST(DecodeKernelMiscInfo, givenArgsInfoEntryWithMissingArgIndexWhenDecodingKernelsMiscInfoSectionThenReturnError) {
    NEO::ConstStringRef kernelMiscInfoEmptyArgsInfo = R"===(---
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
...
)===";

    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfoEmptyArgsInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);

    auto expectedError{"DeviceBinaryFormat::zebin : Error : KernelMiscInfo : ArgInfo index missing (has default value -1)"};
    EXPECT_STREQ(outErrors.c_str(), expectedError);
}

TEST(DecodeKernelMiscInfo, whenDecodingKernelsMiscInfoSectionAndParsingErrorIsEncounteredThenReturnError) {
    NEO::ConstStringRef kernelMiscInfo = R"===(---
kernels_misc_info:
    args_info:
// ENFORCE PARSING ERROR
...
)===";
    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);
}

TEST(DecodeKernelMiscInfo, givenKernelMiscInfoEntryWithMissingKernelNameWhenDecodingKernelsMiscInfoSectionThenErrorIsReturned) {
    NEO::ConstStringRef kernelMiscInfo = R"===(---
kernels_misc_info:
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
...
)===";
    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);

    auto expectedError{"DeviceBinaryFormat::zebin : Error : Missing kernel name in kernels_misc_info section.\n"};
    EXPECT_STREQ(outErrors.c_str(), expectedError);
}

TEST(DecodeKernelMiscInfo, givenKernelMiscInfoEntryAndProgramInfoWihoutCorrespondingKernelInfoWhenDecodingKernelsMiscInfoSectionThenErrorIsReturned) {
    NEO::ConstStringRef kernelMiscInfo = R"===(---
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
...
)===";
    NEO::ProgramInfo programInfo;
    programInfo.kernelMiscInfoPos = 0u;
    auto kernelInfo = new KernelInfo();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = "invalid_kernel_name";
    programInfo.kernelInfos.push_back(kernelInfo);

    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, kernelMiscInfo, outErrors, outWarnings);
    EXPECT_EQ(DecodeError::invalidBinary, res);

    auto expectedError{"DeviceBinaryFormat::zebin : Error : Cannot find kernel info for kernel some_kernel.\n"};
    EXPECT_STREQ(outErrors.c_str(), expectedError);
}

TEST(DecodeKernelMiscInfo, givenNoKernelMiscInfoSectionAvailableWhenParsingItThenEmitWarningAndReturn) {
    ConstStringRef zeinfo = R"===(---
version:         '1.19'
kernels:
  - name:            kernel1
    execution_env:
      simd_size:       32
    payload_arguments:
      - arg_type:        global_id_offset
        offset:          0
        size:            12
)===";

    NEO::ProgramInfo programInfo;
    setKernelMiscInfoPosition(zeinfo, programInfo);
    EXPECT_EQ(std::string::npos, programInfo.kernelMiscInfoPos);
    std::string outWarnings, outErrors;
    auto res = NEO::Zebin::ZeInfo::decodeAndPopulateKernelMiscInfo(programInfo.kernelMiscInfoPos, programInfo.kernelInfos, zeinfo, outErrors, outWarnings);

    EXPECT_EQ(DecodeError::invalidBinary, res);
    auto expectedError{"DeviceBinaryFormat::zebin : Position of kernels_misc_info not set - may be missing in zeInfo.\n"};
    EXPECT_STREQ(outErrors.c_str(), expectedError);
}

TEST(ExtractZeInfoKernelSections, GivenExperimentalPropertyInKnownSectionsThenSectionIsCapturedProperly) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
      grf_count: 128
      simd_size: 32
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

    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string errors;
    std::string warnings;
    auto errCode = NEO::Zebin::ZeInfo::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", errors, warnings);
    EXPECT_EQ(DecodeError::success, errCode);
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

TEST(ExtractZeInfoKernelSections, GivenUnknownSectionThenEmitsAnError) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    apple :
        - red
        - green
...
)===";

    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    EXPECT_TRUE(parserErrors.empty()) << parserErrors;
    EXPECT_TRUE(parserWarnings.empty()) << parserWarnings;
    ASSERT_TRUE(success);

    auto &kernelNode = *parser.createChildrenRange(*parser.findNodeWithKeyDfs("kernels")).begin();
    std::string errors;
    std::string warnings;
    auto errCode = NEO::Zebin::ZeInfo::extractZeInfoKernelSections(parser, kernelNode, kernelSections, "some_kernel", errors, warnings);
    EXPECT_EQ(DecodeError::unkownZeinfoAttribute, errCode);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"apple\" in context of : some_kernel\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_FALSE(kernelSections.nameNd.empty());
    EXPECT_EQ("name", parser.readKey(*kernelSections.nameNd[0])) << parser.readKey(*kernelSections.nameNd[0]).str();
}

TEST(ValidateZeInfoKernelSectionsCount, GivenCorrectNumberOfSectionsThenReturnSuccess) {
    std::string errors;
    std::string warnings;
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);

    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    kernelSections.bindingTableIndicesNd.resize(1);
    kernelSections.payloadArgumentsNd.resize(1);
    kernelSections.perThreadPayloadArgumentsNd.resize(1);
    kernelSections.perThreadMemoryBuffersNd.resize(1);
    kernelSections.debugEnvNd.resize(1);

    errors.clear();
    warnings.clear();
    err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, WhenNameSectionIsMissingThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.executionEnvNd.resize(1);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of name, got : 0\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, WhenExecutionEnvSectionIsMissingThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of execution_env, got : 0\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoNameSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(2);
    kernelSections.executionEnvNd.resize(1);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of name, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoExecutionEnvSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of execution_env, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoDebugEnvSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.debugEnvNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected at most 1 of debug_env, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoBindingTableIndicesSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.bindingTableIndicesNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected at most 1 of binding_table_indices, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPayloadArgumentsSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.payloadArgumentsNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected at most 1 of payload_arguments, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPerThreadPayloadArgumentsSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.perThreadPayloadArgumentsNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected at most 1 of per_thread_payload_arguments, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ValidateZeInfoKernelSectionsCount, GivenTwoPerThreadMemoryBuffersSectionsThenFail) {
    NEO::Zebin::ZeInfo::ZeInfoKernelSections kernelSections;
    std::string errors;
    std::string warnings;
    kernelSections.nameNd.resize(1);
    kernelSections.executionEnvNd.resize(1);
    kernelSections.perThreadMemoryBuffersNd.resize(2);
    auto err = NEO::Zebin::ZeInfo::validateZeInfoKernelSectionsCount(kernelSections, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected at most 1 of per_thread_memory_buffers, got : 2\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExperimentalProperties(parser,
                                                                    experimentalPropertiesNode,
                                                                    experimentalProperties,
                                                                    "some_kernel",
                                                                    errors,
                                                                    warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExperimentalProperties(parser,
                                                                    experimentalPropertiesNode,
                                                                    experimentalProperties,
                                                                    "some_kernel",
                                                                    errors,
                                                                    warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExperimentalProperties(parser,
                                                                    experimentalPropertiesNode,
                                                                    experimentalProperties,
                                                                    "some_kernel",
                                                                    errors,
                                                                    warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExperimentalProperties(parser,
                                                                    experimentalPropertiesNode,
                                                                    experimentalProperties,
                                                                    "some_kernel",
                                                                    errors,
                                                                    warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT experimentalProperties;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExperimentalProperties(parser,
                                                                    experimentalPropertiesNode,
                                                                    experimentalProperties,
                                                                    "some_kernel",
                                                                    errors,
                                                                    warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidExperimentalPropertiesThenPopulateKernelDescriptorSucceeds) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidSingleChannelLocalSizeThenPopulateKernelDescriptorSucceeds) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        local_size
          offset:          16
          size:            4
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_EQ(16u, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[0]);
    EXPECT_EQ(NEO::undefined<CrossThreadDataOffset>, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[1]);
    EXPECT_EQ(NEO::undefined<CrossThreadDataOffset>, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[2]);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenErrorWhileReadingExperimentalPropertiesThenPopulateKernelDescriptorFails) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessBufferAddressingWhenDecodingZeInfoThenBufferAddressingModeIsBindlessAndStateless) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::BindlessAndStateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenStatelessArgBufferAndMissingHas4GbBuffersInExecEnvWhenDecodingZeInfoThenAssumeBuffersAccessedInStatelessMode) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            8
          arg_index:       0
          addrmode:        stateless
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::Stateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenStatefulImplicitArgsWhenDecodingZeInfoThenNumberOfStatefulArgsIsCorrect) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        const_base
          offset:          48
          size:            8
        - arg_type:        global_base
          offset:          56
          size:            8
        - arg_type:        global_base
          offset:          72
          size:            4
          addrmode:        bindless
...
)===";

    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::BindlessAndStateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
    EXPECT_EQ(1u, kernelDescriptor->kernelAttributes.numArgsStateful);

    EXPECT_EQ(56, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_EQ(72, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless);

    EXPECT_EQ(48, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(undefined<CrossThreadDataOffset>, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless);

    ConstStringRef zeinfo2 = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        const_base
          offset:          48
          size:            8
        - arg_type:        global_base
          offset:          56
          size:            8
        - arg_type:        const_base
          offset:          72
          size:            4
          addrmode:        bindless
...
)===";

    err = decodeZeInfoKernelEntry(zeinfo2);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::BindlessAndStateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
    EXPECT_EQ(1u, kernelDescriptor->kernelAttributes.numArgsStateful);

    EXPECT_EQ(56, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_EQ(undefined<CrossThreadDataOffset>, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless);

    EXPECT_EQ(48, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(72, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless);

    ConstStringRef zeinfo3 = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        const_base
          offset:          48
          size:            8
        - arg_type:        global_base
          offset:          56
          size:            8
        - arg_type:        global_base
          offset:          72
          size:            4
          addrmode:        bindless
        - arg_type:        const_base
          offset:          80
          size:            4
          addrmode:        bindless
...
)===";

    err = decodeZeInfoKernelEntry(zeinfo3);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::BindlessAndStateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
    EXPECT_EQ(2u, kernelDescriptor->kernelAttributes.numArgsStateful);

    EXPECT_EQ(56, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_EQ(72, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless);

    EXPECT_EQ(48, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(80, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenStatefulArgsWhenDecodingZeInfoThenNumberOfStatefulArgsIsCorrect) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
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
        - arg_type:        arg_bypointer
          offset:          8
          size:            4
          arg_index:       1
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(2u, kernelDescriptor->kernelAttributes.numArgsStateful);

    ConstStringRef zeinfo2 = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            4
          arg_index:       0
          addrmode:        stateful
          addrspace:       global
          access_type:     readwrite
        - arg_type:        arg_bypointer
          offset:          8
          size:            4
          arg_index:       1
          addrmode:        stateful
          addrspace:       global
          access_type:     readwrite
...
)===";
    err = decodeZeInfoKernelEntry(zeinfo2);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(2u, kernelDescriptor->kernelAttributes.numArgsStateful);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenStatefulAndBindlessArgsWhenDecodingZeInfoThenInvalidBinaryErrorReturned) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
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
        - arg_type:        const_base
          offset:          136
          size:            8
          bti_value:       1
        - arg_type:        global_base
          offset:          144
          size:            8
          bti_value:       2
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenStatefulSamplerWhenDecodingZeInfoThenNumberOfStatefulArgsDoesNotCountSampler) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            0
          arg_index:       0
          addrmode:        stateful
          addrspace:       sampler
          access_type:     readwrite
          sampler_index:   0
          sampler_type:    texture
        - arg_type:        arg_bypointer
          offset:          8
          size:            4
          arg_index:       1
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(1u, kernelDescriptor->kernelAttributes.numArgsStateful);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessSamplerWhenDecodingZeInfoThenNumberOfStatefulArgsDoesNotCountSampler) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            8
          arg_index:       0
          addrmode:        bindless
          addrspace:       sampler
          access_type:     readwrite
          sampler_index:   0
          sampler_type:    texture
        - arg_type:        arg_bypointer
          offset:          8
          size:            4
          arg_index:       1
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(1u, kernelDescriptor->kernelAttributes.numArgsStateful);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessImageAddressingWhenDecodingZeInfoThenImageAddressingModeIsBindless) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            0
          arg_index:       0
          addrmode:        bindless
          addrspace:       image
          access_type:     readonly
          image_type:      image_2d
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(KernelDescriptor::Bindless, kernelDescriptor->kernelAttributes.imageAddressingMode);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessAndBindfulBufferAddressingWhenDecodingZeInfoThenErrorReturned) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
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
        - arg_type:        arg_bypointer
          offset:          4
          size:            4
          arg_index:       1
          addrmode:        stateful
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : bindless and bindful addressing modes must not be mixed.\n", errors.c_str());
    EXPECT_EQ(KernelDescriptor::BindfulAndStateless, kernelDescriptor->kernelAttributes.bufferAddressingMode);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessAndBindfulImageAddressingWhenDecodingZeInfoThenErrorReturned) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          0
          size:            0
          arg_index:       0
          addrmode:        bindless
          addrspace:       image
          access_type:     readonly
          image_type:      image_2d
        - arg_type:        arg_bypointer
          offset:          8
          size:            0
          arg_index:       1
          addrmode:        stateful
          addrspace:       image
          access_type:     readonly
          image_type:      image_2d
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : bindless and bindful addressing modes must not be mixed.\n", errors.c_str());
    EXPECT_EQ(KernelDescriptor::Bindful, kernelDescriptor->kernelAttributes.imageAddressingMode);
}
TEST(ReadZeInfoExecutionEnvironment, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
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
        has_rtcalls : true
        require_disable_eufusion : true
        has_sample : true
        hw_preemption_mode : 2
        inline_data_payload_size : 32
        offset_to_skip_per_thread_data_load : 23
        offset_to_skip_set_ffid_gp : 29
        required_sub_group_size : 16
        simd_size : 32
        slm_size : 1024
        subgroup_independent_forward_progress : true
        required_work_group_size: [8, 2, 1]
        work_group_walk_order_dimensions: [0, 1, 2]
        thread_scheduling_mode: age_based
        indirect_stateless_count: 2
        has_lsc_stores_with_non_default_l1_cache_controls: true
        has_printf_calls: true
        has_indirect_calls: true
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv{};
    EXPECT_FALSE(execEnv.hasSample);

    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
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
    EXPECT_TRUE(execEnv.hasRTCalls);
    EXPECT_TRUE(execEnv.hasSample);
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
    using ThreadSchedulingMode = NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ThreadSchedulingMode;
    EXPECT_EQ(ThreadSchedulingMode::ThreadSchedulingModeAgeBased, execEnv.threadSchedulingMode);
    EXPECT_EQ(2, execEnv.indirectStatelessCount);
    EXPECT_TRUE(execEnv.hasLscStoresWithNonDefaultL1CacheControls);
    EXPECT_TRUE(execEnv.hasPrintfCalls);
    EXPECT_TRUE(execEnv.hasIndirectCalls);
}

TEST(ReadZeInfoExecutionEnvironment, GivenMinimalExecutionEnvThenSetProperMembersToDefaults) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 32
        grf_count: 128
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &execEnvNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv{};
    EXPECT_FALSE(execEnv.hasSample);

    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(32, execEnv.simdSize);
    EXPECT_EQ(128, execEnv.grfCount);

    namespace Defaults = NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::Defaults;
    EXPECT_EQ(Defaults::barrierCount, execEnv.barrierCount);
    EXPECT_EQ(Defaults::disableMidThreadPreemption, execEnv.disableMidThreadPreemption);
    EXPECT_EQ(Defaults::has4GBBuffers, execEnv.has4GBBuffers);
    EXPECT_EQ(Defaults::hasDpas, execEnv.hasDpas);
    EXPECT_EQ(Defaults::hasFenceForImageAccess, execEnv.hasFenceForImageAccess);
    EXPECT_EQ(Defaults::hasGlobalAtomics, execEnv.hasGlobalAtomics);
    EXPECT_EQ(Defaults::hasMultiScratchSpaces, execEnv.hasMultiScratchSpaces);
    EXPECT_EQ(Defaults::hasNoStatelessWrite, execEnv.hasNoStatelessWrite);
    EXPECT_EQ(Defaults::hasStackCalls, execEnv.hasStackCalls);
    EXPECT_EQ(Defaults::hasRTCalls, execEnv.hasRTCalls);
    EXPECT_EQ(Defaults::hasSample, execEnv.hasSample);
    EXPECT_EQ(Defaults::hwPreemptionMode, execEnv.hwPreemptionMode);
    EXPECT_EQ(Defaults::inlineDataPayloadSize, execEnv.inlineDataPayloadSize);
    EXPECT_EQ(Defaults::offsetToSkipPerThreadDataLoad, execEnv.offsetToSkipPerThreadDataLoad);
    EXPECT_EQ(Defaults::offsetToSkipSetFfidGp, execEnv.offsetToSkipSetFfidGp);
    EXPECT_EQ(Defaults::requiredSubGroupSize, execEnv.requiredSubGroupSize);
    EXPECT_EQ(Defaults::slmSize, execEnv.slmSize);
    EXPECT_EQ(Defaults::subgroupIndependentForwardProgress, execEnv.subgroupIndependentForwardProgress);
    EXPECT_EQ(Defaults::requiredWorkGroupSize[0], execEnv.requiredWorkGroupSize[0]);
    EXPECT_EQ(Defaults::requiredWorkGroupSize[1], execEnv.requiredWorkGroupSize[1]);
    EXPECT_EQ(Defaults::requiredWorkGroupSize[2], execEnv.requiredWorkGroupSize[2]);
    EXPECT_EQ(Defaults::workgroupWalkOrderDimensions[0], execEnv.workgroupWalkOrderDimensions[0]);
    EXPECT_EQ(Defaults::workgroupWalkOrderDimensions[1], execEnv.workgroupWalkOrderDimensions[1]);
    EXPECT_EQ(Defaults::workgroupWalkOrderDimensions[2], execEnv.workgroupWalkOrderDimensions[2]);
    EXPECT_EQ(Defaults::threadSchedulingMode, execEnv.threadSchedulingMode);
    EXPECT_EQ(Defaults::indirectStatelessCount, execEnv.indirectStatelessCount);
    EXPECT_EQ(Defaults::hasLscStoresWithNonDefaultL1CacheControls, execEnv.hasLscStoresWithNonDefaultL1CacheControls);
    EXPECT_EQ(Defaults::hasPrintfCalls, execEnv.hasPrintfCalls);
    EXPECT_EQ(Defaults::hasIndirectCalls, execEnv.hasIndirectCalls);
}

TEST(ReadZeInfoExecutionEnvironment, GivenUnknownEntryThenEmitsError) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size : 8
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"something_new\" in context of some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoExecutionEnvironment, GivenActualKernelStartOffsetThenDontTreatItAsInvalidEntry) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size : 8
        actual_kernel_start_offset: 240
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &argsNode = *parser.findNodeWithKeyDfs("execution_env");
    std::string errors;
    std::string warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, argsNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ReadZeInfoExecutionEnvironment, GivenInvalidValueForKnownEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size : true
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read simd_size from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoExecutionEnvironment, GivenInvalidLengthForKnownCollectionEntryThenFails) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        required_work_group_size: [5, 2]
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
    NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoExecutionEnvironment(parser, execEnvNode, execEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : wrong size of collection required_work_group_size in context of : some_kernel. Got : 2 expected : 3\n", errors.c_str());
}

TEST(ReadZeInfoAttributes, GivenValidYamlEntriesThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    user_attributes:
      intel_reqd_sub_group_size: 16
      intel_reqd_workgroup_walk_order: [0, 1, 2]
      reqd_work_group_size: [256, 2, 1]
      vec_type_hint:   uint
      work_group_size_hint: [256, 2, 1]
      new_user_hint: new_user_hint_value
      invalid_kernel: invalid_kernel_reason
      intel_reqd_thread_group_dispatch_size: 8
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &attributeNd = *parser.findNodeWithKeyDfs(NEO::Zebin::ZeInfo::Tags::Kernel::attributes);
    std::string errors;
    std::string warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::Attributes::AttributesBaseT attributes;
    auto err = NEO::Zebin::ZeInfo::readZeInfoAttributes(parser, attributeNd, attributes, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(16, attributes.intelReqdSubgroupSize.value());
    EXPECT_EQ(0, attributes.intelReqdWorkgroupWalkOrder.value()[0]);
    EXPECT_EQ(1, attributes.intelReqdWorkgroupWalkOrder.value()[1]);
    EXPECT_EQ(2, attributes.intelReqdWorkgroupWalkOrder.value()[2]);
    EXPECT_EQ(256, attributes.reqdWorkgroupSize.value()[0]);
    EXPECT_EQ(2, attributes.reqdWorkgroupSize.value()[1]);
    EXPECT_EQ(1, attributes.reqdWorkgroupSize.value()[2]);
    EXPECT_TRUE(equals(attributes.vecTypeHint.value(), "uint"));
    EXPECT_EQ(256, attributes.workgroupSizeHint.value()[0]);
    EXPECT_EQ(2, attributes.workgroupSizeHint.value()[1]);
    EXPECT_EQ(1, attributes.workgroupSizeHint.value()[2]);
    ASSERT_EQ(1U, attributes.otherHints.size());
    EXPECT_TRUE(equals(attributes.otherHints[0].first, "new_user_hint"));
    EXPECT_TRUE(equals(attributes.otherHints[0].second, "new_user_hint_value"));
    EXPECT_TRUE(equals(attributes.invalidKernel.value(), "invalid_kernel_reason"));
    EXPECT_EQ(8, attributes.intelReqdThreadgroupDispatchSize.value());
}

TEST(ReadZeInfoDebugEnvironment, givenSipSurfaceBtiEntryThenSetProperMembers) {
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
    NEO::Zebin::ZeInfo::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(0, debugEnv.debugSurfaceBTI);
}

TEST(ReadZeInfoDebugEnvironment, givenSipSurfaceOffsetEntryThenSetProperMembers) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    debug_env:
        sip_surface_offset: 0
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
    NEO::Zebin::ZeInfo::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(0, debugEnv.debugSurfaceOffset);
}

TEST(ReadZeInfoDebugEnvironment, givenUnknownEntryThenEmitsError) {
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
    NEO::Zebin::ZeInfo::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"different\" in context of some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    auto err = NEO::Zebin::ZeInfo::readZeInfoDebugEnvironment(parser, argsNode, debugEnv, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read sip_surface_bti from : [any] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, givenValidDebugEnvironmentWithInvalidSipSurfaceBTIThenDoesNotGenerateSSH) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      debug_env:
        sip_surface_bti: 123
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(0U, kernelDescriptor->payloadMappings.bindingTable.numEntries);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor->payloadMappings.bindingTable.tableOffset));
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor->payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful));
    ASSERT_EQ(0U, kernelDescriptor->generatedSsh.size());
}

TEST_F(decodeZeInfoKernelEntryTest, givenValidDebugEnvironmentWithSIPSurfaceBTISetThenGeneratesSshAndSetsSystemThreadSurfaceAddress) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      debug_env:
        sip_surface_bti: 0
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(1U, kernelDescriptor->payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(64U, kernelDescriptor->payloadMappings.bindingTable.tableOffset);
    ASSERT_EQ(128U, kernelDescriptor->generatedSsh.size());
    EXPECT_EQ(0U, kernelDescriptor->payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful);
    EXPECT_EQ(0U, reinterpret_cast<const uint32_t *>(ptrOffset(kernelDescriptor->generatedSsh.data(), 64U))[0]);
}

TEST_F(decodeZeInfoKernelEntryTest, givenErrorWhileReadingDebugEnvironmentThenPopulateKernelDescriptorFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      debug_env:
        sip_surface_bti: any
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgumentByValueWithMoreThanOneElementWithSourceOffsetsSpecifiedThenSetThemAccordingly) {
    ConstStringRef zeinfo = R"===(
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
    namespace Defaults = NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::Defaults;
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    auto res = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, res);
    auto elements = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescValue>().elements;
    EXPECT_EQ(0, elements[0].sourceOffset);
    EXPECT_EQ(1, elements[1].sourceOffset);
}

TEST_F(decodeZeInfoKernelEntryTest, GiveArgumentByValueWithOneElementWithoutSourceOffsetSpecifiedSetItToZero) {
    ConstStringRef zeinfo = R"===(
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
    namespace Defaults = NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::Defaults;
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    auto res = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, res);
    auto elements = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescValue>().elements;
    EXPECT_EQ(0, elements[0].sourceOffset);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgumentByValueWithoutAnySourceOffsetsSpecifiedThenPopulateKernelDescriptorReturnsError) {
    ConstStringRef zeinfo = R"===(
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
    namespace Defaults = NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::Defaults;
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("Missing source offset value for element in argByValue\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenKernelAttributesWhenPopulatingKernelDescriptorThenKernelLanguageSourcesAreSetAccordingly) {
    ConstStringRef zeinfo = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 8
    user_attributes:
      intel_reqd_sub_group_size: 16
      intel_reqd_workgroup_walk_order: [0, 1, 2]
      reqd_work_group_size: [256, 2, 1]
      intel_reqd_thread_group_dispatch_size: 8
      vec_type_hint:   uint
      work_group_size_hint: [256, 2, 1]
      new_user_hint: new_user_hint_value
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    EXPECT_STREQ("new_user_hint(new_user_hint_value) intel_reqd_sub_group_size(16) intel_reqd_workgroup_walk_order(0,1,2) reqd_work_group_size(256,2,1) work_group_size_hint(256,2,1) vec_type_hint(uint) intel_reqd_thread_group_dispatch_size(8)", kernelDescriptor->kernelMetadata.kernelLanguageAttributes.c_str());
    EXPECT_EQ(16U, kernelDescriptor->kernelMetadata.requiredSubGroupSize);
    EXPECT_EQ(8U, kernelDescriptor->kernelMetadata.requiredThreadGroupDispatchSize);
    EXPECT_FALSE(kernelDescriptor->kernelAttributes.flags.isInvalid);
}

TEST(BaseKernelDescriptorAndPayloadArgumentPoinetrsExt, givenKernelDescriptorAndPayloadArgWhenProperlyCreatedThenExtraFieldsSetToNullPtr) {
    NEO::KernelDescriptor kd;
    NEO::Zebin::ZeInfo::KernelPayloadArgBaseT arg;

    EXPECT_EQ(nullptr, arg.pPayArgExt);
}

TEST(PopulateKernelSourceAttributes, GivenInvalidKernelAttributeWhenPopulatingKernelSourceAttributesThenKernelIsInvalidFlagIsSet) {
    NEO::KernelDescriptor kd;
    NEO::Zebin::ZeInfo::KernelAttributesBaseT attributes;
    attributes.invalidKernel = "reason";
    NEO::Zebin::ZeInfo::populateKernelSourceAttributes(kd, attributes);
    EXPECT_TRUE(kd.kernelAttributes.flags.isInvalid);
    EXPECT_STREQ("invalid_kernel(reason)", kd.kernelMetadata.kernelLanguageAttributes.c_str());
}

TEST(PopulateKernelSourceAttributes, GivenAttributesWithoutRequiredWalkOrderWhenPopulatingKernelSourceAttributesThenKernelNotRequireWalkOrder) {
    NEO::KernelDescriptor kd;
    NEO::Zebin::ZeInfo::KernelAttributesBaseT attributes;
    NEO::Zebin::ZeInfo::populateKernelSourceAttributes(kd, attributes);
    EXPECT_FALSE(kd.kernelAttributes.flags.requiresWorkgroupWalkOrder);
}

TEST(PopulateKernelSourceAttributes, GivenRequiredWalkOrderAttributeWhenPopulatingKernelSourceAttributesThenKernelRequireWalkOrder) {
    NEO::KernelDescriptor kd;
    NEO::Zebin::ZeInfo::KernelAttributesBaseT attributes;
    attributes.intelReqdWorkgroupWalkOrder = {0, 2, 1};
    NEO::Zebin::ZeInfo::populateKernelSourceAttributes(kd, attributes);
    EXPECT_TRUE(kd.kernelAttributes.flags.requiresWorkgroupWalkOrder);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenUnknownAttributeWhenPopulatingKernelDescriptorThenErrorIsReturned) {
    ConstStringRef zeinfo = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
        simd_size: 8
    user_attributes:
      unknown_attribute: unkown_attribute_value
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"unknown_attribute\" in context of some_kernel\n", errors.c_str());
}

TEST(ReadZeInfoEnumChecked, GivenInvalidNodeThenFail) {
    using ArgType = NEO::Zebin::ZeInfo::EnumLookup::ArgType::ArgType;
    NEO::Yaml::YamlParser parser;
    NEO::Yaml::Node node;
    ArgType outEnumValue;
    std::string errors;

    bool success = NEO::Zebin::ZeInfo::readZeInfoEnumChecked(parser, node, outEnumValue, "some_kernel", errors);
    EXPECT_FALSE(success);
}

TEST(ReadEnumChecked, GivenInvalidEnumStringThenReturnErrorAndFail) {
    using ArgType = NEO::Zebin::ZeInfo::EnumLookup::ArgType::ArgType;
    ArgType outEnumValue;
    std::string errors;

    bool success = NEO::Zebin::ZeInfo::readEnumChecked("invalid_enum_string_representation", outEnumValue, "some_kernel", errors);
    EXPECT_FALSE(success);
    EXPECT_EQ(ArgType::argTypeUnknown, outEnumValue);

    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unhandled \"invalid_enum_string_representation\" argument type in context of some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::KernelPerThreadPayloadArguments args;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, args.size());
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypePackedLocalIds, args[0].argType);
    EXPECT_EQ(8, args[0].offset);
    EXPECT_EQ(16, args[0].size);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeLocalId, args[1].argType);
    EXPECT_EQ(32, args[1].offset);
    EXPECT_EQ(192, args[1].size);
}

TEST(ReadZeInfoPerThreadPayloadArguments, GivenUnknownEntryThenEmitsError) {
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
    NEO::Zebin::ZeInfo::KernelPerThreadPayloadArguments args;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"something_new\" for per-thread payload argument in context of some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::KernelPerThreadPayloadArguments args;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read offset from : [true] in context of : some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::KernelPerThreadPayloadArguments args;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadPayloadArguments(parser, argsNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Skippinig 0-size per-thread argument of type : packed_local_ids in context of some_kernel\n", warnings.c_str());
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
          is_ptr : true
        - arg_type : const_base
          offset : 32
          size : 8
          bti_value : 1
        - arg_type:  global_base
          offset: 56
          size: 8
          is_ptr: true
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
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    int32_t maxArgIndex = -1;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(2, maxArgIndex);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(4U, args.size());

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeArgBypointer, args[0].argType);
    EXPECT_EQ(16, args[0].offset);
    EXPECT_EQ(8, args[0].size);
    EXPECT_EQ(1, args[0].argIndex);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::memoryAddressingModeStateless, args[0].addrmode);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::addressSpaceGlobal, args[0].addrspace);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::accessTypeReadwrite, args[0].accessType);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeArgByvalue, args[1].argType);
    EXPECT_EQ(24, args[1].offset);
    EXPECT_EQ(4, args[1].size);
    EXPECT_EQ(2, args[1].argIndex);
    EXPECT_TRUE(args[1].isPtr);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataConstBuffer, args[2].argType);
    EXPECT_EQ(32, args[2].offset);
    EXPECT_EQ(8, args[2].size);
    EXPECT_EQ(1, args[2].btiValue);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataGlobalBuffer, args[3].argType);
    EXPECT_EQ(56, args[3].offset);
    EXPECT_EQ(8, args[3].size);
    EXPECT_TRUE(args[3].isPtr);
}

TEST(ReadZeInfoPayloadArguments, GivenUnknownEntryThenEmitsError) {
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
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    int32_t maxArgIndex = -1;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"something_new\" for payload argument in context of some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    int32_t maxArgIndex = -1;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(2U, args.size());
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeArgBypointer, args[0].argType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::argTypeArgBypointer, args[1].argType);
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
    NEO::Zebin::ZeInfo::KernelPayloadArguments args;
    int32_t maxArgIndex = -1;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPayloadArguments(parser, argsNode, args, maxArgIndex, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read size from : [true] in context of : some_kernel\n", errors.c_str());
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
    NEO::Zebin::ZeInfo::KernelBindingTableEntries btis;
    auto err = NEO::Zebin::ZeInfo::readZeInfoBindingTableIndices(parser, btisNode, btis, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(2U, btis.size());

    EXPECT_EQ(1, btis[0].btiValue);
    EXPECT_EQ(7, btis[0].argIndex);

    EXPECT_EQ(5, btis[1].btiValue);
    EXPECT_EQ(13, btis[1].argIndex);
}

TEST(ReadZeInfoBindingTableIndices, GivenUnknownEntryThenEmitsError) {
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
    NEO::Zebin::ZeInfo::KernelBindingTableEntries btis;
    auto err = NEO::Zebin::ZeInfo::readZeInfoBindingTableIndices(parser, argsNode, btis, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"something_new\" for binding table index in context of some_kernel\n", errors.c_str());

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
    NEO::Zebin::ZeInfo::KernelBindingTableEntries btis;
    auto err = NEO::Zebin::ZeInfo::readZeInfoBindingTableIndices(parser, argsNode, btis, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read arg_index from : [any] in context of : some_kernel\n", errors.c_str());
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
      - type:            scratch
        usage:           private_space
        size:            256
        slot:            1
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
    NEO::Zebin::ZeInfo::KernelPerThreadMemoryBuffers buffers;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(3U, buffers.size());

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeScratch, buffers[0].allocationType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::MemoryUsageSingleSpace, buffers[0].memoryUsage);
    EXPECT_FALSE(buffers[0].isSimtThread);
    EXPECT_EQ(64, buffers[0].size);
    EXPECT_EQ(0, buffers[0].slot);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeGlobal, buffers[1].allocationType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::MemoryUsagePrivateSpace, buffers[1].memoryUsage);
    EXPECT_EQ(128, buffers[1].size);
    EXPECT_FALSE(buffers[1].isSimtThread);

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeScratch, buffers[2].allocationType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::MemoryUsagePrivateSpace, buffers[2].memoryUsage);
    EXPECT_FALSE(buffers[2].isSimtThread);
    EXPECT_EQ(256, buffers[2].size);
    EXPECT_EQ(1, buffers[2].slot);
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
    NEO::Zebin::ZeInfo::KernelPerThreadMemoryBuffers buffers;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, buffers.size());

    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeGlobal, buffers[0].allocationType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::MemoryUsagePrivateSpace, buffers[0].memoryUsage);
    EXPECT_EQ(128, buffers[0].size);
    EXPECT_TRUE(buffers[0].isSimtThread);
}

TEST(ReadZeInfoPerThreadMemoryBuffers, GivenUnknownEntryThenEmmitsError) {
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
    NEO::Zebin::ZeInfo::KernelPerThreadMemoryBuffers buffers;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, buffers, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"something_new\" for per-thread memory buffer in context of some_kernel\n", errors.c_str());

    ASSERT_EQ(1U, buffers.size());
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::AllocationTypeScratch, buffers[0].allocationType);
    EXPECT_EQ(NEO::Zebin::ZeInfo::Types::Kernel::PerThreadMemoryBuffer::MemoryUsageSingleSpace, buffers[0].memoryUsage);
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
    NEO::Zebin::ZeInfo::KernelPerThreadMemoryBuffers args;
    auto err = NEO::Zebin::ZeInfo::readZeInfoPerThreadMemoryBuffers(parser, buffersNode, args, "some_kernel", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read size from : [eight] in context of : some_kernel\n", errors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValid32BitZebinThenReturnSuccess) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin;
    singleBinary.deviceBinary = {zebin.storage.data(), zebin.storage.size()};
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenInvalidElfThenReturnError) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_TRUE(warnings.empty());
    EXPECT_FALSE(errors.empty());
    EXPECT_STREQ("Invalid or missing ELF header", errors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, WhenFailedToExtractZebinSectionsThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.elfHeader->shStrNdx = NEO::Elf::SHN_UNDEF;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_FALSE(errors.empty());

    std::string elfErrors;
    std::string elfWarnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, elfErrors, elfWarnings);
    ASSERT_TRUE((nullptr != elf.elfFileHeader) && elfErrors.empty() && elfWarnings.empty());
    ZebinSections sections;
    std::string extractErrors;
    std::string extractWarnings;
    auto extractErr = extractZebinSections(elf, sections, extractErrors, extractWarnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, extractErr);
    EXPECT_STREQ(extractErrors.c_str(), errors.c_str());
    EXPECT_STREQ(extractWarnings.c_str(), warnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, WhenValidationOfZebinSectionsCountFailsThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, NEO::Zebin::Elf::SectionNames::spv, {});
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, NEO::Zebin::Elf::SectionNames::spv, {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_FALSE(errors.empty());

    std::string elfErrors;
    std::string elfWarnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, elfErrors, elfWarnings);
    ASSERT_TRUE((nullptr != elf.elfFileHeader) && elfErrors.empty() && elfWarnings.empty());
    ZebinSections sections;
    std::string extractErrors;
    std::string extractWarnings;
    auto extractErr = extractZebinSections(elf, sections, extractErrors, extractWarnings);
    EXPECT_EQ(NEO::DecodeError::success, extractErr);
    EXPECT_TRUE(extractErrors.empty()) << extractErrors;
    EXPECT_TRUE(extractWarnings.empty()) << extractWarnings;

    std::string validateErrors;
    std::string validateWarnings;
    auto validateErr = validateZebinSectionsCount(sections, validateErrors, validateWarnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, validateErr);
    EXPECT_STREQ(validateErrors.c_str(), errors.c_str());
    EXPECT_STREQ(validateWarnings.c_str(), warnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenGlobalDataSectionThenSetsUpInitDataAndSize) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataGlobal, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(sizeof(data), programInfo.globalVariables.size);
    EXPECT_EQ(0, memcmp(programInfo.globalVariables.initData, data, sizeof(data)));
    EXPECT_EQ(0U, programInfo.globalConstants.size);
    EXPECT_EQ(nullptr, programInfo.globalConstants.initData);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenConstDataSectionThenSetsUpInitDataAndSize) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConst, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(sizeof(data), programInfo.globalConstants.size);
    EXPECT_EQ(0, memcmp(programInfo.globalConstants.initData, data, sizeof(data)));
    EXPECT_EQ(0U, programInfo.globalVariables.size);
    EXPECT_EQ(nullptr, programInfo.globalVariables.initData);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenConstZeroInitDataSectionThenSetUpZeroInitSizeBasedOnHeaderData) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t mockData[0x10]{0u}; // note that BSS section does not store any data in ELF
    auto &bssConstHeader = zebin.appendSection(NEO::Elf::SHT_NOBITS, NEO::Zebin::Elf::SectionNames::dataConstZeroInit, mockData);
    bssConstHeader.size = 16u;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(16u, programInfo.globalConstants.zeroInitSize);
    EXPECT_EQ(nullptr, programInfo.globalConstants.initData);
    EXPECT_EQ(0u, programInfo.globalConstants.size);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenGlobalZeroInitDataSectionThenSetUpZeroInitSizeBasedOnHeaderData) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t mockData[0x10]{0u}; // note that BSS section does not store any data in ELF
    auto &bssGlobalHeader = zebin.appendSection(NEO::Elf::SHT_NOBITS, NEO::Zebin::Elf::SectionNames::dataGlobalZeroInit, mockData);
    bssGlobalHeader.size = 16u;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    EXPECT_EQ(16u, programInfo.globalVariables.zeroInitSize);
    EXPECT_EQ(nullptr, programInfo.globalVariables.initData);
    EXPECT_EQ(0u, programInfo.globalVariables.size);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenConstDataStringsSectionThenSetsUpInitDataAndSize) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', '!'};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConstString, data);

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(sizeof(data), programInfo.globalStrings.size);
    EXPECT_NE(nullptr, programInfo.globalStrings.initData);
    EXPECT_EQ(sizeof(data), programInfo.globalStrings.size);
    EXPECT_EQ(0, memcmp(programInfo.globalStrings.initData, data, sizeof(data)));
}

TEST(DecodeSingleDeviceBinaryZebin, GivenIntelGTNoteSectionThenAddsItToZebinSections) {
    ZebinTestData::ValidEmptyProgram zebin;

    constexpr int mockIntelGTNotesDataSize = 0x10;
    std::array<uint8_t, mockIntelGTNotesDataSize> mockIntelGTNotesData = {};
    zebin.appendSection(NEO::Elf::SHT_NOTE, NEO::Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(mockIntelGTNotesData.data(), mockIntelGTNotesDataSize));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    ZebinSections zebinSections;
    auto extractErr = extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::success, extractErr) << errors << " " << warnings;
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

    ZebinSections zebinSections;
    auto extractErr = extractZebinSections(elf, zebinSections, errors, warnings);
    ASSERT_EQ(NEO::DecodeError::success, extractErr);
    EXPECT_EQ(0u, zebinSections.noteIntelGTSections.size());
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(warnings.empty());
    auto expectedWarning = "DeviceBinaryFormat::zebin : Unhandled SHT_NOTE section : " + sectionName.str() + " currently supports only : " + NEO::Zebin::Elf::SectionNames::noteIntelGT.str() + ".\n";
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenSymtabWithInvalidSymEntriesThenFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[] = {2, 3, 5, 7, 11, 13, 17, 19};
    zebin.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Zebin::Elf::SectionNames::symtab, data).entsize = sizeof(NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64>) - 1;

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_STREQ("Invalid symbol table entries size - expected : 24, got : 23\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenYamlParserForZeInfoFailsThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = NEO::ConstStringRef("unterminated_string : \"");
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_TRUE(warnings.empty()) << warnings;

    NEO::Yaml::YamlParser parser;
    std::string parserErrors;
    std::string parserWarnings;
    bool validYaml = parser.parse((brokenZeInfo), parserErrors, parserWarnings);
    EXPECT_FALSE(validYaml);
    EXPECT_STREQ(parserWarnings.c_str(), warnings.c_str());
    EXPECT_STREQ(parserErrors.c_str(), errors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, GivenEmptyInZeInfoThenEmitsWarning) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = NEO::ConstStringRef("#no data\n");
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_STREQ("NEO::Yaml : Text has no data\nDeviceBinaryFormat::zebin : Empty kernels metadata section (.ze_info)\n", warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenUnknownEntryInZeInfoGlobalScopeThenEmitsError) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = std::string("some_entry : a\nkernels : \n  - name : valid_empty_kernel\n    execution_env : \n      simd_size  : 32\n      grf_count : 128\nversion:\'") + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + "\'\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"some_entry\" in global scope of .ze_info\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoContainsUnknownAttributeAndNoKernelsSectionThenEmitsError) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + "\'\na:b\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"a\" in global scope of .ze_info\n",
                 errors.c_str());
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoDoesNotContainKernelsSectionThenSuccessReturned) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto zeInfo = std::string("version:\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + "\'\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error) << errors.c_str();
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoContainsMultipleKernelSectionsThenFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + "\'\nkernels : \n  - name : valid_empty_kernel\n    execution_env : \n      simd_size  : 32\n      grf_count : 128\n" + "\nkernels : \n  - name : valid_empty_kernel\n    execution_env : \n      simd_size  : 32\n      grf_count : 128\n...\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo : Expected at most 1 of kernels, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoContainsMultipleVersionSectionsThenFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto brokenZeInfo = std::string("version:\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + "\'\nversion:\'5.4\'\nkernels:\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo : Expected at most 1 of version, got : 2\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoDoesNotContainVersionSectionsThenEmitsWarnings) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto zeInfo = ConstStringRef("kernels:\n");
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    auto version = NEO::Zebin::ZeInfo::zeInfoDecoderVersion;
    auto expectedWarning = "DeviceBinaryFormat::zebin::.ze_info : No version info provided (i.e. no version entry in global scope of DeviceBinaryFormat::zebin::.ze_info) - will use decoder's default : '" +
                           std::to_string(version.major) + "." + std::to_string(version.minor) + "'\n";

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoVersionIsInvalidThenFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto zeInfo = ConstStringRef("version:\'1a\'\nkernels:\n");
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : 1a\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoMinorVersionIsNewerThenEmitsWarning) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    auto version = NEO::Zebin::ZeInfo::zeInfoDecoderVersion;
    std::string expectedWarning = "DeviceBinaryFormat::zebin::.ze_info : Minor version : " + std::to_string(version.minor + 1) + " is newer than available in decoder : " + std::to_string(version.minor) + "\n";
    version.minor += 1;
    auto zeInfo = std::string("version:\'") + versionToString(version) + "\'\nkernels:\n";
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_STREQ(expectedWarning.c_str(), warnings.c_str());
    EXPECT_TRUE(errors.empty()) << errors;
}

TEST(DecodeSingleDeviceBinaryZebin, WhenZeInfoMajorVersionIsMismatchedThenFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    {
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
        auto version = NEO::Zebin::ZeInfo::zeInfoDecoderVersion;
        version.major += 1;
        auto zeInfo = std::string("version:\'") + versionToString(version) + "\'\nkernels:\n";
        zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string errors;
        std::string warnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
        EXPECT_EQ(NEO::DecodeError::unhandledBinary, error);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unhandled major version : 2, decoder is at : 1\n", errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }

    {
        ZebinTestData::ValidEmptyProgram zebin;
        zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
        auto version = NEO::Zebin::ZeInfo::zeInfoDecoderVersion;
        version.major -= 1;
        auto zeInfo = std::string("version:\'") + versionToString(version) + "\'\nkernels:\n";
        zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string errors;
        std::string warnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
        EXPECT_EQ(NEO::DecodeError::unhandledBinary, error);
        EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unhandled major version : 0, decoder is at : 1\n", errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(DecodeSingleDeviceBinaryZebin, WhenDecodeZeInfoFailsThenDecodingFails) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string brokenZeInfo = "version : \'" + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    -
)===";
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);

    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(brokenZeInfo.data(), brokenZeInfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of name, got : 0\nDeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of execution_env, got : 0\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoThenPopulatesKernelDescriptorProperly) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
    - name : some_other_kernel
      execution_env :
        simd_size : 32
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_other_kernel", {kernelIsa, sizeof(kernelIsa)});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(2U, programInfo.kernelInfos.size());
    EXPECT_STREQ("some_kernel", programInfo.kernelInfos[0]->kernelDescriptor.kernelMetadata.kernelName.c_str());
    EXPECT_STREQ("some_other_kernel", programInfo.kernelInfos[1]->kernelDescriptor.kernelMetadata.kernelName.c_str());
    EXPECT_EQ(8, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(32, programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(DeviceBinaryFormat::zebin, programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.binaryFormat);
    EXPECT_EQ(DeviceBinaryFormat::zebin, programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.binaryFormat);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoAndExternalFunctionsMetadataThenPopulatesExternalFunctionMetadataProperly) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
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
        has_rtcalls: true
        has_printf_calls: true
        has_indirect_calls: true
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(1U, programInfo.externalFunctions.size());
    auto &funInfo = programInfo.externalFunctions[0];
    EXPECT_STREQ("fun1", funInfo.functionName.c_str());
    EXPECT_EQ(128U, funInfo.numGrfRequired);
    EXPECT_EQ(8U, funInfo.simdSize);
    EXPECT_EQ(1U, funInfo.barrierCount);
    EXPECT_EQ(true, funInfo.hasRTCalls);
    EXPECT_EQ(true, funInfo.hasIndirectCalls);
    EXPECT_EQ(true, funInfo.hasPrintfCalls);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoWithEmptyKernelsAndExternalFunctionsMetadataThenPopulatesExternalFunctionMetadataProperly) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:        []
functions:
    - name: fun1
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 1
        has_rtcalls: true
)===";

    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(1U, programInfo.externalFunctions.size());
    auto &funInfo = programInfo.externalFunctions[0];
    EXPECT_STREQ("fun1", funInfo.functionName.c_str());
    EXPECT_EQ(128U, funInfo.numGrfRequired);
    EXPECT_EQ(8U, funInfo.simdSize);
    EXPECT_EQ(1U, funInfo.barrierCount);
    EXPECT_EQ(true, funInfo.hasRTCalls);
}

TEST(DecodeSingleDeviceBinaryZebin, GivenValidZeInfoAndInvalidExternalFunctionsMetadataThenFail) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string validZeInfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
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
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    const std::string expectedError = "DeviceBinaryFormat::zebin::.ze_info : could not read grf_count from : [abc] in context of : external functions\nDeviceBinaryFormat::zebin::.ze_info : could not read simd_size from : [defgas] in context of : external functions\n";
    EXPECT_STREQ(expectedError.c_str(), errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, GivenZeInfoWithTwoExternalFunctionsEntriesThenFail) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string validZeInfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
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
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(validZeInfo.data(), validZeInfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    const std::string expectedError = "DeviceBinaryFormat::zebin::ZeInfo : Expected at most 1 of functions, got : 2\n";
    EXPECT_STREQ(expectedError.c_str(), errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(DecodeSingleDeviceBinaryZebin, givenZeInfoWithKernelsMiscInfoSectionWhenDecodingBinaryThenDoNotParseThisSection) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
  - name:            some_kernel
    execution_env:
      simd_size:       32
    payload_arguments:
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
kernels_misc_info:
  // DO NOT Parse
  // ANYTHING IN THIS SECTION
  // OTHERWISE, YOU WILL GET PARSING ERROR
...
)===";
    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = {zebin.storage.data(), zebin.storage.size()};
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_NE(std::string::npos, programInfo.kernelMiscInfoPos);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenMinimalExecutionEnvThenPopulateKernelDescriptorWithDefaults) {
    ConstStringRef zeinfo = R"===(
    kernels:
        - name : some_kernel
          execution_env:
            simd_size: 32
            grf_count: 128
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    namespace Defaults = NEO::Zebin::ZeInfo::Types::Kernel::ExecutionEnv::Defaults;
    const auto &kernelDescriptor = *this->kernelDescriptor;
    EXPECT_EQ(kernelDescriptor.entryPoints.skipPerThreadDataLoad, static_cast<NEO::InstructionsSegmentOffset>(Defaults::offsetToSkipPerThreadDataLoad));
    EXPECT_EQ(kernelDescriptor.entryPoints.skipSetFFIDGP, static_cast<NEO::InstructionsSegmentOffset>(Defaults::offsetToSkipSetFfidGp));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.passInlineData, (Defaults::inlineDataPayloadSize != 0));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption, Defaults::disableMidThreadPreemption);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress, Defaults::subgroupIndependentForwardProgress);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.useStackCalls, Defaults::hasStackCalls);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.hasRTCalls, Defaults::hasRTCalls);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages, Defaults::hasFenceForImageAccess);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode, Defaults::hasDpas);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.hasSample, Defaults::hasSample);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.usesStatelessWrites, (false == Defaults::hasNoStatelessWrite));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.hasIndirectCalls, Defaults::hasIndirectCalls);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.flags.hasPrintfCalls, Defaults::hasPrintfCalls);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.barrierCount, static_cast<uint8_t>(Defaults::barrierCount));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.binaryFormat, DeviceBinaryFormat::zebin);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.bufferAddressingMode, (Defaults::has4GBBuffers) ? KernelDescriptor::Stateless : KernelDescriptor::BindfulAndStateless);
    EXPECT_EQ(kernelDescriptor.kernelAttributes.inlineDataPayloadSize, static_cast<uint16_t>(Defaults::inlineDataPayloadSize));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[0]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[1]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2], static_cast<uint16_t>(Defaults::requiredWorkGroupSize[2]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.slmInlineSize, static_cast<uint32_t>(Defaults::slmSize));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[0], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[0]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[1], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[1]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.workgroupWalkOrder[2], static_cast<uint8_t>(Defaults::workgroupWalkOrderDimensions[2]));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.privateScratchMemorySize, static_cast<uint32_t>(Defaults::privateSize));
    EXPECT_EQ(kernelDescriptor.kernelAttributes.spillFillScratchMemorySize, static_cast<uint32_t>(Defaults::spillSize));
    EXPECT_EQ(kernelDescriptor.kernelMetadata.requiredSubGroupSize, static_cast<uint8_t>(Defaults::requiredSubGroupSize));
}

TEST_F(decodeZeInfoKernelEntryTest, WhenValidationOfZeinfoSectionsCountFailsThenDecodingFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of name, got : 0\nDeviceBinaryFormat::zebin::ZeInfo::Kernel : Expected exactly 1 of execution_env, got : 0\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidExecutionEnvironmentThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env :
        simd_size : true
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read simd_size from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidPerThreadPayloadArgYamlEntriesThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        grf_count: 128
        simd_size: 32
      per_thread_payload_arguments:
        - arg_type:        local_id
          offset:          aaa
          size:            8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read offset from : [aaa] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidPayloadArgYamlEntriesThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        grf_count: 128
        simd_size: 32
      payload_arguments:
        - arg_type:        global_id_offset
          offset:          aaa
          size:            12
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read offset from : [aaa] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidPerThreadMemoryBufferYamlEntriesThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        grf_count: 128
        simd_size: 32
      per_thread_memory_buffers:
        - type:        scratch
          usage:       spill_fill_space
          size:        eight
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read size from : [eight] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidSimdSizeThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid simd size : 7 in context of : some_kernel. Expected 1, 8, 16 or 32. Got : 7\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidSimdSizeThenSetsItCorrectly) {
    uint32_t validSimdSizes[] = {1, 8, 16, 32};
    for (auto simdSize : validSimdSizes) {
        std::string zeinfo = R"===(
    kernels:
        - name : some_kernel
          execution_env:
            simd_size: )===" +
                             std::to_string(simdSize) + "\n";
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_EQ(simdSize, kernelDescriptor->kernelAttributes.simdSize);
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidPerThreadArgThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_payload_arguments:
        - arg_type:        local_size
          offset:          0
          size:            8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid arg type in per-thread data section in context of : some_kernel.\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidThreadSchedulingModesThenPopulateCorrectly) {
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::ExecutionEnv::ThreadSchedulingMode;

    std::pair<ConstStringRef, ThreadArbitrationPolicy> threadSchedulingModes[3] = {
        {ageBased, ThreadArbitrationPolicy::AgeBased},
        {roundRobin, ThreadArbitrationPolicy::RoundRobin},
        {roundRobinStall, ThreadArbitrationPolicy::RoundRobinAfterDependency}};
    for (auto &[str, val] : threadSchedulingModes) {
        std::string zeinfo = R"===(
        kernels:
            - name: some_kernel
              execution_env:
                simd_size: 32
                thread_scheduling_mode: )===" +
                             str.str() +
                             R"===(
        )===";
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_TRUE(errors.empty()) << errors;

        EXPECT_EQ(val, kernelDescriptor->kernelAttributes.threadArbitrationPolicy);
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidLocalIdThenAlignUpChannelSizeToGrfSize) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 16
      per_thread_payload_arguments:
        - arg_type:        local_id
          offset:          0
          size:            192
)===";
    grfSize = 64U;
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(3U, kernelDescriptor->kernelAttributes.numLocalIdChannels);
    EXPECT_EQ(192U, kernelDescriptor->kernelAttributes.perThreadDataSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidPerThreadArgThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      per_thread_payload_arguments:
        - arg_type:        local_id
          offset:          0
          size:            192
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(3U, kernelDescriptor->kernelAttributes.numLocalIdChannels);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidPayloadArgThenFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        local_id
          offset:          0
          size:            12
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid arg type in cross thread data section in context of : some_kernel.\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenZebinAppendElwsThenInjectsElwsArg) {
    DebugManagerStateRestore dbgRestore;
    NEO::debugManager.flags.ZebinAppendElws.set(true);

    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        local_size
          offset:          16
          size:            12
)===";

    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(64, kernelDescriptor->kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(16, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[0]);
    EXPECT_EQ(20, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[1]);
    EXPECT_EQ(24, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[2]);
    EXPECT_EQ(32, kernelDescriptor->payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0]);
    EXPECT_EQ(36, kernelDescriptor->payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1]);
    EXPECT_EQ(40, kernelDescriptor->payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2]);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidBindingTableYamlEntriesThenFails) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : could not read bti_value from : [true] in context of : some_kernel\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidBindingTableEntriesThenGeneratesSsh) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    ASSERT_EQ(2U, kernelDescriptor->payloadMappings.explicitArgs.size());
    EXPECT_EQ(128, kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescPointer>().bindful);
    EXPECT_EQ(448, kernelDescriptor->payloadMappings.explicitArgs[1].as<ArgDescImage>().bindful);
    EXPECT_EQ(8U, kernelDescriptor->payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(512U, kernelDescriptor->payloadMappings.bindingTable.tableOffset);
    ASSERT_EQ(576U, kernelDescriptor->generatedSsh.size());
    EXPECT_EQ(128U, reinterpret_cast<const uint32_t *>(ptrOffset(kernelDescriptor->generatedSsh.data(), 512U))[2]);
    EXPECT_EQ(448U, reinterpret_cast<const uint32_t *>(ptrOffset(kernelDescriptor->generatedSsh.data(), 512U))[7]);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBtiEntryForWrongArgTypeThenFail) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid binding table entry for non-pointer and non-image argument idx : 0.\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidArgSamplerThenGeneratesDsh) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    const auto &kernelDescriptor = *this->kernelDescriptor;
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesSamplers);

    const auto &samplerTable = kernelDescriptor.payloadMappings.samplerTable;
    EXPECT_EQ(1U, samplerTable.numSamplers);
    EXPECT_EQ(0U, samplerTable.borderColor);
    EXPECT_EQ(64U, samplerTable.tableOffset);

    const auto &args = kernelDescriptor.payloadMappings.explicitArgs;
    ASSERT_EQ(1U, args.size());
    EXPECT_EQ(64U, args[0].as<ArgDescSampler>().bindful);

    ASSERT_EQ(128U, kernelDescriptor.generatedDsh.size());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWhenTypeIsGlobalAndUsageIsNotPrivateThenFails) {
    {
        ConstStringRef zeinfo = R"===(
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid per-thread memory buffer memory usage type for global allocation type in context of : some_kernel. Expected : private_space.\n", errors.c_str());
    }

    {
        ConstStringRef zeinfo = R"===(
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_TRUE(warnings.empty()) << warnings;
        EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid per-thread memory buffer memory usage type for global allocation type in context of : some_kernel. Expected : private_space.\n", errors.c_str());
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWhenTypeIsSlmThenFails) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid per-thread memory buffer allocation type in context of : some_kernel.\n", errors.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWhenTypeIsGlobalAndUsageIsPrivateThenSetsProperFieldsInDescriptor) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_memory_buffers:
          - type:            global
            usage:           private_space
            size:            256
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(256U, kernelDescriptor->kernelAttributes.perHwThreadPrivateMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, givenPerSimtThreadBufferWhenPopulatingThenCalculatesCorrectSize) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(256U * 8, kernelDescriptor->kernelAttributes.perHwThreadPrivateMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferOfSizeBiggerThanMinimalWhenTypeIsScratchAndSimtThreadThenSetsProperFieldsInDescriptor) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        spill_size: 2048
      per_thread_memory_buffers:
          - type:            scratch
            usage:           spill_fill_space
            size:            2048
            is_simt_thread:  true
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.spillFillScratchMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferBiggerThanMinimalWhenSlotAndSimtThreadIsProvidedThenSetsProperFieldsInDescriptorInCorrectSlot) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        private_size: 2048
      per_thread_memory_buffers:
          - type:            scratch
            usage:           private_space
            size:            2048
            is_simt_thread:  true
            slot : 1
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(0U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.privateScratchMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryForSpillAndPrivateDefinedInSeparateFieldsThenProperFieldsInDescriptorAreSet) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        private_size: 256
        spill_size: 512
      per_thread_memory_buffers:
          - type:            scratch
            usage:           single_space
            size:            1024
          - type:            scratch
            usage:           single_space
            size:            2048
            slot:            1
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(1024U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(256U, kernelDescriptor->kernelAttributes.privateScratchMemorySize);
    EXPECT_EQ(512U, kernelDescriptor->kernelAttributes.spillFillScratchMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryForSpillAndPrivateDefinedInOlderZeInfoThenFallbackToLegacySlotMeaning) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        private_size: 256
        spill_size: 512
      per_thread_memory_buffers:
          - type:            scratch
            usage:           single_space
            size:            1024
          - type:            scratch
            usage:           single_space
            size:            2048
            slot:            1
)===";
    auto err = decodeZeInfoKernelEntryForSpecificVersion(zeinfo, {1, 38});
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(1024U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.privateScratchMemorySize);
    EXPECT_EQ(1024U, kernelDescriptor->kernelAttributes.spillFillScratchMemorySize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferOfSizeBiggerThanMinimalWhenTypeIsScratchThenSetsProperFieldsInDescriptor) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_memory_buffers:
          - type:            scratch
            usage:           private_space
            size:            1540
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(2048U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(0U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWhenSlotIsProvidedThenSetsProperFieldsInDescriptorInCorrectSlot) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_memory_buffers:
          - type:            scratch
            usage:           private_space
            size:            1024
            slot : 1
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(0U, kernelDescriptor->kernelAttributes.perThreadScratchSize[0]);
    EXPECT_EQ(1024U, kernelDescriptor->kernelAttributes.perThreadScratchSize[1]);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWhenSlotIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid scratch buffer slot 2 in context of : some_kernel. Expected 0 or 1.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWithInvalidSizeThenErrorIsReturned) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_memory_buffers:
          - type:            scratch
            usage:           private_space
            size:            0
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid per-thread memory buffer allocation size (size must be greater than 0) in context of : some_kernel.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPerThreadMemoryBufferWithMultipleScratchEntriesForTheSameSlotThenFails) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid duplicated scratch buffer entry 0 in context of : some_kernel.\n", errors.c_str());
}

TEST(DecodeZebinTest, GivenKernelWithoutCorrespondingTextSectionThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
)===";
    NEO::ProgramInfo programInfo;
    std::string errors, warnings;
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    auto err = decodeZebin(programInfo, elf, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Could not find text section for kernel some_kernel\n", errors.c_str());
}

TEST(DecodeZebinTest, givenGtpinInfoSectionsWhenDecodingZebinThenProperlySetIgcInfoForGtpinForEachCorrespondingKernel) {
    std::string errors, warnings;
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[0x10]{0u};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someOtherKernel", ArrayRef<const uint8_t>::fromAny(data, 0x10));

    std::array<uint8_t, 16> mockGtpinData1{};
    mockGtpinData1.fill(7u);
    std::array<uint8_t, 16> mockGtpinData2{};
    mockGtpinData2.fill(16u);
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_GTPIN_INFO, NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + zebin.kernelName, ArrayRef<const uint8_t>::fromAny(mockGtpinData1.data(), mockGtpinData1.size()));
    zebin.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_GTPIN_INFO, NEO::Zebin::Elf::SectionNames::gtpinInfo.str() + "someOtherKernel", ArrayRef<const uint8_t>::fromAny(mockGtpinData2.data(), mockGtpinData2.size()));

    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::ProgramInfo programInfo;
    programInfo.kernelInfos.reserve(2);
    auto kernelInfo1 = new KernelInfo();
    kernelInfo1->kernelDescriptor.kernelMetadata.kernelName = zebin.kernelName;
    programInfo.kernelInfos.push_back(kernelInfo1);
    auto kernelInfo2 = new KernelInfo();
    kernelInfo2->kernelDescriptor.kernelMetadata.kernelName = "someOtherKernel";
    programInfo.kernelInfos.push_back(kernelInfo2);

    auto err = decodeZebin(programInfo, elf, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_EQ(0, memcmp(reinterpret_cast<const uint8_t *>(kernelInfo1->igcInfoForGtpin), mockGtpinData1.data(), mockGtpinData1.size()));
    EXPECT_EQ(0, memcmp(reinterpret_cast<const uint8_t *>(kernelInfo2->igcInfoForGtpin), mockGtpinData2.data(), mockGtpinData2.size()));
}

TEST(DecodeZebinTest, givenTextSectionThenTreatItAsExternalFunctions) {
    std::string errors, warnings;
    ZebinTestData::ValidEmptyProgram zebin;
    const uint8_t data[0x10]{0u};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "someOtherKernel", ArrayRef<const uint8_t>::fromAny(data, 0x10));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::text, ArrayRef<const uint8_t>::fromAny(data, 0x10));

    auto elf = NEO::Elf::decodeElf(zebin.storage, errors, warnings);
    ASSERT_NE(nullptr, elf.elfFileHeader) << errors << " " << warnings;

    NEO::ProgramInfo programInfo;
    auto err = decodeZebin(programInfo, elf, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, err);
    ASSERT_EQ(2U, programInfo.kernelInfos.size());
    EXPECT_STREQ(NEO::Zebin::Elf::SectionNames::externalFunctions.data(), programInfo.kernelInfos[1]->kernelDescriptor.kernelMetadata.kernelName.c_str());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidExecutionEnvironmentThenPopulateKernelDescriptorProperly) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
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
        simd_size : 32
        slm_size : 1024
        subgroup_independent_forward_progress : true
        eu_thread_count : 8
        required_work_group_size: [8, 2, 1]
        work_group_walk_order_dimensions: [0, 1, 2]
        indirect_stateless_count : 2
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    auto &kernelDescriptor = *this->kernelDescriptor;
    EXPECT_EQ(7U, kernelDescriptor.kernelAttributes.barrierCount);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.usesBarriers());
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption);
    EXPECT_EQ(13U, kernelDescriptor.kernelAttributes.numGrfRequired);
    EXPECT_EQ(KernelDescriptor::Stateless, kernelDescriptor.kernelAttributes.bufferAddressingMode);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages);
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.usesStatelessWrites);
    EXPECT_EQ(32, kernelDescriptor.kernelAttributes.inlineDataPayloadSize);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.passInlineData);
    EXPECT_EQ(23U, kernelDescriptor.entryPoints.skipPerThreadDataLoad);
    EXPECT_EQ(29U, kernelDescriptor.entryPoints.skipSetFFIDGP);
    EXPECT_EQ(32U, kernelDescriptor.kernelAttributes.simdSize);
    EXPECT_EQ(1024U, kernelDescriptor.kernelAttributes.slmInlineSize);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress);
    EXPECT_EQ(8U, kernelDescriptor.kernelAttributes.numThreadsRequired);
    EXPECT_EQ(8U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2]);
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[0]);
    EXPECT_EQ(1U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[1]);
    EXPECT_EQ(2U, kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);
}

TEST_F(decodeZeInfoKernelEntryTest, givenPipeKernelArgumentWhenPopulatingKernelDescriptorThenProperTypeQualifierIsSet) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          40
          size:            8
          arg_index:       0
          addrmode:        stateless
          addrspace:       global
          access_type:     readwrite
          is_pipe:         true
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    const auto &argTraits = kernelDescriptor->payloadMappings.explicitArgs[0].getTraits();
    KernelArgMetadata::TypeQualifiers expectedQual = {};
    expectedQual.pipeQual = true;
    EXPECT_EQ(expectedQual.packed, argTraits.typeQualifiers.packed);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeLocalIdWhenOffsetIsNonZeroThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_payload_arguments:
        - arg_type:        local_id
          offset:          4
          size:            192
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid offset for argument of type local_id in context of : some_kernel. Expected 0.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeLocalIdWhenSizeIsInvalidThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      per_thread_payload_arguments:
        - arg_type:        local_id
          offset:          0
          size:            7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type local_id in context of : some_kernel. For simd=8 expected : 32 or 64 or 96. Got : 7 \n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeLocalIdWhenSizeIsValidThenCalculateNumChannelAndSetEmitLocalIdAccordingly) {
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
            auto err = decodeZeInfoKernelEntry(zeinfo);
            EXPECT_EQ(NEO::DecodeError::success, err) << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(errors.empty()) << errors << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(warnings.empty()) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(numChannels, kernelDescriptor->kernelAttributes.numLocalIdChannels) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(simdSize, kernelDescriptor->kernelAttributes.simdSize) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;

            const auto &emitLocalId = kernelDescriptor->kernelAttributes.localId;
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 0), emitLocalId[0]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 1), emitLocalId[1]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 2), emitLocalId[2]);
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypePackedLocalIdWhenOffsetIsNonZeroThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 1
      per_thread_payload_arguments:
        - arg_type:        packed_local_ids
          offset:          4
          size:            6
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Unhandled offset for argument of type packed_local_ids in context of : some_kernel. Expected 0.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypePackedLocalIdWhenSizeIsInvalidThenFail) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 1
      per_thread_payload_arguments:
        - arg_type:        packed_local_ids
          offset:          0
          size:            1
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type packed_local_ids in context of : some_kernel. Expected : 2 or 4 or 6. Got : 1 \n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypePackedLocalIdWhenSizeIsValidThenCalculateNumChannelAndSetEmitLocalIdAccordingly) {
    uint8_t simdSizes[] = {1};
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
            auto err = decodeZeInfoKernelEntry(zeinfo);
            EXPECT_EQ(NEO::DecodeError::success, err) << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(errors.empty()) << errors << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_TRUE(warnings.empty()) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(numChannels, kernelDescriptor->kernelAttributes.numLocalIdChannels) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;
            EXPECT_EQ(simdSize, kernelDescriptor->kernelAttributes.simdSize) << warnings << "simd : " << simdSize << ", num channels : " << numChannels;

            const auto &emitLocalId = kernelDescriptor->kernelAttributes.localId;
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 0), emitLocalId[0]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 1), emitLocalId[1]);
            EXPECT_EQ(static_cast<uint8_t>(numChannels > 2), emitLocalId[2]);
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBufferPointerArgWhenAddressSpaceIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AddressSpace = NEO::KernelArgMetadata::AddressSpaceQualifier;
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::PayloadArgument::AddrSpace;
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());
        EXPECT_TRUE(kernelDescriptor->payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::argTPointer>());
        EXPECT_EQ(addressSpace.second, kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().getAddressQualifier());
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPointerArgWhenAddressSpaceIsImageThenPopulatesArgDescriptorAccordingly) {
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::PayloadArgument::AddrSpace;

    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());
    EXPECT_TRUE(kernelDescriptor->payloadMappings.explicitArgs[0].is<NEO::ArgDescriptor::argTImage>());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPointerArgWhenAccessQualifierIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AccessQualifier = NEO::KernelArgMetadata::AccessQualifier;
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::PayloadArgument::AccessType;
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());
        EXPECT_EQ(accessQualifier.second, kernelDescriptor->payloadMappings.explicitArgs[0].getTraits().getAccessQualifier()) << accessQualifier.first.str();
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenNonPointerArgWhenAddressSpaceIsStatelessThenFails) {
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::PayloadArgument::AddrSpace;
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
        EXPECT_STREQ("Invalid or missing memory addressing stateless for arg idx : 0 in context of : some_kernel.\n", errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPointerArgWhenMemoryAddressingModeIsUknownThenFail) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("Invalid or missing memory addressing mode for arg idx : 0 in context of : some_kernel.\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenPointerArgWhenMemoryAddressingModeIsKnownThenPopulatesArgDescriptorAccordingly) {
    using AddressingMode = NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::MemoryAddressingMode;
    using namespace NEO::Zebin::ZeInfo::Tags::Kernel::PayloadArgument::MemoryAddressingMode;
    std::pair<NEO::ConstStringRef, AddressingMode> addressingModes[] = {{stateful, AddressingMode::memoryAddressingModeStateful},
                                                                        {stateless, AddressingMode::memoryAddressingModeStateless},
                                                                        {bindless, AddressingMode::memoryAddressingModeBindless},
                                                                        {sharedLocalMemory, AddressingMode::memoryAddressingModeSharedLocalMemory}};

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
        bool statefulOrBindlessAddressing = (AddressingMode::memoryAddressingModeStateful == addressingMode.second) || (AddressingMode::memoryAddressingModeBindless == addressingMode.second);
        if (statefulOrBindlessAddressing) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(expectedArgsCount, kernelDescriptor->payloadMappings.explicitArgs.size());
        auto &argAsPointer = kernelDescriptor->payloadMappings.explicitArgs[0].as<NEO::ArgDescPointer>();
        switch (addressingMode.second) {
        default:
            EXPECT_EQ(AddressingMode::memoryAddressingModeStateful, addressingMode.second);
            EXPECT_FALSE(argAsPointer.accessedUsingStatelessAddressingMode);
            break;
        case AddressingMode::memoryAddressingModeStateless:
            EXPECT_EQ(16, argAsPointer.stateless);
            EXPECT_EQ(8, argAsPointer.pointerSize);
            break;
        case AddressingMode::memoryAddressingModeBindless:
            EXPECT_EQ(16, argAsPointer.bindless);
            break;
        case AddressingMode::memoryAddressingModeSharedLocalMemory:
            EXPECT_EQ(16, argAsPointer.slmOffset);
            EXPECT_EQ(16, argAsPointer.requiredSlmAlignment);
            break;
        }

        if (statefulOrBindlessAddressing) {
            auto &argAsImage = kernelDescriptor->payloadMappings.explicitArgs[1].as<NEO::ArgDescImage>();
            auto &argAsSampler = kernelDescriptor->payloadMappings.explicitArgs[2].as<NEO::ArgDescSampler>();
            switch (addressingMode.second) {
            default:
                ASSERT_FALSE(true);
                break;
            case AddressingMode::memoryAddressingModeStateful:
                break;
            case AddressingMode::memoryAddressingModeBindless:
                EXPECT_EQ(24U, argAsImage.bindless);
                EXPECT_EQ(8U, argAsImage.size);
                EXPECT_EQ(32U, argAsSampler.bindless);
                break;
            }
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeLocalSizeWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : local_size
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type local_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeLocalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.localWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGlobaIdOffsetWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : global_id_offset
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type global_id_offset in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypePrivateBaseStatelessWhenArgSizeValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : private_base_stateless
                  offset : 16
                  size : 8
    )===";

    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
    ASSERT_EQ(16U, kernelDescriptor->payloadMappings.implicitArgs.privateMemoryAddress.stateless);
    ASSERT_EQ(8U, kernelDescriptor->payloadMappings.implicitArgs.privateMemoryAddress.pointerSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGlobaIdOffsetWhenArgSizeValidThenPopulatesKernelDescriptor) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.globalWorkOffset[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGroupCountWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : group_count
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type group_count in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGroupCountWhenArgSizeValidThenPopulatesKernelDescriptor) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeEnqueuedLocalSizeWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : enqueued_local_size
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type enqueued_local_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeByPointerWithSlmAlignmentSetThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
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
    auto res = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, res);
    ASSERT_EQ(1u, kernelDescriptor->payloadMappings.explicitArgs.size());
    const auto &arg = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(8, arg.requiredSlmAlignment);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeEnqueuedLocalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.enqueuedLocalWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGlobalSizeWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : global_size
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type global_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, givenRegionArgTypesWhenArgSizeIsInvalidThenFails) {
    ConstStringRef zeInfoRegionGroupSize = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_size
                  offset : 16
                  size : 7
)===";
    auto err = decodeZeInfoKernelEntry(zeInfoRegionGroupSize);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type region_group_size in context of : some_kernel. Expected 4 or 8 or 12. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    ConstStringRef zeInfoRegionGroupDim = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_dimension
                  offset : 16
                  size : 7
)===";
    err = decodeZeInfoKernelEntry(zeInfoRegionGroupDim);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type region_group_dimension in context of : some_kernel. Expected 4. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;

    ConstStringRef zeInfoRegionGroupCount = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_wg_count
                  offset : 16
                  size : 7
)===";
    err = decodeZeInfoKernelEntry(zeInfoRegionGroupCount);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_STREQ("DeviceBinaryFormat::zebin : Invalid size for argument of type region_group_wg_count in context of : some_kernel. Expected 4. Got : 7\n", errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST_F(decodeZeInfoKernelEntryTest, givenRegionArgTypesWhenArgSizeIsCorrectThenReturnSuccess) {
    ConstStringRef zeInfoRegionGroupSize = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_size
                  offset : 16
                  size : 12
)===";
    auto err = decodeZeInfoKernelEntry(zeInfoRegionGroupSize);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    for (uint32_t i = 0; i < 3; ++i) {
        EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.regionGroupSize[i]);
    }
    EXPECT_FALSE(kernelDescriptor->kernelAttributes.flags.usesRegionGroupBarrier);

    ConstStringRef zeInfoRegionGroupDim = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_dimension
                  offset : 16
                  size : 4
)===";
    err = decodeZeInfoKernelEntry(zeInfoRegionGroupDim);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(16, kernelDescriptor->payloadMappings.dispatchTraits.regionGroupDimension);
    EXPECT_FALSE(kernelDescriptor->kernelAttributes.flags.usesRegionGroupBarrier);

    ConstStringRef zeInfoRegionGroupCount = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_wg_count
                  offset : 16
                  size : 4
)===";
    err = decodeZeInfoKernelEntry(zeInfoRegionGroupCount);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(16, kernelDescriptor->payloadMappings.dispatchTraits.regionGroupWgCount);
    EXPECT_FALSE(kernelDescriptor->kernelAttributes.flags.usesRegionGroupBarrier);

    ConstStringRef zeInfoRegionGroupBarrier = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type : region_group_barrier_buffer
                  offset : 16
                  size : 8
)===";
    err = decodeZeInfoKernelEntry(zeInfoRegionGroupBarrier);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    EXPECT_EQ(16, kernelDescriptor->payloadMappings.implicitArgs.regionGroupBarrierBuffer.stateless);
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.usesRegionGroupBarrier);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeGlobalSizeWhenArgSizeValidThenPopulatesKernelDescriptor) {
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
        auto err = decodeZeInfoKernelEntry(zeinfo);
        EXPECT_EQ(NEO::DecodeError::success, err);
        EXPECT_TRUE(errors.empty()) << errors;
        EXPECT_TRUE(warnings.empty()) << warnings;
        ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
        for (uint32_t i = 0; i < vectorSize / sizeof(uint32_t); ++i) {
            EXPECT_EQ(16 + sizeof(uint32_t) * i, kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[i])
                << " vectorSize : " << vectorSize << ", idx : " << i;
        }
    }
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeBufferOffsetWhenOffsetAndSizeValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
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
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());
    const auto &arg = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(8, arg.bufferOffset);
}

TEST_F(decodeZeInfoKernelEntryTest, givenPureStatefulArgWithBufferAddressWhenThereIsNoStatelessAccessThenPopulatesKernelDescriptorAndArgIsPureStateful) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: arg_bypointer
                  offset: 0
                  size: 0
                  arg_index: 0
                  addrmode: stateful
                  addrspace: global
                  access_type: readwrite
                - arg_type: buffer_address
                  offset: 32
                  size: 8
                  arg_index: 0
              binding_table_indices:
                - bti_value: 0
                  arg_index: 0
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());

    const auto &arg = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(32, arg.stateless);
    EXPECT_EQ(8, arg.pointerSize);
    EXPECT_FALSE(arg.accessedUsingStatelessAddressingMode);
    EXPECT_TRUE(arg.isPureStateful());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenNoArgsThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32

)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(0U, kernelDescriptor->payloadMappings.explicitArgs.size());
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeBufferOffsetWhenSizeIsInvalidThenPopulateArgDescriptorFails) {
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT bufferOffsetArg;
    bufferOffsetArg.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeBufferOffset;
    bufferOffsetArg.offset = 8;
    bufferOffsetArg.argIndex = 0;

    for (auto size : {1, 2, 8}) {
        bufferOffsetArg.size = size;
        std::string errors, warnings;
        auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, bufferOffsetArg, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, retVal);
        auto expectedError = "DeviceBinaryFormat::zebin : Invalid size for argument of type buffer_offset in context of : some_kernel. Expected 4. Got : " + std::to_string(size) + "\n";
        EXPECT_STREQ(expectedError.c_str(), errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(PopulateArgDescriptor, GivenValueArgWithPointerMemberThenItIsProperlyPopulated) {
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT valueArg;
    valueArg.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeArgByvalue;
    valueArg.argIndex = 0;
    valueArg.offset = 8;
    valueArg.size = 8;
    valueArg.isPtr = true;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, valueArg, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescValue>().elements[0].isPtr);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeWorkDimensionsWhenSizeIsValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: work_dimensions
                  offset: 32
                  size: 4
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(32U, kernelDescriptor->payloadMappings.dispatchTraits.workDim);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeBufferSizeWhenSizeIsValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: buffer_size
                  offset: 32
                  size: 8
                  arg_index: 0
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    ASSERT_EQ(1U, kernelDescriptor->payloadMappings.explicitArgs.size());
    const auto &arg = kernelDescriptor->payloadMappings.explicitArgs[0].as<ArgDescPointer>();
    EXPECT_EQ(32, arg.bufferSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeImplicitArgBufferWhenPopulatingKernelDescriptorThenProperOffsetIsSetAndImplicitArgsAreRequired) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      payload_arguments:
        - arg_type:        implicit_arg_buffer
          offset:          4
          size:            8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_EQ(4u, kernelDescriptor->payloadMappings.implicitArgs.implicitArgsBuffer);
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenExecEnvRequireImplicitArgBufferTrueWhenPopulatingKernelDescriptorThenImplicitArgsAreRequired) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
        require_iab: true
      payload_arguments:
        - arg_type: work_dimensions
            offset: 32
            size: 4
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor->payloadMappings.implicitArgs.implicitArgsBuffer));
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenExecEnvRequireImplicitArgBufferFalseWhenPopulatingKernelDescriptorThenImplicitArgsAreNotRequired) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
        require_iab: false
      payload_arguments:
        - arg_type: work_dimensions
            offset: 32
            size: 4
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor->payloadMappings.implicitArgs.implicitArgsBuffer));
    EXPECT_FALSE(kernelDescriptor->kernelAttributes.flags.requiresImplicitArgs);
}

TEST(PopulateArgDescriptorCrossthreadPayload, GivenArgTypeWorkDimensionsWhenSizeIsInvalidThenPopulateKernelDescriptorFails) {
    NEO::KernelDescriptor kernelDescriptor;
    kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT workDimensionsArg;
    workDimensionsArg.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeWorkDimensions;
    workDimensionsArg.offset = 0x20;

    for (auto size : {1, 2, 8}) {
        workDimensionsArg.size = size;
        std::string errors, warnings;
        auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, workDimensionsArg, errors, warnings);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, retVal);
        auto expectedError = "DeviceBinaryFormat::zebin : Invalid size for argument of type work_dimensions in context of : some_kernel. Expected 4. Got : " + std::to_string(size) + "\n";
        EXPECT_STREQ(expectedError.c_str(), errors.c_str());
        EXPECT_TRUE(warnings.empty()) << warnings;
    }
}

TEST(PopulateArgDescriptor, GivenValidArgOfTypeRTGlobalBufferThenRtGlobalBufferIsPopulatedCorrectly) {
    NEO::KernelDescriptor kernelDescriptor;
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT rtGlobalBufferArg;
    rtGlobalBufferArg.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeRtGlobalBuffer;
    rtGlobalBufferArg.size = 8;
    rtGlobalBufferArg.offset = 32;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, rtGlobalBufferArg, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(40U, kernelDescriptor.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(8U, kernelDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.pointerSize);
    EXPECT_EQ(32U, kernelDescriptor.payloadMappings.implicitArgs.rtDispatchGlobals.stateless);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.hasRTCalls);
}

TEST(PopulateArgDescriptor, GivenValidConstDataBufferArgThenItIsPopulatedCorrectly) {
    NEO::KernelDescriptor kernelDescriptor;
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT dataConstBuffer;
    dataConstBuffer.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataConstBuffer;
    dataConstBuffer.size = 8;
    dataConstBuffer.offset = 32;
    dataConstBuffer.btiValue = 1;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, dataConstBuffer, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(40U, kernelDescriptor.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(8U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize);
    EXPECT_EQ(32U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(64U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful);
    EXPECT_EQ(2U, kernelDescriptor.payloadMappings.bindingTable.numEntries);
}

TEST(PopulateArgDescriptor, GivenValidGlobalDataBufferArgThenItIsPopulatedCorrectly) {
    NEO::KernelDescriptor kernelDescriptor;
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT dataGlobalBuffer;
    dataGlobalBuffer.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataGlobalBuffer;
    dataGlobalBuffer.btiValue = 1;
    dataGlobalBuffer.isPtr = true;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, dataGlobalBuffer, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(0U, kernelDescriptor.kernelAttributes.crossThreadDataSize);
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless));
    EXPECT_EQ(64U, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful);
    EXPECT_EQ(2U, kernelDescriptor.payloadMappings.bindingTable.numEntries);
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg);
}

TEST(PopulateArgDescriptor, GivenGlobalDataBufferArgWithoutBTIThenItIsPopulatedCorrectly) {
    NEO::KernelDescriptor kernelDescriptor;
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT dataGlobalBuffer;
    dataGlobalBuffer.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataGlobalBuffer;
    dataGlobalBuffer.btiValue = -1;
    dataGlobalBuffer.size = 8;
    dataGlobalBuffer.offset = 32;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, dataGlobalBuffer, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(8U, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize);
    EXPECT_EQ(32U, kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.bindingTable.numEntries);
}

TEST(PopulateArgDescriptor, GivenConstDataBufferArgWithoutBTIThenItIsPopulatedCorrectly) {
    NEO::KernelDescriptor kernelDescriptor;
    NEO::Zebin::ZeInfo::Types::Kernel::PayloadArgument::PayloadArgumentBaseT dataGlobalBuffer;
    dataGlobalBuffer.argType = NEO::Zebin::ZeInfo::Types::Kernel::argTypeDataConstBuffer;
    dataGlobalBuffer.btiValue = -1;
    dataGlobalBuffer.size = 8;
    dataGlobalBuffer.offset = 32;

    std::string errors, warnings;
    auto retVal = NEO::Zebin::ZeInfo::populateKernelPayloadArgument(kernelDescriptor, dataGlobalBuffer, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::success, retVal);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(8U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize);
    EXPECT_EQ(32U, kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_TRUE(NEO::isUndefinedOffset(kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful));
    EXPECT_EQ(0U, kernelDescriptor.payloadMappings.bindingTable.numEntries);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenIndirectDataPointerZeInfoWhenDecodeZeInfoThenIndirectDataPointerIsPopulatedCorrectly) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: indirect_data_pointer
                  offset: 16
                  size: 8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto indirectDataPointerAddress = kernelDescriptor->payloadMappings.implicitArgs.indirectDataPointerAddress;
    EXPECT_EQ(16u, indirectDataPointerAddress.offset);
    EXPECT_EQ(8u, indirectDataPointerAddress.pointerSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenScratchPointerZeInfoWhenDecodeZeInfoThenScratchPointerIsPopulatedCorrectly) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: scratch_pointer
                  offset: 24
                  size: 8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto scratchPointerAddress = kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress;
    EXPECT_EQ(24u, scratchPointerAddress.offset);
    EXPECT_EQ(8u, scratchPointerAddress.pointerSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypePrintfBufferWhenOffsetAndSizeIsValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: printf_buffer
                  offset: 32
                  size: 8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto printfSurfaceAddress = kernelDescriptor->payloadMappings.implicitArgs.printfSurfaceAddress;
    ASSERT_EQ(32U, printfSurfaceAddress.stateless);
    EXPECT_EQ(8U, printfSurfaceAddress.pointerSize);
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.usesPrintf);
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.hasPrintfCalls);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeAssertBufferWhenOffsetAndSizeIsValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: assert_buffer
                  offset: 32
                  size: 8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto assertBufferAddress = kernelDescriptor->payloadMappings.implicitArgs.assertBufferAddress;
    ASSERT_EQ(32U, assertBufferAddress.stateless);
    EXPECT_EQ(8U, assertBufferAddress.pointerSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenArgTypeSyncBufferWhenOffsetAndSizeIsValidThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type: sync_buffer
                  offset: 32
                  size: 8
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.usesSyncBuffer);
    const auto &syncBufferAddress = kernelDescriptor->payloadMappings.implicitArgs.syncBufferAddress;
    ASSERT_EQ(32U, syncBufferAddress.stateless);
    EXPECT_EQ(8U, syncBufferAddress.pointerSize);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidImageArgumentWithImageMetadataThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type:        arg_bypointer
                  offset:          0
                  size:            0
                  arg_index:       0
                  addrmode:        stateful
                  addrspace:       image
                  access_type:     readwrite
                  image_type:      image_2d_media
                  image_transformable: true
                - arg_type:        arg_bypointer
                  offset:          0
                  size:            0
                  arg_index:       1
                  addrmode:        stateful
                  addrspace:       image
                  access_type:     readwrite
                  image_type:      image_2d_media_block
                - arg_type:        image_height
                  offset:          0
                  size:            4
                  arg_index:       1
                - arg_type:        image_width
                  offset:          4
                  size:            4
                  arg_index:       1
                - arg_type:        image_depth
                  offset:          8
                  size:            4
                  arg_index:       1
                - arg_type:        image_channel_data_type
                  offset:          12
                  size:            4
                  arg_index:       1
                - arg_type:        image_channel_order
                  offset:          16
                  size:            4
                  arg_index:       1
                - arg_type:        image_array_size
                  offset:          20
                  size:            4
                  arg_index:       1
                - arg_type:        image_num_samples
                  offset:          24
                  size:            4
                  arg_index:       1
                - arg_type:        image_num_mip_levels
                  offset:          28
                  size:            4
                  arg_index:       1
                - arg_type:        flat_image_baseoffset
                  offset:          32
                  size:            8
                  arg_index:       1
                - arg_type:        flat_image_width
                  offset:          40
                  size:            4
                  arg_index:       1
                - arg_type:        flat_image_height
                  offset:          44
                  size:            4
                  arg_index:       1
                - arg_type:        flat_image_pitch
                  offset:          48
                  size:            4
                  arg_index:       1
              binding_table_indices:
                - bti_value:       1
                  arg_index:       0
                - bti_value:       2
                  arg_index:       1
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;
    auto &args = kernelDescriptor->payloadMappings.explicitArgs;

    EXPECT_EQ(64U, args[0].as<ArgDescImage>().bindful);
    EXPECT_TRUE(args[0].getExtendedTypeInfo().isTransformable);
    EXPECT_EQ(NEOImageType::imageType2DMedia, args[0].as<ArgDescImage>().imageType);

    EXPECT_EQ(128U, args[1].as<ArgDescImage>().bindful);
    EXPECT_TRUE(args[1].getExtendedTypeInfo().isMediaBlockImage);
    EXPECT_FALSE(args[1].getExtendedTypeInfo().isTransformable);
    EXPECT_EQ(NEOImageType::imageType2DMediaBlock, args[1].as<ArgDescImage>().imageType);
    const auto &imgMetadata = args[1].as<ArgDescImage>().metadataPayload;
    EXPECT_EQ(0U, imgMetadata.imgHeight);
    EXPECT_EQ(4U, imgMetadata.imgWidth);
    EXPECT_EQ(8U, imgMetadata.imgDepth);
    EXPECT_EQ(12U, imgMetadata.channelDataType);
    EXPECT_EQ(16U, imgMetadata.channelOrder);
    EXPECT_EQ(20U, imgMetadata.arraySize);
    EXPECT_EQ(24U, imgMetadata.numSamples);
    EXPECT_EQ(28U, imgMetadata.numMipLevels);
    EXPECT_EQ(32U, imgMetadata.flatBaseOffset);
    EXPECT_EQ(40U, imgMetadata.flatWidth);
    EXPECT_EQ(44U, imgMetadata.flatHeight);
    EXPECT_EQ(48U, imgMetadata.flatPitch);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidSamplerArgumentWithMetadataThenPopulatesKernelDescriptor) {
    ConstStringRef zeinfo = R"===(
        kernels:
            - name : some_kernel
              execution_env:
                simd_size: 32
              payload_arguments:
                - arg_type:        arg_bypointer
                  offset:          0
                  size:            0
                  arg_index:       0
                  addrmode:        stateful
                  addrspace:       sampler
                  access_type:     readwrite
                  sampler_index:   0
                - arg_type:        arg_bypointer
                  offset:          0
                  size:            0
                  arg_index:       1
                  addrmode:        stateful
                  addrspace:       sampler
                  access_type:     readwrite
                  sampler_index:   1
                  sampler_type:    vd
                - arg_type:        arg_bypointer
                  offset:          0
                  size:            0
                  arg_index:       2
                  addrmode:        stateful
                  addrspace:       sampler
                  access_type:     readwrite
                  sampler_index:   2
                  sampler_type:    ve
                - arg_type:        sampler_snap_wa
                  offset:          0
                  size:            4
                  arg_index:       2
                - arg_type:        sampler_normalized
                  offset:          4
                  size:            4
                  arg_index:       2
                - arg_type:        sampler_address
                  offset:          8
                  size:            4
                  arg_index:       2
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_TRUE(warnings.empty()) << warnings;

    const auto &kd = *this->kernelDescriptor;
    auto &args = kd.payloadMappings.explicitArgs;

    auto &sampler0 = args[0].as<ArgDescSampler>();
    EXPECT_EQ(64U, sampler0.bindful);
    EXPECT_EQ(0U, sampler0.index);
    EXPECT_EQ(undefined<uint8_t>, sampler0.size);

    auto &sampler1 = args[1].as<ArgDescSampler>();
    EXPECT_EQ(80U, sampler1.bindful);
    EXPECT_EQ(1U, sampler1.index);
    EXPECT_EQ(undefined<uint8_t>, sampler1.size);

    auto &sampler2 = args[2].as<ArgDescSampler>();
    EXPECT_EQ(96U, sampler2.bindful);
    EXPECT_EQ(0U, sampler2.metadataPayload.samplerSnapWa);
    EXPECT_EQ(4U, sampler2.metadataPayload.samplerNormalizedCoords);
    EXPECT_EQ(8U, sampler2.metadataPayload.samplerAddressingMode);
    EXPECT_EQ(2U, sampler2.index);
    EXPECT_EQ(undefined<uint8_t>, sampler2.size);

    EXPECT_TRUE(kd.kernelAttributes.flags.usesSamplers);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessSamplerArgumentWithMetadataThenKernelDescriptorIsPopulated) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          88
          size:            8
          arg_index:       0
          addrmode:        bindless
          addrspace:       sampler
          access_type:     readwrite
          sampler_index:   3
          sampler_type:    texture
        - arg_type:        arg_bypointer
          offset:          8
          size:            4
          arg_index:       1
          addrmode:        bindless
          addrspace:       global
          access_type:     readwrite
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(1u, kernelDescriptor->kernelAttributes.numArgsStateful);

    const auto &kd = *this->kernelDescriptor;
    auto &args = kd.payloadMappings.explicitArgs;

    auto &sampler0 = args[0].as<ArgDescSampler>();
    EXPECT_EQ(undefined<DynamicStateHeapOffset>, sampler0.bindful);
    EXPECT_EQ(88u, sampler0.bindless);
    EXPECT_EQ(3U, sampler0.index);
    EXPECT_EQ(8U, sampler0.size);
}

class IntelGTNotesFixture : public ::testing::Test {
  protected:
    void SetUp() override {
        zebin.elfHeader->type = NEO::Elf::ET_REL;
        zebin.elfHeader->machine = NEO::Elf::ElfMachine::EM_INTELGT;
    }

    void appendSingleIntelGTSectionData(const NEO::Elf::ElfNoteSection &elfNoteSection, uint8_t *const intelGTSectionData, const uint8_t *descData, const char *ownerName, size_t spaceAvailable, size_t &offset) {
        ASSERT_GE(spaceAvailable, sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize);
        memcpy_s(ptrOffset(intelGTSectionData, offset), sizeof(NEO::Elf::ElfNoteSection), &elfNoteSection, sizeof(NEO::Elf::ElfNoteSection));
        offset += sizeof(NEO::Elf::ElfNoteSection);
        memcpy_s(reinterpret_cast<char *>(ptrOffset(intelGTSectionData, offset)), elfNoteSection.nameSize, ownerName, elfNoteSection.nameSize);
        offset += elfNoteSection.nameSize;
        memcpy_s(ptrOffset(intelGTSectionData, offset), elfNoteSection.descSize, descData, elfNoteSection.descSize);
        offset += elfNoteSection.descSize;
    }

    void appendSingleIntelGTSectionData(const NEO::Elf::ElfNoteSection &elfNoteSection, uint8_t *const intelGTSectionData, const uint8_t *descData, const char *ownerName, size_t spaceAvailable) {
        size_t offset = 0u;
        appendSingleIntelGTSectionData(elfNoteSection, intelGTSectionData, descData, ownerName, spaceAvailable, offset);
    }

    void appendIntelGTSectionData(std::vector<NEO::Elf::ElfNoteSection> &elfNoteSections, uint8_t *const intelGTSectionData, std::vector<uint8_t *> &descData, size_t spaceAvailable) {
        size_t idx = 0u, offset = 0u;
        for (auto &elfNoteSection : elfNoteSections) {
            appendSingleIntelGTSectionData(elfNoteSection, intelGTSectionData, descData.at(idx), Zebin::Elf::intelGTNoteOwnerName.str().c_str(), spaceAvailable, offset);
            spaceAvailable -= (sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.descSize + elfNoteSection.nameSize);
            idx++;
        }
    }

    ZebinTestData::ValidEmptyProgram<> zebin;
};

TEST_F(IntelGTNotesFixture, GivenIabVersion2NoPrintfAttributeInZeInfoAndConstDataStringsSectionWhenDecodingBinaryThenKernelsWithIabRequiredHasFlagUsesPrintfSetToTrue) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:
    - name : some_kernel
      execution_env :
        simd_size : 8
        require_iab:     true
    - name : some_other_kernel
      execution_env :
        simd_size : 32
)===";

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_other_kernel", {kernelIsa, sizeof(kernelIsa)});

    const uint8_t data[] = {'H', 'e', 'l', 'l', 'o', '!'};
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConstString, data);

    const uint32_t indirectAccessBufferMajorVersion = 2u;

    {
        NEO::Elf::ElfNoteSection elfNoteSection;

        elfNoteSection.type = Zebin::Elf::IntelGTSectionType::indirectAccessBufferMajorVersion;
        elfNoteSection.descSize = sizeof(uint32_t);
        elfNoteSection.nameSize = 8u;

        std::vector<uint8_t *> descData;
        uint8_t indirectAccessBufferData[4];
        memcpy_s(indirectAccessBufferData, 4, &indirectAccessBufferMajorVersion, 4);
        descData.push_back(indirectAccessBufferData);

        const auto sectionDataSize = sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;

        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        memcpy_s(noteIntelGTSectionData.get(), sizeof(NEO::Elf::ElfNoteSection), &elfNoteSection, sizeof(NEO::Elf::ElfNoteSection));
        auto offset = sizeof(NEO::Elf::ElfNoteSection);
        memcpy_s(reinterpret_cast<char *>(ptrOffset(noteIntelGTSectionData.get(), offset)), elfNoteSection.nameSize, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), elfNoteSection.nameSize);
        offset += elfNoteSection.nameSize;
        memcpy_s(ptrOffset(noteIntelGTSectionData.get(), offset), elfNoteSection.descSize, descData[0], elfNoteSection.descSize);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));
    }

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    singleBinary.generatorFeatureVersions.indirectAccessBuffer = 2;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;

    ASSERT_EQ(2u, programInfo.kernelInfos.size());
    EXPECT_TRUE(programInfo.kernelInfos[0]->kernelDescriptor.kernelAttributes.flags.usesPrintf);
    EXPECT_FALSE(programInfo.kernelInfos[1]->kernelDescriptor.kernelAttributes.flags.usesPrintf);
}

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenValidIntelGTNotesSectionThenReturnsIntelGTNotes) {
    std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
    size_t numNotes = 5;

    for (size_t i = 0; i < numNotes; i++) {
        auto &inserted = elfNoteSections.emplace_back();
        inserted.nameSize = 8u;
        inserted.descSize = 4u;
    }
    elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
    elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::gfxCore;
    elfNoteSections.at(2).type = Zebin::Elf::IntelGTSectionType::targetMetadata;
    elfNoteSections.at(3).type = Zebin::Elf::IntelGTSectionType::zebinVersion;
    elfNoteSections.at(4).type = Zebin::Elf::IntelGTSectionType::vISAAbiVersion; // not handled by the runtime, but should be recognized

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = hardwareInfoTable[productFamily]->platform.usRevId - 1;
    targetMetadata.maxHwRevisionId = hardwareInfoTable[productFamily]->platform.usRevId + 1;
    std::vector<uint8_t *> descData;

    uint8_t platformData[4];
    memcpy_s(platformData, 4, &productFamily, 4);
    descData.push_back(platformData);

    uint8_t coreData[4];
    memcpy_s(coreData, 4, &renderCoreFamily, 4);
    descData.push_back(coreData);

    uint8_t metadataPackedData[4];
    memcpy_s(metadataPackedData, 4, &targetMetadata.packed, 4);
    descData.push_back(metadataPackedData);

    uint8_t mockData[4] = {0x0, 0x0, 0x0, 0x0}; // mock data for ZebinVersion and vISAAbiVersion notes
    descData.push_back(mockData);
    descData.push_back(mockData);
    const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNoteSection) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);

    appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    std::vector<Zebin::Elf::IntelGTNote> intelGTNotesRead = {};
    auto decodeError = NEO::Zebin::getIntelGTNotes(elf, intelGTNotesRead, outErrReason, outWarning);
    EXPECT_EQ(DecodeError::success, decodeError);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    EXPECT_EQ(numNotes, intelGTNotesRead.size());

    auto validNotes = true;
    for (size_t i = 0; i < intelGTNotesRead.size(); ++i) {
        validNotes &= (intelGTNotesRead.at(i).type == elfNoteSections.at(i).type);
        validNotes &= (intelGTNotesRead.at(i).data.size() == elfNoteSections.at(i).descSize);
        if (validNotes) {
            validNotes &= (0 == memcmp(intelGTNotesRead.at(i).data.begin(), descData[i], elfNoteSections.at(i).descSize));
        }
    }
    EXPECT_TRUE(validNotes);
}

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenInvalidIntelGTNotesSectionNameThenSectionsIsSkipped) {
    uint8_t mockSectionData[0x10];
    zebin.appendSection(NEO::Elf::SHT_NOTE, ".note.wrong.name", ArrayRef<uint8_t>::fromAny(mockSectionData, 0x10));

    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    std::vector<Zebin::Elf::IntelGTNote> intelGTNotesRead = {};
    auto decodeError = NEO::Zebin::getIntelGTNotes(elf, intelGTNotesRead, outErrReason, outWarning);
    EXPECT_EQ(DecodeError::success, decodeError);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    EXPECT_EQ(0U, intelGTNotesRead.size());
}

TEST_F(IntelGTNotesFixture, WhenGettingIntelGTNotesGivenInvalidOwnerNameInIntelGTNotesThenTheNoteIsSkipped) {
    {
        NEO::Elf::ElfNoteSection elfNoteSection = {};
        elfNoteSection.descSize = 4u;
        elfNoteSection.nameSize = 8u;
        uint8_t mockDescData[4u]{0};
        auto sectionDataSize = sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
        const char badOwnerString[8] = "badName";
        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), mockDescData, badOwnerString, sectionDataSize);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

        std::string outErrReason, outWarning;
        auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
        EXPECT_TRUE(outWarning.empty());
        EXPECT_TRUE(outErrReason.empty());

        std::vector<Zebin::Elf::IntelGTNote> intelGTNotesRead = {};
        auto decodeError = NEO::Zebin::getIntelGTNotes(elf, intelGTNotesRead, outErrReason, outWarning);
        EXPECT_EQ(DecodeError::success, decodeError);
        auto expectedWarning = "DeviceBinaryFormat::zebin : Invalid owner name : badName for IntelGTNote - note will not be used.\n";
        EXPECT_STREQ(expectedWarning, outWarning.c_str());
        EXPECT_TRUE(outErrReason.empty());
        EXPECT_EQ(0U, intelGTNotesRead.size());
        zebin.removeSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT);
    }
    {
        NEO::Elf::ElfNoteSection elfNoteSection = {};
        elfNoteSection.descSize = 4u;
        elfNoteSection.nameSize = 0u;
        uint8_t mockDescData[4u]{0};
        auto sectionDataSize = sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), mockDescData, nullptr, sectionDataSize);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

        std::string outErrReason, outWarning;
        auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
        EXPECT_TRUE(outWarning.empty());
        EXPECT_TRUE(outErrReason.empty());

        std::vector<Zebin::Elf::IntelGTNote> intelGTNotesRead = {};
        auto decodeError = NEO::Zebin::getIntelGTNotes(elf, intelGTNotesRead, outErrReason, outWarning);
        EXPECT_EQ(DecodeError::success, decodeError);
        auto expectedWarning = "DeviceBinaryFormat::zebin : Empty owner name.\n";
        EXPECT_STREQ(expectedWarning, outWarning.c_str());
        EXPECT_TRUE(outErrReason.empty());
        EXPECT_EQ(0U, intelGTNotesRead.size());
    }
}

TEST_F(IntelGTNotesFixture, GivenValidTargetDeviceAndNoteWithUnrecognizedTypeWhenValidatingTargetDeviceThenEmitWarning) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId;

    std::vector<NEO::Elf::ElfNoteSection> elfNotes = {};
    for (int i = 0; i < 3; i++) {
        auto &inserted = elfNotes.emplace_back();
        inserted.descSize = 4u;
        inserted.nameSize = 8u;
    }
    elfNotes.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
    elfNotes.at(1).type = Zebin::Elf::IntelGTSectionType::gfxCore;
    elfNotes.at(2).type = Zebin::Elf::IntelGTSectionType::lastSupported + 1; // unsupported
    std::vector<uint8_t *> descData;

    uint8_t platformDescData[4u];
    memcpy_s(platformDescData, 4u, &targetDevice.productFamily, 4u);
    descData.push_back(platformDescData);

    uint8_t coreDescData[4u];
    memcpy_s(coreDescData, 4u, &targetDevice.coreFamily, 4u);
    descData.push_back(coreDescData);

    uint8_t mockDescData[4]{0};
    descData.push_back(mockDescData);

    const auto sectionDataSize = std::accumulate(elfNotes.begin(), elfNotes.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNote) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNote.nameSize + elfNote.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);

    appendIntelGTSectionData(elfNotes, noteIntelGTSectionData.get(), descData, sectionDataSize);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));
    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    SingleDeviceBinary singleDeviceBinary{};
    auto validationRes = validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_TRUE(validationRes);
    EXPECT_TRUE(outErrReason.empty());

    auto expectedWarning = "DeviceBinaryFormat::zebin : Unrecognized IntelGTNote type: " + std::to_string(elfNotes.at(2).type) + "\n";
    EXPECT_STREQ(expectedWarning.c_str(), outWarning.c_str());
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndValidNotesThenReturnTrue) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId & 0x1F; // mask to 5 bits (minHwRevisionId and maxHwRevisionId size)

    std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
    for (int i = 0; i < 3; i++) {
        auto &inserted = elfNoteSections.emplace_back();
        inserted.descSize = 4u;
        inserted.nameSize = 8u;
    }

    elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
    elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::gfxCore;
    elfNoteSections.at(2).type = Zebin::Elf::IntelGTSectionType::targetMetadata;
    std::vector<uint8_t *> descData;

    uint8_t platformData[4];
    memcpy_s(platformData, 4, &targetDevice.productFamily, 4);
    descData.push_back(platformData);

    uint8_t coreData[4];
    memcpy_s(coreData, 4, &targetDevice.coreFamily, 4);
    descData.push_back(coreData);

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = (targetDevice.stepping == 0) ? targetDevice.stepping : targetDevice.stepping - 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 1;

    uint8_t metadataPackedData[4];
    memcpy_s(metadataPackedData, 4, &targetMetadata.packed, 4);
    descData.push_back(metadataPackedData);

    const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNoteSection) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    SingleDeviceBinary singleDeviceBinary{};
    EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, givenAotConfigInIntelGTNotesSectionWhenValidatingTargetDeviceThenUseOnlyItForValidation) {
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = 0x00001234;

    TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8u;
    ASSERT_EQ(IGFX_UNKNOWN, targetDevice.productFamily);
    ASSERT_EQ(IGFX_UNKNOWN_CORE, targetDevice.coreFamily);
    targetDevice.aotConfig.value = aotConfig.value;

    NEO::Elf::ElfNoteSection elfNoteSection;
    elfNoteSection.descSize = 4u;
    elfNoteSection.nameSize = 8u;
    elfNoteSection.type = Zebin::Elf::IntelGTSectionType::productConfig;

    uint8_t productConfigData[4];
    memcpy_s(productConfigData, 4, &aotConfig.value, 4);

    auto sectionDataSize = sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), productConfigData, NEO::Zebin::Elf::intelGTNoteOwnerName.data(), sectionDataSize);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, givenRequestedTargetDeviceWithApplyValidationWorkaroundFlagSetToTrueWhenValidatingDeviceBinaryThenDoNotUseProductConfigForValidation) {
    NEO::HardwareIpVersion aotConfig = {0};
    aotConfig.value = 0x00001234;

    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.aotConfig.value = aotConfig.value + 0x10; // ensure mismatch and validation error if AOT config is used
    targetDevice.applyValidationWorkaround = true;

    std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
    for (int i = 0; i < 2; i++) {
        auto &inserted = elfNoteSections.emplace_back();
        inserted.descSize = 4u;
        inserted.nameSize = 8u;
    }

    // Minimum passing configuration - required i.e. proper product family passed
    // Product config data should get ignored and not be used at all
    elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
    elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::productConfig;
    std::vector<uint8_t *> descData;

    uint8_t productFamilyData[4];
    memcpy_s(productFamilyData, 4, &targetDevice.productFamily, 4);
    descData.push_back(productFamilyData);

    uint8_t productConfigData[4];
    memcpy_s(productConfigData, 4, &targetDevice.aotConfig.value, 4);
    descData.push_back(productConfigData);

    const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNoteSection) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<NEO::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    SingleDeviceBinary singleDeviceBinary{};
    EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST(ValidateTargetDevice32BitZebin, Given32BitZebinAndValidIntelGTNotesWhenValidatingTargetDeviceThenReturnTrue) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 4;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId & 0x1F; // mask to 5 bits (minHwRevisionId and maxHwRevisionId size)

    ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin;
    zebin.elfHeader->type = NEO::Elf::ET_REL;
    zebin.elfHeader->machine = NEO::Elf::ElfMachine::EM_INTELGT;

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = targetDevice.stepping;
    targetMetadata.maxHwRevisionId = targetDevice.stepping;
    auto currentVersion = versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion);
    auto intelGTNotesSection = ZebinTestData::createIntelGTNoteSection(productFamily, renderCoreFamily, targetMetadata, currentVersion);
    zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, intelGTNotesSection);
    std::string outErrReason, outWarning;
    auto elf = NEO::Elf::decodeElf<Zebin::Elf::EI_CLASS_32>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST(ValidateTargetDeviceGeneratorZebin, GivenZebinAndValidIntelGTNotesWithGeneratorIdWhenValidatingTargetDeviceThenGeneratorIsSetCorrectly) {

    for (auto generatorId : {0u, 1u}) {
        TargetDevice targetDevice;
        targetDevice.productFamily = productFamily;
        targetDevice.coreFamily = renderCoreFamily;
        targetDevice.maxPointerSizeInBytes = 4;
        targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId & 0x1F; // mask to 5 bits (minHwRevisionId and maxHwRevisionId size)

        ZebinTestData::ValidEmptyProgram<NEO::Elf::EI_CLASS_32> zebin;
        zebin.elfHeader->type = NEO::Elf::ET_REL;
        zebin.elfHeader->machine = NEO::Elf::ElfMachine::EM_INTELGT;

        Zebin::Elf::ZebinTargetFlags targetMetadata;
        targetMetadata.validateRevisionId = true;
        targetMetadata.minHwRevisionId = targetDevice.stepping;
        targetMetadata.maxHwRevisionId = targetDevice.stepping;
        targetMetadata.generatorId = generatorId;
        auto currentVersion = versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion);
        auto intelGTNotesSection = ZebinTestData::createIntelGTNoteSection(productFamily, renderCoreFamily, targetMetadata, currentVersion);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, intelGTNotesSection);
        std::string outErrReason, outWarning;
        auto elf = NEO::Elf::decodeElf<Zebin::Elf::EI_CLASS_32>(zebin.storage, outErrReason, outWarning);
        EXPECT_TRUE(outWarning.empty());
        EXPECT_TRUE(outErrReason.empty());

        SingleDeviceBinary singleDeviceBinary{};
        EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));

        bool isIgcGeneratedExpectation = static_cast<bool>(generatorId);
        bool isIgcGenerated = static_cast<bool>(singleDeviceBinary.generator);
        EXPECT_EQ(isIgcGeneratedExpectation, isIgcGenerated);
    }
}

TEST_F(IntelGTNotesFixture, GivenForceCompatibilityModeWhenValidatingTargetDeviceWithNoNotesThenReturnTrue) {
    DebugManagerStateRestore dbgRestore;
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId;

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
    NEO::debugManager.flags.ForceCompatibilityMode.set(true);
    EXPECT_TRUE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndNoNotesThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId;

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenInvalidTargetDeviceAndValidNotesThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId;

    std::vector<Zebin::Elf::ElfNoteSection> elfNoteSections;
    for (int i = 0; i < 3; i++) {
        auto &inserted = elfNoteSections.emplace_back();
        inserted.descSize = 4u;
        inserted.nameSize = 8u;
    }

    elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
    elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::gfxCore;
    elfNoteSections.at(2).type = Zebin::Elf::IntelGTSectionType::targetMetadata;
    std::vector<uint8_t *> descData;

    uint8_t platformData[4];
    memcpy_s(platformData, 4, &productFamily, 4);
    descData.push_back(platformData);

    uint8_t coreData[4];
    memcpy_s(coreData, 4, &renderCoreFamily, 4);
    descData.push_back(coreData);

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.minHwRevisionId = targetDevice.stepping + 1;
    targetMetadata.maxHwRevisionId = targetDevice.stepping + 1;

    uint8_t metadataPackedData[4];
    memcpy_s(metadataPackedData, 4, &targetMetadata.packed, 4);
    descData.push_back(metadataPackedData);

    const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                 [](auto totalSize, const auto &elfNoteSection) {
                                                     return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                 });
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidTargetDeviceAndInvalidNoteTypeThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.productFamily = productFamily;
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.maxPointerSizeInBytes = 8;
    targetDevice.stepping = hardwareInfoTable[productFamily]->platform.usRevId;

    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = Zebin::Elf::IntelGTSectionType::lastSupported + 1;
    elfNoteSection.descSize = 0u;
    elfNoteSection.nameSize = 8u;
    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);

    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), nullptr, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenInvalidIntelGTNotesSecionSizeWhichWilLCauseOOBAccessThenReturnFalse) {
    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.descSize = 4u;
    elfNoteSection.nameSize = 8u;
    elfNoteSection.type = Zebin::Elf::IntelGTSectionType::productFamily;

    uint8_t platformData[4];
    memcpy_s(platformData, 4, &productFamily, 4);

    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
    auto incorrectSectionDataSize = sectionDataSize + 0x5; // add rubbish data at the end - may cause OOB access.
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(incorrectSectionDataSize);

    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), platformData, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), incorrectSectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), incorrectSectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    TargetDevice targetDevice;
    auto result = validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_FALSE(result);
    EXPECT_TRUE(outWarning.empty());
    auto errStr{"DeviceBinaryFormat::zebin : Offsetting will cause out-of-bound memory read! Section size: " + std::to_string(incorrectSectionDataSize) +
                ", current section data offset: " + std::to_string(sectionDataSize)};
    EXPECT_TRUE(std::string::npos != outErrReason.find(errStr));
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenValidZeInfoVersionInIntelGTNotesThenZeInfoVersionIsPopulatedCorrectly) {
    auto decoderVersion = std::to_string(Zebin::ZeInfo::zeInfoDecoderVersion.major) + "." + std::to_string(Zebin::ZeInfo::zeInfoDecoderVersion.minor);

    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = 4;
    elfNoteSection.descSize = static_cast<uint32_t>(decoderVersion.length() + 1);
    elfNoteSection.nameSize = 8u;

    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + alignUp(elfNoteSection.descSize, 4);
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), reinterpret_cast<const uint8_t *>(decoderVersion.c_str()),
                                   Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    TargetDevice targetDevice;
    validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_TRUE(outErrReason.empty());
}

TEST_F(IntelGTNotesFixture, WhenValidatingTargetDeviceGivenIndirectDetectionVersionInIntelGTNotesThenIndirectDetectionVersionIsPopulatedCorrectly) {
    const uint32_t indirectDetectionVersion = 1u;

    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = Zebin::Elf::IntelGTSectionType::indirectAccessDetectionVersion;
    elfNoteSection.descSize = sizeof(uint32_t);
    elfNoteSection.nameSize = 8u;

    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.nameSize + alignUp(elfNoteSection.descSize, 4);
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), reinterpret_cast<const uint8_t *>(&indirectDetectionVersion),
                                   Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    TargetDevice targetDevice;
    validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_TRUE(outErrReason.empty());
    EXPECT_EQ(indirectDetectionVersion, singleDeviceBinary.generatorFeatureVersions.indirectMemoryAccessDetection);

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::ProgramInfo programInfo;
    singleDeviceBinary.deviceBinary = {zebin.storage.data(), zebin.storage.size()};
    std::string errors;
    std::string warnings;
    zebin.elfHeader->machine = productFamily;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleDeviceBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_TRUE(errors.empty()) << errors;
    EXPECT_EQ(indirectDetectionVersion, programInfo.indirectDetectionVersion);
}

TEST_F(IntelGTNotesFixture, GivenIndirectAccessBufferVersionInIntelGTNotesWhenValidatingTargetDeviceThenVersionIsPopulatedCorrectly) {

    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    compilerProductHelper.adjustHwInfoForIgc(hwInfo);
    TargetDevice targetDevice = getTargetDevice(rootDeviceEnvironment);
    if (zebin.elfHeader->identity.eClass == NEO::Elf::EI_CLASS_64) {
        targetDevice.maxPointerSizeInBytes = 8;
    }
    const uint32_t indirectAccessBufferMajorVersion = 4u;

    {
        std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
        for (int i = 0; i < 2; i++) {
            auto &inserted = elfNoteSections.emplace_back();
            inserted.descSize = 4u;
            inserted.nameSize = 8u;
        }

        elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
        elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::productConfig;

        elfNoteSections.emplace_back();
        elfNoteSections.at(2).type = Zebin::Elf::IntelGTSectionType::indirectAccessBufferMajorVersion;
        elfNoteSections.at(2).descSize = sizeof(uint32_t);
        elfNoteSections.at(2).nameSize = 8u;

        std::vector<uint8_t *> descData;

        uint8_t productFamilyData[4];
        memcpy_s(productFamilyData, 4, &targetDevice.productFamily, 4);
        descData.push_back(productFamilyData);

        uint8_t productConfigData[4];
        memcpy_s(productConfigData, 4, &targetDevice.aotConfig.value, 4);
        descData.push_back(productConfigData);

        uint8_t indirectAccessBufferData[4];
        memcpy_s(indirectAccessBufferData, 4, &indirectAccessBufferMajorVersion, 4);
        descData.push_back(indirectAccessBufferData);

        const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                     [](auto totalSize, const auto &elfNoteSection) {
                                                         return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                     });
        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));
    }

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    SingleDeviceBinary singleDeviceBinary{};
    singleDeviceBinary.targetDevice = targetDevice;

    validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_EQ(indirectAccessBufferMajorVersion, singleDeviceBinary.generatorFeatureVersions.indirectAccessBuffer);

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::ProgramInfo programInfo;
    singleDeviceBinary.deviceBinary = {zebin.storage.data(), zebin.storage.size()};

    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleDeviceBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_EQ(indirectAccessBufferMajorVersion, programInfo.indirectAccessBufferMajorVersion);
}

TEST_F(IntelGTNotesFixture, GivenFailFromValidateTargetDeviceWhenDecodingBinaryThenInvalidBinaryReturend) {

    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    compilerProductHelper.adjustHwInfoForIgc(hwInfo);
    TargetDevice targetDevice = getTargetDevice(rootDeviceEnvironment);
    if (zebin.elfHeader->identity.eClass == NEO::Elf::EI_CLASS_64) {
        targetDevice.maxPointerSizeInBytes = 8;
    }

    {
        std::vector<NEO::Elf::ElfNoteSection> elfNoteSections;
        for (int i = 0; i < 2; i++) {
            auto &inserted = elfNoteSections.emplace_back();
            inserted.descSize = 4u;
            inserted.nameSize = 8u;
        }

        elfNoteSections.at(0).type = Zebin::Elf::IntelGTSectionType::productFamily;
        elfNoteSections.at(1).type = Zebin::Elf::IntelGTSectionType::productConfig;

        std::vector<uint8_t *> descData;

        uint8_t productFamilyData[4] = {0};
        descData.push_back(productFamilyData);

        uint8_t productConfigData[4] = {0};
        descData.push_back(productConfigData);

        const auto sectionDataSize = std::accumulate(elfNoteSections.begin(), elfNoteSections.end(), size_t{0u},
                                                     [](auto totalSize, const auto &elfNoteSection) {
                                                         return totalSize + sizeof(NEO::Elf::ElfNoteSection) + elfNoteSection.nameSize + elfNoteSection.descSize;
                                                     });
        auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
        appendIntelGTSectionData(elfNoteSections, noteIntelGTSectionData.get(), descData, sectionDataSize);
        zebin.appendSection(NEO::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));
    }

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    SingleDeviceBinary singleDeviceBinary{};
    singleDeviceBinary.targetDevice = targetDevice;

    EXPECT_FALSE(validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator));

    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    NEO::ProgramInfo programInfo;
    singleDeviceBinary.deviceBinary = {zebin.storage.data(), zebin.storage.size()};

    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleDeviceBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
}

TEST_F(IntelGTNotesFixture, GivenNotNullTerminatedVersioningStringWhenGettingIntelGTNotesThenEmitWarningAndDontUseIt) {
    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = 4;
    elfNoteSection.descSize = 4u;
    elfNoteSection.nameSize = 8u;

    uint8_t zeInfoVersionNotTerminated[4] = {0x31, 0x2e, 0x31, 0x35}; // version "1.15 not null terminated"
    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.descSize + elfNoteSection.nameSize;
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), zeInfoVersionNotTerminated, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());

    std::vector<Zebin::Elf::IntelGTNote> intelGTNotes = {};
    auto decodeError = NEO::Zebin::getIntelGTNotes(elf, intelGTNotes, outErrReason, outWarning);
    EXPECT_EQ(DecodeError::success, decodeError);
    EXPECT_EQ(0u, intelGTNotes.size());
    EXPECT_TRUE(outErrReason.empty());
    EXPECT_STREQ("DeviceBinaryFormat::zebin :  Versioning string is not null-terminated: 1.15 - note will not be used.\n", outWarning.c_str());
}

TEST_F(IntelGTNotesFixture, GivenInvalidVersioningWhenValidatingTargetDeviceThenReturnFalse) {
    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = 4;
    elfNoteSection.descSize = 4u;
    elfNoteSection.nameSize = 8u;

    uint8_t incorrectZeInfoVersion[4] = {0x2e, 0x31, 0x31, 0x0}; // version ".11\0"
    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.descSize + elfNoteSection.nameSize;
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), incorrectZeInfoVersion, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    TargetDevice targetDevice;
    validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Invalid version format - expected 'MAJOR.MINOR' string, got : .11\n", outErrReason.c_str());
}

TEST_F(IntelGTNotesFixture, GivenIncompatibleVersioningWhenValidatingTargetDeviceThenReturnFalse) {
    Zebin::Elf::ElfNoteSection elfNoteSection = {};
    elfNoteSection.type = 4;
    elfNoteSection.descSize = 4u;
    elfNoteSection.nameSize = 8u;

    uint8_t incompatibleZeInfo[4] = {0x32, 0x2e, 0x3f, 0x0}; // version "2.9\0"
    auto sectionDataSize = sizeof(Zebin::Elf::ElfNoteSection) + elfNoteSection.descSize + elfNoteSection.nameSize;
    auto noteIntelGTSectionData = std::make_unique<uint8_t[]>(sectionDataSize);
    appendSingleIntelGTSectionData(elfNoteSection, noteIntelGTSectionData.get(), incompatibleZeInfo, Zebin::Elf::intelGTNoteOwnerName.str().c_str(), sectionDataSize);
    zebin.appendSection(Zebin::Elf::SHT_NOTE, Zebin::Elf::SectionNames::noteIntelGT, ArrayRef<uint8_t>::fromAny(noteIntelGTSectionData.get(), sectionDataSize));

    std::string outErrReason, outWarning;
    auto elf = Zebin::Elf::decodeElf<Zebin::Elf::EI_CLASS_64>(zebin.storage, outErrReason, outWarning);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_TRUE(outErrReason.empty());
    SingleDeviceBinary singleDeviceBinary{};

    TargetDevice targetDevice;
    validateTargetDevice(elf, targetDevice, outErrReason, outWarning, singleDeviceBinary.generatorFeatureVersions, singleDeviceBinary.generator);
    EXPECT_TRUE(outWarning.empty());
    EXPECT_STREQ("DeviceBinaryFormat::zebin::.ze_info : Unhandled major version : 2, decoder is at : 1\n", outErrReason.c_str());
}

TEST(ValidateTargetDeviceTests, givenMismatechAotConfigWhenValidatingTargetDeviceThenUseOnlyItForValidationAndReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.aotConfig.value = 0x00001234;
    targetDevice.maxPointerSizeInBytes = 8u;

    auto mismatchedAotConfig = static_cast<AOT::PRODUCT_CONFIG>(0x00004321); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    Zebin::Elf::ZebinTargetFlags targetMetadata;
    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, productFamily, renderCoreFamily, mismatchedAotConfig, targetMetadata);
    EXPECT_FALSE(res);
}

TEST(ValidateTargetDeviceTests, givenMismatchedProductFamilyWhenValidatingTargetDeviceThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8u;
    ASSERT_EQ(AOT::UNKNOWN_ISA, targetDevice.aotConfig.value);
    targetDevice.coreFamily = IGFX_UNKNOWN_CORE;
    targetDevice.productFamily = productFamily;

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    auto mismatchedPlatformFamily = IGFX_UNKNOWN;
    for (int i = 0; i < IGFX_MAX_PRODUCT; i++) {
        const auto hwInfo = NEO::hardwareInfoTable[i];
        if (nullptr != hwInfo && hwInfo->platform.eRenderCoreFamily != renderCoreFamily) {
            mismatchedPlatformFamily = hwInfo->platform.eProductFamily;
        }
    }

    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, mismatchedPlatformFamily, IGFX_UNKNOWN_CORE, AOT::UNKNOWN_ISA, targetMetadata);
    EXPECT_FALSE(res);
}

TEST(ValidateTargetDeviceTests, givenMissingProductFamilyWhenValidatingTargetDeviceThenReturnTrue) {
    TargetDevice targetDevice{};
    targetDevice.maxPointerSizeInBytes = 8u;
    ASSERT_EQ(AOT::UNKNOWN_ISA, targetDevice.aotConfig.value);
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.productFamily = productFamily;

    Zebin::Elf::ZebinTargetFlags targetMetadata{};

    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, IGFX_UNKNOWN, targetDevice.coreFamily, AOT::UNKNOWN_ISA, targetMetadata);
    EXPECT_TRUE(res);
}

TEST(ValidateTargetDeviceTests, givenMissingGfxCoreFamilyWhenValidatingTargetDeviceThenReturnTrue) {
    TargetDevice targetDevice{};
    targetDevice.maxPointerSizeInBytes = 8u;
    ASSERT_EQ(AOT::UNKNOWN_ISA, targetDevice.aotConfig.value);
    targetDevice.coreFamily = renderCoreFamily;
    targetDevice.productFamily = productFamily;

    Zebin::Elf::ZebinTargetFlags targetMetadata{};

    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, targetDevice.productFamily, IGFX_UNKNOWN_CORE, AOT::UNKNOWN_ISA, targetMetadata);
    EXPECT_TRUE(res);
}

TEST(ValidateTargetDeviceTests, givenMismatchedGfxCoreWhenValidatingTargetDeviceThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8u;
    ASSERT_EQ(AOT::UNKNOWN_ISA, targetDevice.aotConfig.value);
    targetDevice.coreFamily = renderCoreFamily;

    Zebin::Elf::ZebinTargetFlags targetMetadata;
    auto mismatchedGfxCore = static_cast<GFXCORE_FAMILY>(renderCoreFamily + 1);

    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, IGFX_UNKNOWN, mismatchedGfxCore, AOT::UNKNOWN_ISA, targetMetadata);
    EXPECT_FALSE(res);
}

TEST(ValidateTargetDeviceTests, givenSteppingBiggerThanMaxHwRevisionWhenValidatingTargetDeviceThenReturnFalse) {
    TargetDevice targetDevice;
    targetDevice.maxPointerSizeInBytes = 8u;
    targetDevice.coreFamily = renderCoreFamily;

    targetDevice.stepping = 2u;
    Zebin::Elf::ZebinTargetFlags targetMetadata;
    targetMetadata.validateRevisionId = true;
    targetMetadata.maxHwRevisionId = 1u;

    auto res = validateTargetDevice(targetDevice, Zebin::Elf::EI_CLASS_64, IGFX_UNKNOWN, renderCoreFamily, AOT::UNKNOWN_ISA, targetMetadata);
    EXPECT_FALSE(res);
}

TEST(ValidateTargetDeviceTests, givenDeviceInCompatModeWhenValidatingTargetDeviceThenUseItOnlyForValidation) {
    for (auto &currentDevice : AOT::deviceAcronyms) {
        TargetDevice targetDevice;
        targetDevice.aotConfig.value = currentDevice.second;
        targetDevice.maxPointerSizeInBytes = 8u;

        Zebin::Elf::ZebinTargetFlags targetMetadata;

        for (auto &deviceToCompare : AOT::deviceAcronyms) {
            auto productConfigToCompare = deviceToCompare.second;

            auto res = validateTargetDevice(targetDevice,
                                            Zebin::Elf::EI_CLASS_64,
                                            productFamily,
                                            renderCoreFamily,
                                            productConfigToCompare,
                                            targetMetadata);

            auto prodConfCompatPairItr = AOT::getCompatibilityMapping().find(productConfigToCompare);

            if (targetDevice.aotConfig.value == productConfigToCompare) {
                EXPECT_TRUE(res);
            } else if (prodConfCompatPairItr != AOT::getCompatibilityMapping().end()) {
                auto prodConfVec = prodConfCompatPairItr->second;
                auto found = std::find(prodConfVec.begin(), prodConfVec.end(), currentDevice.second);
                if (found != prodConfVec.end()) {
                    EXPECT_TRUE(res);
                } else {
                    EXPECT_FALSE(res);
                }
            } else {
                EXPECT_FALSE(res);
            }
        }
    }
}

TEST(ValidateTargetDeviceTests, givenDeviceWithoutCompatModeWhenValidatingTargetDeviceThenUseItOnlyForValidation) {
    bool compatModeInitState = debugManager.flags.EnableCompatibilityMode.get();
    debugManager.flags.EnableCompatibilityMode.set(false);

    for (auto &currentDevice : AOT::deviceAcronyms) {
        TargetDevice targetDevice;
        targetDevice.aotConfig.value = currentDevice.second;
        targetDevice.maxPointerSizeInBytes = 8u;

        Zebin::Elf::ZebinTargetFlags targetMetadata;

        for (auto &deviceToCompare : AOT::deviceAcronyms) {
            auto productConfigToCompare = deviceToCompare.second;

            auto res = validateTargetDevice(targetDevice,
                                            Zebin::Elf::EI_CLASS_64,
                                            productFamily,
                                            renderCoreFamily,
                                            productConfigToCompare,
                                            targetMetadata);

            if (targetDevice.aotConfig.value == productConfigToCompare) {
                EXPECT_TRUE(res);
            } else {
                EXPECT_FALSE(res);
            }
        }
    }

    debugManager.flags.EnableCompatibilityMode.set(compatModeInitState);
}

TEST(PopulateGlobalDeviceHostNameMapping, givenValidZebinWithGlobalHostAccessTableSectionThenPopulateHostDeviceNameMapCorrectly) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

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

    uint8_t kernelIsa[8]{0U};
    ZebinTestData::ValidEmptyProgram zebin;
    zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
    zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeinfo.data(), zeinfo.size()));
    zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {kernelIsa, sizeof(kernelIsa)});

    NEO::ProgramInfo programInfo;
    NEO::SingleDeviceBinary singleBinary;
    singleBinary.deviceBinary = zebin.storage;
    std::string errors;
    std::string warnings;
    auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
    EXPECT_EQ(NEO::DecodeError::success, error);
    EXPECT_TRUE(errors.empty()) << errors;

    EXPECT_EQ(2u, programInfo.globalsDeviceToHostNameMap.size());
    EXPECT_STREQ("IntVarName", programInfo.globalsDeviceToHostNameMap["int_var"].c_str());
    EXPECT_STREQ("BoolVarName", programInfo.globalsDeviceToHostNameMap["bool_var"].c_str());
}

TEST(PopulateGlobalDeviceHostNameMapping, givenZebinWithGlobalHostAccessTableSectionAndInvalidValuesThenReturnInvalidBinaryError) {
    NEO::MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();

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
        zebin.removeSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo);
        zebin.appendSection(NEO::Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));
        zebin.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "some_kernel", {});

        NEO::ProgramInfo programInfo;
        NEO::SingleDeviceBinary singleBinary;
        singleBinary.deviceBinary = zebin.storage;
        std::string errors;
        std::string warnings;
        auto error = NEO::decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(programInfo, singleBinary, errors, warnings, gfxCoreHelper);
        EXPECT_EQ(NEO::DecodeError::invalidBinary, error);
    }
}

TEST(PopulateGlobalDeviceHostNameMapping, givenZebinWithGlobalHostAccessTableSectionAndUnrecognizableKeyThenEmitError) {
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
    NEO::Zebin::ZeInfo::ZeInfoGlobalHostAccessTables tables;
    auto err = NEO::Zebin::ZeInfo::readZeInfoGlobalHostAceessTable(parser, tableNode, tables, "global_host_access_table", errors, warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;

    std::string expectedError("DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"banana_type\" for payload argument in context of global_host_access_table\n");
    EXPECT_STREQ(expectedError.c_str(), errors.c_str());
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

        auto &functionsNode = *parser.findNodeWithKeyDfs(NEO::Zebin::ZeInfo::Tags::functions);
        std::string errors;
        std::string warnings;
        ProgramInfo programInfo;
        for (const auto &functionNd : parser.createChildrenRange(functionsNode)) {
            auto err = NEO::Zebin::ZeInfo::populateExternalFunctionsMetadata(programInfo, parser, functionNd, errors, warnings);
            EXPECT_EQ(DecodeError::success, err);
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

TEST(PopulateZeInfoExternalFunctionsMetadata, GivenValidExternalFunctionsMetadataWithUnknownEntriesThenEmitError) {
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

    auto &functionsNode = *parser.findNodeWithKeyDfs(NEO::Zebin::ZeInfo::Tags::functions);
    auto &functionNode = *parser.createChildrenRange(functionsNode).begin();
    std::string errors;
    std::string warnings;
    ProgramInfo programInfo;
    auto err = NEO::Zebin::ZeInfo::populateExternalFunctionsMetadata(programInfo, parser, functionNode, errors, warnings);
    EXPECT_EQ(DecodeError::unkownZeinfoAttribute, err);
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(warnings.empty()) << warnings;
    const auto expectedError = "DeviceBinaryFormat::zebin::.ze_info : Unknown entry \"unknown\" in context of : external functions\n";
    EXPECT_STREQ(expectedError, errors.c_str());
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

    auto &functionsNode = *parser.findNodeWithKeyDfs(NEO::Zebin::ZeInfo::Tags::functions);
    auto &functionNode = *parser.createChildrenRange(functionsNode).begin();
    std::string errors;
    std::string warnings;
    ProgramInfo programInfo;
    auto err = NEO::Zebin::ZeInfo::populateExternalFunctionsMetadata(programInfo, parser, functionNode, errors, warnings);
    EXPECT_EQ(DecodeError::invalidBinary, err);
    EXPECT_EQ(0U, programInfo.externalFunctions.size());
    const auto expectedError = "DeviceBinaryFormat::zebin::.ze_info : could not read grf_count from : [abc] in context of : external functions\nDeviceBinaryFormat::zebin::.ze_info : could not read simd_size from : [def] in context of : external functions\nDeviceBinaryFormat::zebin::.ze_info : could not read barrier_count from : [ghi] in context of : external functions\n";
    EXPECT_STREQ(expectedError, errors.c_str());
    EXPECT_TRUE(warnings.empty()) << warnings;
}

TEST(ParsingEmptyOptionalVectorTypesZeInfo, givenEmptyOptionalVectorDataEntryInZeInfoWithSamePreviousIndentationWhenParsingZeInfoThenFalsIsReturnedAndErrorPrinted) {
    NEO::Yaml::YamlParser parser;
    bool parseResult = true;
    std::string errors;
    std::string warnings;

    NEO::ConstStringRef zeInfoMissingEntrySamePrevInd = R"===(---
kernels:
  - name:            some_kernel
    execution_env:
      simd_size: 8
    per_thread_payload_arguments:
    binding_table_indices:
      - bti_value:       0
        arg_index:       0
...
)===";
    parseResult = parser.parse(zeInfoMissingEntrySamePrevInd, errors, warnings);
    ASSERT_FALSE(parseResult);
    EXPECT_STREQ("NEO::Yaml : Could not parse line : [5] : [per_thread_payload_arguments:] <-- parser position on error. Reason : Vector data type expects to have at least one value starting with -\n", errors.c_str());
}

TEST(ParsingEmptyOptionalVectorTypesZeInfo, givenEmptyKernelsEntryInZeInfoWhenParsingZeInfoThenZeroKernelsAreParsed) {
    NEO::Yaml::YamlParser parser;
    bool parseResult = true;
    std::string errors;
    std::string warnings;

    std::string zeinfo = std::string("version :\'") + versionToString(Zebin::ZeInfo::zeInfoDecoderVersion) + R"===('
kernels:         []
functions:
  - name:            test_function
    execution_env:
      grf_count:       16
      simd_size:       32
kernels_misc_info:
  - name:            ''
...
)===";
    parseResult = parser.parse(zeinfo, errors, warnings);
    ASSERT_TRUE(parseResult);
    EXPECT_EQ(0u, errors.size());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenValidInlineSamplersThenPopulateKernelDescriptorSucceeds) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      inline_samplers:
        - sampler_index:             0
          addrmode:     clamp_edge
          filtermode:   nearest
          normalized:   true
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty());

    ASSERT_EQ(1U, kernelDescriptor->inlineSamplers.size());
    const auto &inlineSampler = kernelDescriptor->inlineSamplers[0];
    EXPECT_EQ(0U, inlineSampler.samplerIndex);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::AddrMode::clampEdge, inlineSampler.addrMode);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::FilterMode::nearest, inlineSampler.filterMode);
    EXPECT_TRUE(inlineSampler.isNormalized);

    EXPECT_EQ(undefined<CrossThreadDataOffset>, inlineSampler.bindless);
    EXPECT_EQ(undefined<uint8_t>, inlineSampler.size);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenInvalidInlineSamplersEntryThenPopulateKernelDescriptorFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      inline_samplers:
        - sampler_index:  -1
          addrmode:       gibberish
          filtermode:     trash
          normalized:     dead_beef
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_FALSE(errors.empty());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenMissingMemberInInlineSamplersThenPopulateKernelDescriptorFails) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      inline_samplers:
        -  addrmode:     clamp_edge
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenBindlessInlineSamplersThenPopulateKernelDescriptorSetsBindlessOffsetAndSize) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        has_sample:      true
      payload_arguments:
        - arg_type:        inline_sampler
          offset:          40
          size:            4
          addrmode:        bindless
          addrspace:       sampler
          sampler_index:   0
        - arg_type:        inline_sampler
          offset:          44
          size:            4
          addrmode:        bindless
          addrspace:       sampler
          sampler_index:   1
      inline_samplers:
        - sampler_index:   1
          addrmode:        none
          filtermode:      nearest
        - sampler_index:   0
          addrmode:        repeat
          filtermode:      linear
          normalized:      true
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);
    EXPECT_TRUE(errors.empty());

    ASSERT_EQ(2U, kernelDescriptor->inlineSamplers.size());

    const auto &inlineSampler1 = kernelDescriptor->inlineSamplers[0];
    EXPECT_EQ(1U, inlineSampler1.samplerIndex);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::AddrMode::none, inlineSampler1.addrMode);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::FilterMode::nearest, inlineSampler1.filterMode);
    EXPECT_FALSE(inlineSampler1.isNormalized);
    EXPECT_EQ(44u, inlineSampler1.bindless);
    EXPECT_EQ(4u, inlineSampler1.size);

    const auto &inlineSampler0 = kernelDescriptor->inlineSamplers[1];
    EXPECT_EQ(0U, inlineSampler0.samplerIndex);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::AddrMode::repeat, inlineSampler0.addrMode);
    EXPECT_EQ(NEO::KernelDescriptor::InlineSampler::FilterMode::linear, inlineSampler0.filterMode);
    EXPECT_TRUE(inlineSampler0.isNormalized);
    EXPECT_EQ(40u, inlineSampler0.bindless);
    EXPECT_EQ(4u, inlineSampler0.size);
}

TEST_F(decodeZeInfoKernelEntryTest, givenGlobalBufferAndConstBufferWhenPopulatingKernelDescriptorThenPopulateThemProperly) {
    NEO::ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
      payload_arguments:
        - arg_type:        global_base
          offset:          0
          size:            8
          bti_value:       0
        - arg_type:        const_base
          offset:          8
          size:            8
          bti_value:       1
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_EQ(0U, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.stateless);
    EXPECT_EQ(8U, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.pointerSize);
    EXPECT_EQ(0U, kernelDescriptor->payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful);

    EXPECT_EQ(8U, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.stateless);
    EXPECT_EQ(8U, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.pointerSize);
    EXPECT_EQ(64U, kernelDescriptor->payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful);

    EXPECT_EQ(2u, kernelDescriptor->payloadMappings.bindingTable.numEntries);
    EXPECT_EQ(128u, kernelDescriptor->payloadMappings.bindingTable.tableOffset);
    EXPECT_EQ(0U, *reinterpret_cast<const uint32_t *>(kernelDescriptor->generatedSsh.data() + 128U));
    EXPECT_EQ(64U, *reinterpret_cast<const uint32_t *>(kernelDescriptor->generatedSsh.data() + 132U));
    ASSERT_EQ(192u, kernelDescriptor->generatedSsh.size());
}

TEST_F(decodeZeInfoKernelEntryTest, GivenImageArgWithReadOnlyAccessWhenPopulatingKernelDescriptorThenHasImageWriteArgIsFalse) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          24
          size:            8
          arg_index:       0
          addrmode:        bindless
          addrspace:       image
          access_type:     readonly
          image_type:      image_2d
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_FALSE(kernelDescriptor->kernelAttributes.hasImageWriteArg);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenImageArgWithWriteOnlyAccessWhenPopulatingKernelDescriptorThenHasImageWriteArgIsTrue) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          32
          size:            8
          arg_index:       1
          addrmode:        bindless
          addrspace:       image
          access_type:     writeonly
          image_type:      image_2d
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_TRUE(kernelDescriptor->kernelAttributes.hasImageWriteArg);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenImageArgWithReadWriteAccessWhenPopulatingKernelDescriptorThenHasImageWriteArgIsTrue) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 32
      payload_arguments:
        - arg_type:        arg_bypointer
          offset:          24
          size:            8
          arg_index:       0
          addrmode:        bindless
          addrspace:       image
          access_type:     readwrite
          image_type:      image_2d
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_TRUE(kernelDescriptor->kernelAttributes.hasImageWriteArg);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenKernelWithIndirectCallsWhenPopulatingKernelDescriptorThenHasIndirectCallsIsSet) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        has_indirect_calls: true
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.hasIndirectCalls);
}

TEST_F(decodeZeInfoKernelEntryTest, GivenKernelWithPrintfCallsWhenPopulatingKernelDescriptorThenHasPrintfCallsIsSet) {
    ConstStringRef zeinfo = R"===(
kernels:
    - name : some_kernel
      execution_env:
        simd_size: 8
        has_printf_calls: true
...
)===";
    auto err = decodeZeInfoKernelEntry(zeinfo);
    EXPECT_EQ(NEO::DecodeError::success, err);

    EXPECT_TRUE(kernelDescriptor->kernelAttributes.flags.hasPrintfCalls);
}

TEST(PopulateInlineSamplers, GivenInvalidSamplerIndexThenPopulateInlineSamplersFails) {
    NEO::KernelDescriptor kd;
    std::string errors, warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::InlineSamplerBaseT inlineSamplerSrc;
    inlineSamplerSrc.samplerIndex = -1;
    inlineSamplerSrc.addrMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::AddrMode::none;
    inlineSamplerSrc.filterMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::FilterMode::nearest;
    auto err = NEO::Zebin::ZeInfo::populateKernelInlineSampler(kd, inlineSamplerSrc, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_FALSE(errors.empty());
}

TEST(GenerateDsh, given4SamplersWhenGenerateDSHIsCalledThenDshIsGeneratedCorrectly) {

    NEO::KernelDescriptor kd;
    auto numSamplers = 4u;
    kd.payloadMappings.samplerTable.numSamplers = numSamplers;
    uint32_t samplerBorderColorStateSize = 64u;
    uint32_t samplerStateSize = 16u;
    NEO::Zebin::ZeInfo::generateDSH(kd, samplerStateSize, samplerBorderColorStateSize);

    EXPECT_EQ(numSamplers, kd.payloadMappings.samplerTable.numSamplers);
    EXPECT_EQ(samplerBorderColorStateSize, kd.payloadMappings.samplerTable.tableOffset);
    EXPECT_EQ(0u, kd.payloadMappings.samplerTable.borderColor);

    size_t dshSize = samplerBorderColorStateSize + numSamplers * samplerStateSize;
    auto expectedSize = samplerBorderColorStateSize > 0 ? alignUp(dshSize, samplerBorderColorStateSize) : dshSize;
    EXPECT_EQ(expectedSize, kd.generatedDsh.size());
}

TEST(GenerateDsh, given4SamplersWithoutSamplerBorderColorStateWhenGenerateDSHIsCalledThenDshIsGeneratedCorrectly) {

    NEO::KernelDescriptor kd;
    auto numSamplers = 4u;
    kd.payloadMappings.samplerTable.numSamplers = numSamplers;
    uint32_t samplerBorderColorStateSize = 0u;
    uint32_t samplerStateSize = 16u;
    NEO::Zebin::ZeInfo::generateDSH(kd, samplerStateSize, samplerBorderColorStateSize);

    EXPECT_EQ(numSamplers, kd.payloadMappings.samplerTable.numSamplers);
    EXPECT_EQ(samplerBorderColorStateSize, kd.payloadMappings.samplerTable.tableOffset);
    EXPECT_EQ(0u, kd.payloadMappings.samplerTable.borderColor);

    size_t dshSize = samplerBorderColorStateSize + numSamplers * samplerStateSize;
    auto expectedSize = samplerBorderColorStateSize > 0 ? alignUp(dshSize, samplerBorderColorStateSize) : dshSize;
    EXPECT_EQ(expectedSize, kd.generatedDsh.size());
}

TEST(PopulateInlineSamplers, GivenInvalidAddrModeThenPopulateInlineSamplersFails) {
    NEO::KernelDescriptor kd;
    std::string errors, warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::InlineSamplerBaseT inlineSamplerSrc;
    inlineSamplerSrc.samplerIndex = 0;
    inlineSamplerSrc.addrMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::AddrMode::unknown;
    inlineSamplerSrc.filterMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::FilterMode::nearest;
    auto err = NEO::Zebin::ZeInfo::populateKernelInlineSampler(kd, inlineSamplerSrc, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_FALSE(errors.empty());
}

TEST(PopulateInlineSamplers, GivenInvalidFilterModeThenPopulateInlineSamplersFails) {
    NEO::KernelDescriptor kd;
    std::string errors, warnings;
    NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::InlineSamplerBaseT inlineSamplerSrc;
    inlineSamplerSrc.samplerIndex = 0;
    inlineSamplerSrc.addrMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::AddrMode::none;
    inlineSamplerSrc.filterMode = NEO::Zebin::ZeInfo::Types::Kernel::InlineSamplers::FilterMode::unknown;
    auto err = NEO::Zebin::ZeInfo::populateKernelInlineSampler(kd, inlineSamplerSrc, errors, warnings);
    EXPECT_EQ(NEO::DecodeError::invalidBinary, err);
    EXPECT_FALSE(errors.empty());
}

TEST(ReadZeInfoInlineSamplers, GivenUnknownEntryThenPrintError) {
    NEO::ConstStringRef yaml = R"===(---
kernels:
  - name:            some_kernel
    inline_samplers:
      -  sampler_index:   0
         new_entry:       3
...
)===";

    std::string parserErrors;
    std::string parserWarnings;
    NEO::Yaml::YamlParser parser;
    bool success = parser.parse(yaml, parserErrors, parserWarnings);
    ASSERT_TRUE(success);
    auto &inlineSamplersNode = *parser.findNodeWithKeyDfs("inline_samplers");
    std::string errors;
    std::string warnings;
    NEO::Zebin::ZeInfo::KernelInlineSamplers inlineSamplers;
    auto err = NEO::Zebin::ZeInfo::readZeInfoInlineSamplers(parser,
                                                            inlineSamplersNode,
                                                            inlineSamplers,
                                                            "some_kernel",
                                                            errors,
                                                            warnings);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, err);
    EXPECT_TRUE(warnings.empty()) << warnings;
    EXPECT_FALSE(errors.empty());
}

TEST(ZeInfoMetadataExtractionFromElf, givenValidElfContainingZeInfoSectionWhenExtractingZeInfoMetadataStringThenProperMetadataIsReturnedForEachElfType) {
    ConstStringRef zeInfoData{"mockZeInfoData\n"};
    constexpr auto mockSectionDataSize = 0x10;
    uint8_t mockSectionData[mockSectionDataSize]{0};

    NEO::Elf::ElfEncoder<Zebin::Elf::EI_CLASS_32> elfEncoder32B;
    NEO::Elf::ElfEncoder<Zebin::Elf::EI_CLASS_64> elfEncoder64B;

    elfEncoder32B.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfoData.data(), zeInfoData.size()));
    elfEncoder32B.appendSection(NEO::Elf::SHT_PROGBITS, "someOtherSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    auto encoded32BElf = elfEncoder32B.encode();

    elfEncoder64B.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfoData.data(), zeInfoData.size()));
    elfEncoder64B.appendSection(NEO::Elf::SHT_PROGBITS, "someOtherSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    auto encoded64BElf = elfEncoder64B.encode();

    std::string outErrors{}, outWarnings{};
    auto zeInfoStr32B = getZeInfoFromZebin(ArrayRef<const uint8_t>::fromAny(encoded32BElf.data(), encoded32BElf.size()), outErrors, outWarnings);
    auto zeInfoStr64B = getZeInfoFromZebin(ArrayRef<const uint8_t>::fromAny(encoded64BElf.data(), encoded64BElf.size()), outErrors, outWarnings);
    EXPECT_STREQ(zeInfoStr32B.data(), zeInfoData.data());
    EXPECT_STREQ(zeInfoStr64B.data(), zeInfoData.data());
}

TEST(ZeInfoMetadataExtractionFromElf, givenValidElfNotContainingZeInfoSectionWhenExtractingZeInfoMetadataStringThenEmptyDataIsReturnedForEachElfType) {
    constexpr auto mockSectionDataSize = 0x10;
    uint8_t mockSectionData[mockSectionDataSize]{0};

    NEO::Elf::ElfEncoder<Zebin::Elf::EI_CLASS_32> elfEncoder32B;
    NEO::Elf::ElfEncoder<Zebin::Elf::EI_CLASS_64> elfEncoder64B;

    elfEncoder32B.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, "notZeInfoSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    elfEncoder32B.appendSection(NEO::Elf::SHT_PROGBITS, "alsoNotZeInfoSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    auto encoded32BElf = elfEncoder32B.encode();

    elfEncoder64B.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, "notZeInfoSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    elfEncoder64B.appendSection(NEO::Elf::SHT_PROGBITS, "alsoNotZeInfoSection", ArrayRef<const uint8_t>::fromAny(mockSectionData, mockSectionDataSize));
    auto encoded64BElf = elfEncoder64B.encode();

    std::string outErrors{}, outWarnings{};
    auto zeInfoStr32B = getZeInfoFromZebin(ArrayRef<const uint8_t>::fromAny(encoded32BElf.data(), encoded32BElf.size()), outErrors, outWarnings);
    auto zeInfoStr64B = getZeInfoFromZebin(ArrayRef<const uint8_t>::fromAny(encoded64BElf.data(), encoded64BElf.size()), outErrors, outWarnings);
    EXPECT_EQ(nullptr, zeInfoStr32B.data());
    EXPECT_EQ(nullptr, zeInfoStr64B.data());
}

TEST(EncounterUnknownZeInfoAttribute, whenUnknownAttributeIsEncounteredThenErrorMessageAndErrorCodeAreSet) {
    std::string errorMessage, warning;
    NEO::DecodeError errorCode = NEO::DecodeError::success;
    NEO::Zebin::ZeInfo::encounterUnknownZeInfoAttribute("unknown_attribute", errorMessage, warning, errorCode);
    EXPECT_EQ(NEO::DecodeError::unkownZeinfoAttribute, errorCode);
    EXPECT_TRUE(warning.empty()) << warning;
    EXPECT_FALSE(errorMessage.empty());
}

TEST(EncounterUnknownZeInfoAttribute, givenEnvVariabletoIgnoreUnknownAttributesWhenUnknownAttributeIsEncounteredThenWarningIsSet) {
    DebugManagerStateRestore dbgRestore;
    NEO::debugManager.flags.IgnoreZebinUnknownAttributes.set(true);

    std::string errorMessage, warning;
    NEO::DecodeError errorCode = NEO::DecodeError::success;
    NEO::Zebin::ZeInfo::encounterUnknownZeInfoAttribute("unknown_attribute", errorMessage, warning, errorCode);
    EXPECT_EQ(NEO::DecodeError::success, errorCode);
    EXPECT_TRUE(errorMessage.empty()) << errorMessage;
    EXPECT_FALSE(warning.empty());
}
