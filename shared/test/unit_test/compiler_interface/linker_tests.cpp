/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/implicit_args_test_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "RelocationInfo.h"
#include "gtest/gtest.h"

#include <string>

TEST(SegmentTypeTests, givenSegmentTypeWhenAsStringIsCalledThenProperRepresentationIsReturned) {
    EXPECT_STREQ("UNKNOWN", NEO::asString(NEO::SegmentType::unknown));
    EXPECT_STREQ("GLOBAL_CONSTANTS", NEO::asString(NEO::SegmentType::globalConstants));
    EXPECT_STREQ("GLOBAL_VARIABLES", NEO::asString(NEO::SegmentType::globalVariables));
    EXPECT_STREQ("INSTRUCTIONS", NEO::asString(NEO::SegmentType::instructions));
}

TEST(LinkerInputTraitsTests, whenPointerSizeNotSizeThenDefaultsToHostPointerSize) {
    using PointerSize = NEO::LinkerInput::Traits::PointerSize;
    auto expectedPointerSize = (sizeof(void *) == 4) ? PointerSize::Ptr32bit : PointerSize::Ptr64bit;
    auto pointerSize = NEO::LinkerInput::Traits{}.pointerSize;
    EXPECT_EQ(expectedPointerSize, pointerSize);
}

TEST(LinkerInputTests, givenGlobalsSymbolTableThenProperlyDecodesGlobalVariablesAndGlobalConstants) {
    NEO::LinkerInput linkerInput;
    EXPECT_TRUE(linkerInput.isValid());
    vISA::GenSymEntry entry[2] = {{}, {}};
    entry[0].s_name[0] = 'A';
    entry[0].s_offset = 8;
    entry[0].s_size = 16;
    entry[0].s_type = vISA::GenSymType::S_GLOBAL_VAR;

    entry[1].s_name[0] = 'B';
    entry[1].s_offset = 24;
    entry[1].s_size = 8;
    entry[1].s_type = vISA::GenSymType::S_GLOBAL_VAR_CONST;

    EXPECT_EQ(0U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);

    auto decodeResult = linkerInput.decodeGlobalVariablesSymbolTable(entry, 2);
    EXPECT_TRUE(decodeResult);
    EXPECT_TRUE(linkerInput.isValid());

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SegmentType::globalVariables, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::globalConstants, symbolB->second.segment);

    auto symbolC = linkerInput.getSymbols().find("C");
    EXPECT_EQ(linkerInput.getSymbols().end(), symbolC);
}

TEST(LinkerInputTests, givenFunctionsSymbolTableThenProperlyDecodesGlobalVariablesAndGlobalConstants) {
    // Note : this is subject to change in IGC shotly.
    // GLOBAL_VAR/CONST will be ultimately allowed only in globalVariables symbol table.
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry[2] = {{}, {}};
    entry[0].s_name[0] = 'A';
    entry[0].s_offset = 8;
    entry[0].s_size = 16;
    entry[0].s_type = vISA::GenSymType::S_GLOBAL_VAR;

    entry[1].s_name[0] = 'B';
    entry[1].s_offset = 24;
    entry[1].s_size = 8;
    entry[1].s_type = vISA::GenSymType::S_GLOBAL_VAR_CONST;

    EXPECT_EQ(0U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);

    auto decodeResult = linkerInput.decodeExportedFunctionsSymbolTable(entry, 2, 3);
    EXPECT_TRUE(decodeResult);
    EXPECT_TRUE(linkerInput.isValid());

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SegmentType::globalVariables, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::globalConstants, symbolB->second.segment);

    auto symbolC = linkerInput.getSymbols().find("C");
    EXPECT_EQ(linkerInput.getSymbols().end(), symbolC);
}

TEST(LinkerInputTests, givenGlobalsSymbolTableThenFunctionExportsAreNotAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry = {};
    entry.s_name[0] = 'A';
    entry.s_offset = 8;
    entry.s_size = 16;
    entry.s_type = vISA::GenSymType::S_FUNC;

    auto decodeResult = linkerInput.decodeGlobalVariablesSymbolTable(&entry, 1);
    EXPECT_FALSE(decodeResult);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, givenFunctionsSymbolTableThenProperlyDecodesExportedFunctions) {
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry[2] = {{}, {}};
    entry[0].s_name[0] = 'A';
    entry[0].s_offset = 8;
    entry[0].s_size = 16;
    entry[0].s_type = vISA::GenSymType::S_FUNC;

    entry[1].s_name[0] = 'B';
    entry[1].s_offset = 24;
    entry[1].s_size = 8;
    entry[1].s_type = vISA::GenSymType::S_FUNC;

    EXPECT_EQ(0U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_EQ(-1, linkerInput.getExportedFunctionsSegmentId());

    auto decodeResult = linkerInput.decodeExportedFunctionsSymbolTable(entry, 2, 3);
    EXPECT_TRUE(decodeResult);
    EXPECT_TRUE(linkerInput.isValid());

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_TRUE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SegmentType::instructions, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::instructions, symbolB->second.segment);

    EXPECT_EQ(3, linkerInput.getExportedFunctionsSegmentId());
}

TEST(LinkerInputTests, givenFunctionsSymbolTableThenUndefIsAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry = {};
    entry.s_name[0] = 'A';
    entry.s_offset = 8;
    entry.s_size = 16;
    entry.s_type = vISA::GenSymType::S_UNDEF;

    auto decodeResult = linkerInput.decodeExportedFunctionsSymbolTable(&entry, 1, 3);
    EXPECT_TRUE(decodeResult);
    EXPECT_TRUE(linkerInput.isValid());
}

TEST(LinkerInputTests, givenFunctionsSymbolTableThenNoTypeIsNotAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry = {};
    entry.s_name[0] = 'A';
    entry.s_offset = 8;
    entry.s_size = 16;
    entry.s_type = vISA::GenSymType::S_NOTYPE;

    auto decodeResult = linkerInput.decodeExportedFunctionsSymbolTable(&entry, 1, 3);
    EXPECT_FALSE(decodeResult);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, givenRelocationTableThenRelocationEntriesAreProperlyParsed) {
    NEO::LinkerInput linkerInput;
    vISA::GenRelocEntry entry = {};
    entry.r_symbol[0] = 'A';
    entry.r_offset = 8;
    entry.r_type = vISA::GenRelocType::R_SYM_ADDR;

    auto decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 3);
    EXPECT_TRUE(decodeResult);

    EXPECT_EQ(0U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 1);
    EXPECT_TRUE(decodeResult);
    EXPECT_TRUE(linkerInput.isValid());
}

TEST(LinkerInputTests, givenRelocationTableThenNoneAsRelocationTypeIsNotAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenRelocEntry entry = {};
    entry.r_symbol[0] = 'A';
    entry.r_offset = 8;
    entry.r_type = vISA::GenRelocType::R_NONE;

    auto decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 3);
    EXPECT_FALSE(decodeResult);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, whenDataRelocationsAreAddedThenProperTraitsAreSet) {
    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 7U;
    relocInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;

    {
        NEO::LinkerInput linkerInput;
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);

        linkerInput.addDataRelocationInfo(relocInfo);
        ASSERT_EQ(1U, linkerInput.getDataRelocations().size());
        EXPECT_EQ(relocInfo.offset, linkerInput.getDataRelocations()[0].offset);
        EXPECT_EQ(relocInfo.relocationSegment, linkerInput.getDataRelocations()[0].relocationSegment);
        EXPECT_EQ(relocInfo.symbolName, linkerInput.getDataRelocations()[0].symbolName);
        EXPECT_EQ(relocInfo.type, linkerInput.getDataRelocations()[0].type);
        EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
        EXPECT_TRUE(linkerInput.isValid());
    }

    {
        NEO::LinkerInput linkerInput;
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
        relocInfo.relocationSegment = NEO::SegmentType::globalVariables;
        linkerInput.addDataRelocationInfo(relocInfo);
        ASSERT_EQ(1U, linkerInput.getDataRelocations().size());
        EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
        EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
        EXPECT_TRUE(linkerInput.isValid());
    }
}

TEST(LinkerInputTests, WhenGettingSegmentForSectionNameThenCorrectSegmentIsReturned) {
    auto segmentConst = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataConst.str());
    auto segmentGlobalConst = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataGlobalConst.str());
    auto segmentGlobal = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataGlobal.str());
    auto segmentConstString = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataConstString.str());
    auto segmentInstructions = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::textPrefix.str());
    auto segmentInstructions2 = NEO::LinkerInput::getSegmentForSection(".text.abc");
    auto segmentGlobalZeroInit = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataGlobalZeroInit.str());
    auto segmentGlobalConstZeroInit = NEO::LinkerInput::getSegmentForSection(NEO::Zebin::Elf::SectionNames::dataConstZeroInit.str());

    EXPECT_EQ(NEO::SegmentType::globalConstants, segmentConst);
    EXPECT_EQ(NEO::SegmentType::globalConstants, segmentGlobalConst);
    EXPECT_EQ(NEO::SegmentType::globalVariables, segmentGlobal);
    EXPECT_EQ(NEO::SegmentType::globalStrings, segmentConstString);
    EXPECT_EQ(NEO::SegmentType::instructions, segmentInstructions);
    EXPECT_EQ(NEO::SegmentType::instructions, segmentInstructions2);
    EXPECT_EQ(NEO::SegmentType::globalVariablesZeroInit, segmentGlobalZeroInit);
    EXPECT_EQ(NEO::SegmentType::globalConstantsZeroInit, segmentGlobalConstZeroInit);
}

TEST(LinkerInputTests, WhenGettingSegmentForUnknownSectionNameThenUnknownSegmentIsReturned) {
    auto segment = NEO::LinkerInput::getSegmentForSection("Not_a_valid_section_name");
    EXPECT_EQ(NEO::SegmentType::unknown, segment);
}

TEST(LinkerInputTests, WhenAddingElfTextRelocationForSegmentIndexThenInstructionSegmentForRelocationAndProperTraitsAreSet) {
    NEO::LinkerInput linkerInput = {};
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 7u;
    relocInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocInfo.symbolName = "test";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    relocInfo.relocationSegment = NEO::SegmentType::globalVariables;

    linkerInput.addElfTextSegmentRelocation(relocInfo, 5);

    ASSERT_EQ(0u, linkerInput.getDataRelocations().size());
    ASSERT_EQ(6u, linkerInput.getRelocationsInInstructionSegments().size());

    auto relocs = linkerInput.getRelocationsInInstructionSegments()[5];

    ASSERT_EQ(1u, relocs.size());

    EXPECT_EQ(NEO::SegmentType::instructions, relocs[0].relocationSegment);
    EXPECT_EQ(std::string("test"), relocs[0].symbolName);
    EXPECT_EQ(7u, relocs[0].offset);

    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
    EXPECT_TRUE(linkerInput.isValid());
}

TEST(LinkerInputTests, WhenAddingTwoElfTextRelocationForSingleSegmentIndexThenBothRelocationsAreAddedForTheSameSegment) {
    NEO::LinkerInput linkerInput = {};
    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 7u;
    relocInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocInfo.symbolName = "test";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    relocInfo.relocationSegment = NEO::SegmentType::globalVariables;

    linkerInput.addElfTextSegmentRelocation(relocInfo, 0);

    ASSERT_EQ(1u, linkerInput.getRelocationsInInstructionSegments().size());
    auto &relocs = linkerInput.getRelocationsInInstructionSegments()[0];
    EXPECT_EQ(1u, relocs.size());

    relocInfo.offset = 24u;
    linkerInput.addElfTextSegmentRelocation(relocInfo, 0);

    EXPECT_EQ(2u, relocs.size());
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
}

TEST(LinkerInputTests, GivenVarDataSegmentThenIsVarDataSegmentReturnsTrue) {
    EXPECT_TRUE(isVarDataSegment(SegmentType::globalVariables));
    EXPECT_TRUE(isVarDataSegment(SegmentType::globalVariablesZeroInit));
}

TEST(LinkerInputTests, GivenConstDataSegmentThenIsConstDataSegmentReturnsTrue) {
    EXPECT_TRUE(isConstDataSegment(SegmentType::globalConstants));
    EXPECT_TRUE(isConstDataSegment(SegmentType::globalConstantsZeroInit));
}

TEST(LinkerInputTests, GivenTwoGlobalSymbolsOfTypeFunctionEachPointingToDifferentInstructionSectionWhenDecodingElfThenLinkerInputIsInvalid) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    elf64.addSymbol(0, 0x1234000, 8, 0, Elf::STT_FUNC, Elf::STB_GLOBAL);
    elf64.addSymbol(8, 0x5000, 16, 2, Elf::STT_FUNC, Elf::STB_GLOBAL);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}, {"hello", 1}};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, GivenGlobalSymbolOfTypeObjectPointingToDataGlobalSectionWhenDecodingElfThenTraitExportsGlobalVariablesIsSet) {
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.global";
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    elf64.addSymbol(0, 0x20, 8, 1, Elf::STT_OBJECT, Elf::STB_GLOBAL);
    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}};

    NEO::LinkerInput linkerInput = {};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalVariables);
    auto &symbol = linkerInput.getSymbols().at("0");
    EXPECT_EQ(0x20U, symbol.offset);
    EXPECT_EQ(8U, symbol.size);
    EXPECT_EQ(SegmentType::globalVariables, symbol.segment);
    EXPECT_TRUE(symbol.global);
}

TEST(LinkerInputTests, GivenGlobalSymbolOfTypeObjectPointingToDataConstSectionWhenDecodingElfThenTraitExportsGlobalVariablesIsSet) {
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    elf64.addSymbol(0, 0, 8, 1, Elf::STT_OBJECT, Elf::STB_GLOBAL);
    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}};

    NEO::LinkerInput linkerInput = {};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    auto &symbol = linkerInput.getSymbols().at("0");
    EXPECT_EQ(0U, symbol.offset);
    EXPECT_EQ(8U, symbol.size);
    EXPECT_EQ(SegmentType::globalConstants, symbol.segment);
    EXPECT_TRUE(symbol.global);
}

TEST(LinkerInputTests, GivenGlobalSymbolOfTypeFuncPointingToFunctionsSectionWhenDecodingElfThenTraitExportsFunctionsIsSetAndExportedFunctionsSegmentIdIsSet) {
    for (auto functionsSectionName : {Zebin::Elf::SectionNames::functions, Zebin::Elf::SectionNames::text}) {
        MockElf<NEO::Elf::EI_CLASS_64> elf64;

        std::unordered_map<uint32_t, std::string> sectionNames;
        sectionNames[0] = ".text.abc";
        sectionNames[1] = functionsSectionName.str();
        elf64.setupSectionNames(std::move(sectionNames));
        elf64.overrideSymbolName = true;

        elf64.addSymbol(0, 0, 32, 1, Elf::STT_FUNC, Elf::STB_GLOBAL);
        NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0},
                                                                      {Zebin::Elf::SectionNames::externalFunctions.str(), 1}};

        NEO::LinkerInput linkerInput = {};
        linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
        EXPECT_TRUE(linkerInput.isValid());
        EXPECT_TRUE(linkerInput.getTraits().exportsFunctions);
        EXPECT_EQ(1, linkerInput.getExportedFunctionsSegmentId()) << functionsSectionName.str();
        auto &symbol = linkerInput.getSymbols().at("0");
        EXPECT_EQ(0U, symbol.offset);
        EXPECT_EQ(32U, symbol.size);
        EXPECT_EQ(SegmentType::instructions, symbol.segment);
        EXPECT_EQ(1U, symbol.instructionSegmentId);
        EXPECT_TRUE(symbol.global);
    }
}

TEST(LinkerInputTests, GivenGlobalSymbolOfTypeDifferentThantObjectOrFuncWhenDecodingElfThenItIsIgnored) {
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    elf64.addSymbol(0, 0, 8, 0, Elf::STT_NOTYPE, Elf::STB_GLOBAL);
    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}};
    NEO::LinkerInput linkerInput = {};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    EXPECT_EQ(0U, linkerInput.getSymbols().size());
}

TEST(LinkerInputTests, GivenGlobalSymbolPointingToSectionDifferentThanInstructionsOrDataWhenDecodingElfThenItIsIgnoredAndAddedToExternalSymbols) {
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ""; // UNDEF section
    sectionNames[1] = ".text.abc";
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    constexpr uint32_t externalSymbol = 3;
    elf64.addSymbol(externalSymbol, 0, 8, 0, Elf::STT_OBJECT, Elf::STB_GLOBAL);
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    EXPECT_EQ(0U, linkerInput.getSymbols().size());
    EXPECT_EQ(1U, linkerInput.externalSymbols.size());
    EXPECT_EQ(std::to_string(externalSymbol), linkerInput.externalSymbols[0]);
}

TEST(LInkerInputTests, GivenSymbolPointingToInstructionSegmentAndInvalidInstructionSectionNameMappingWhenDecodingElfThenLinkerInputIsInvalid) {
    NEO::LinkerInput linkerInput = {};
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    elf64.addSymbol(0, 0, 8, 0, Elf::STT_FUNC, Elf::STB_GLOBAL);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["wrong_name"] = 0;
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, GivenInstructionRelocationAndInvalidInstructionSectionNameMappingWhenDecodingElfThenLinkerInputIsInvalid) {
    NEO::LinkerInput linkerInput = {};
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";

    elf64.setupSectionNames(std::move(sectionNames));
    elf64.addReloc(64, 0, Zebin::Elf::R_ZE_SYM_ADDR, 0, 0, "0");

    elf64.overrideSymbolName = true;
    elf64.addSymbol(0, 0x1234000, 8, 1, Elf::STT_OBJECT, Elf::STB_GLOBAL);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"wrong_name", 0}};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, GivenRelocationWithSymbolIdOutOfBoundsOfSymbolTableWhenDecodingElfThenLinkerInputIsInvalid) {
    NEO::LinkerInput linkerInput = {};
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames = {{0, ".text.abc"}};
    elf64.setupSectionNames(std::move(sectionNames));

    elf64.addReloc(64, 0, Zebin::Elf::R_ZE_SYM_ADDR, 0, 2, "symbol");

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["abc"] = 0;
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_FALSE(linkerInput.isValid());
}

TEST(LinkerInputTests, GivenRelocationToSectionDifferentThanDataOrInstructionsWhenDecodingElfThenItIsIgnored) {
    NEO::LinkerInput linkerInput = {};
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames = {{0, ".text.abc"},
                                                              {1, "unknown.section"}};
    elf64.setupSectionNames(std::move(sectionNames));
    elf64.addReloc(64, 0, Zebin::Elf::R_ZE_SYM_ADDR, 1, 2, "symbol");

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["abc"] = 0;
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    EXPECT_TRUE(linkerInput.getDataRelocations().empty());
    EXPECT_TRUE(linkerInput.getRelocationsInInstructionSegments().empty());
}

TEST(LinkerInputTests, GivenGlobalDataRelocationWithLocalSymbolPointingToConstDataWhenDecodingElfThenRelocationInfoAndSymbolInfoAreCreated) {
    NEO::LinkerInput linkerInput = {};
    MockElf<NEO::Elf::EI_CLASS_64> elf64;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".data.global";

    elf64.setupSectionNames(std::move(sectionNames));
    elf64.addReloc(64, 10, Zebin::Elf::R_ZE_SYM_ADDR, 2, 0, "0");

    elf64.overrideSymbolName = true;
    elf64.addSymbol(0, 0x0, 8, 1, Elf::STT_OBJECT, Elf::STB_LOCAL);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0}};
    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
    EXPECT_TRUE(linkerInput.isValid());
    auto &symbol = linkerInput.getSymbols().at("0");
    EXPECT_FALSE(symbol.global);
    EXPECT_EQ(0U, symbol.offset);
    EXPECT_EQ(8U, symbol.size);
    EXPECT_EQ(SegmentType::globalConstants, symbol.segment);
    auto &dataRelocations = linkerInput.getDataRelocations();
    EXPECT_EQ(1U, dataRelocations.size());
    EXPECT_EQ(64U, dataRelocations[0].offset);
    EXPECT_EQ(10U, dataRelocations[0].addend);
    EXPECT_EQ(SegmentType::globalVariables, dataRelocations[0].relocationSegment);
    EXPECT_EQ("0", dataRelocations[0].symbolName);
    EXPECT_EQ(LinkerInput::RelocationInfo::Type::address, dataRelocations[0].type);
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
}

TEST(LinkerInputTests, GivenInstructionRelocationWithLocalSymbolPointingToFunctionsWhendDecodingElfThenRelocationsInfoAndSymbolInfoAreCreated) {
    for (auto functionsSectionName : {Zebin::Elf::SectionNames::functions, Zebin::Elf::SectionNames::text}) {
        NEO::LinkerInput linkerInput = {};
        MockElf<NEO::Elf::EI_CLASS_64> elf64;

        std::unordered_map<uint32_t, std::string> sectionNames;
        sectionNames[0] = ".text.abc";
        sectionNames[1] = functionsSectionName.str();

        elf64.setupSectionNames(std::move(sectionNames));
        elf64.addReloc(64, 10, Zebin::Elf::R_ZE_SYM_ADDR, 0, 0, "0");

        elf64.overrideSymbolName = true;
        elf64.addSymbol(0, 0x10, 0x40, 1, Elf::STT_FUNC, Elf::STB_LOCAL);

        NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId = {{"abc", 0},
                                                                      {Zebin::Elf::SectionNames::externalFunctions.str(), 1}};
        linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);
        EXPECT_TRUE(linkerInput.isValid());
        auto &symbol = linkerInput.getSymbols().at("0");
        EXPECT_FALSE(symbol.global);
        EXPECT_EQ(0x10U, symbol.offset);
        EXPECT_EQ(0x40U, symbol.size);
        EXPECT_EQ(SegmentType::instructions, symbol.segment);
        EXPECT_EQ(1U, symbol.instructionSegmentId);

        auto &instructionRelocsPerSeg = linkerInput.getRelocationsInInstructionSegments();
        EXPECT_EQ(1U, instructionRelocsPerSeg.size());
        auto &relocations = instructionRelocsPerSeg[0];
        EXPECT_EQ(1U, relocations.size());
        EXPECT_EQ(64U, relocations[0].offset);
        EXPECT_EQ(10U, relocations[0].addend);
        EXPECT_EQ(SegmentType::instructions, relocations[0].relocationSegment);
        EXPECT_EQ("0", relocations[0].symbolName);
        EXPECT_EQ(LinkerInput::RelocationInfo::Type::address, relocations[0].type);
        EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
    }
}

using LinkerTests = Test<DeviceFixture>;

TEST_F(LinkerTests, GivenSymbolToInstructionsAndNoCorrespondingInstructionSegmentWhenRelocatingSymbolsThenFail) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &symbol = linkerInput.symbols["func"];
    symbol.segment = SegmentType::instructions;
    symbol.instructionSegmentId = 4;

    WhiteBox<NEO::Linker> linker(linkerInput);
    Linker::PatchableSegments instSegments;
    auto result = linker.relocateSymbols({}, {}, {}, {}, instSegments, 0U, 0U);
    EXPECT_FALSE(result);
}

TEST_F(LinkerTests, GivenSymbolToInstructionBiggerThanCorrespondingInstructionSegmentWhenRelocatingSymbolsThenFail) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &symbol = linkerInput.symbols["func"];
    symbol.segment = SegmentType::instructions;
    symbol.instructionSegmentId = 0;
    symbol.offset = 8;
    symbol.size = 8;

    WhiteBox<NEO::Linker> linker(linkerInput);
    Linker::PatchableSegments instSegments(1);
    instSegments[0].segmentSize = static_cast<size_t>(symbol.offset + symbol.size - 1);

    auto result = linker.relocateSymbols({}, {}, {}, {}, instSegments, 0U, 0U);
    EXPECT_FALSE(result);
}

TEST_F(LinkerTests, GivenValidSymbolToInstructionsWhenRelocatingSymbolsThenSymbolIsProperlyRelocated) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &symbol = linkerInput.symbols["func"];
    symbol.segment = SegmentType::instructions;
    symbol.instructionSegmentId = 0;
    symbol.offset = 4;
    symbol.size = 8;

    WhiteBox<NEO::Linker> linker(linkerInput);
    Linker::PatchableSegments instSegments(1);
    instSegments[0].segmentSize = 32;
    instSegments[0].gpuAddress = 0xaabbccdd;

    auto result = linker.relocateSymbols({}, {}, {}, {}, instSegments, 0U, 0U);
    EXPECT_TRUE(result);
    EXPECT_EQ(1U, linker.relocatedSymbols.size());
    EXPECT_EQ(linker.relocatedSymbols["func"].gpuAddress, instSegments[0].gpuAddress + symbol.offset);
}

TEST_F(LinkerTests, GivenRelocationToInstructionSegmentWithLocalUndefinedSymbolWhenPatchingInstructionsSegmentsThenItIsPatchedWithZeroes) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfInstructionSegments = true;
    NEO::LinkerInput::RelocationInfo rela;
    rela.offset = 0U;
    rela.addend = 128U;
    rela.type = NEO::LinkerInput::RelocationInfo::Type::address;
    rela.symbolName = "";
    rela.relocationSegment = NEO::SegmentType::instructions;
    linkerInput.textRelocations.push_back({rela});

    WhiteBox<NEO::Linker> linker(linkerInput);

    uint64_t instructionSegmentData{std::numeric_limits<uint64_t>::max()};
    NEO::Linker::PatchableSegment instructionSegmentToPatch;
    instructionSegmentToPatch.hostPointer = reinterpret_cast<void *>(&instructionSegmentData);
    instructionSegmentToPatch.segmentSize = sizeof(instructionSegmentData);

    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;

    linker.patchInstructionsSegments({instructionSegmentToPatch}, unresolvedExternals, kernelDescriptors);
    auto instructionSegmentPatchedData = reinterpret_cast<uint64_t *>(ptrOffset(instructionSegmentToPatch.hostPointer, static_cast<size_t>(rela.offset)));
    EXPECT_EQ(0u, static_cast<uint64_t>(*instructionSegmentPatchedData));
    EXPECT_EQ(0u, unresolvedExternals.size());
}

TEST(LinkerInputTests, GivenInvalidFunctionsSymbolsUsedInFunctionsRelocationsWhenParsingRelocationsForExtFuncUsageThenDoNotAddDependency) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;

    auto &extFuncSymbols = mockLinkerInput.extFuncSymbols;
    extFuncSymbols.resize(1);
    auto &funSym = extFuncSymbols[0];
    funSym.first = "fun";
    funSym.second.offset = 4U;
    funSym.second.size = 4U;

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.symbolName = "fun";

    relocInfo.offset = 0U;
    mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, NEO::Zebin::Elf::SectionNames::externalFunctions.str());
    EXPECT_TRUE(mockLinkerInput.extFunDependencies.empty());

    relocInfo.offset = 0x10U;
    mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, NEO::Zebin::Elf::SectionNames::externalFunctions.str());
    EXPECT_TRUE(mockLinkerInput.extFunDependencies.empty());
}

TEST(LinkerInputTests, GivenFunctionSymbolsUsedInFunctionsRelocationsWhenParsingRelocationsForExtFuncUsageThenAddDependency) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;

    auto &extFuncSymbols = mockLinkerInput.extFuncSymbols;
    extFuncSymbols.resize(2);
    auto &funSym = extFuncSymbols[0];
    funSym.first = "fun";
    funSym.second.offset = 4U;
    funSym.second.size = 4U;

    auto &fun2Sym = extFuncSymbols[1];
    fun2Sym.first = "fun2";
    fun2Sym.second.offset = 8U;
    fun2Sym.second.size = 4U;

    NEO::LinkerInput::RelocationInfo relocInfo{};
    relocInfo.symbolName = "fun3";

    relocInfo.offset = 6U;
    mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, NEO::Zebin::Elf::SectionNames::externalFunctions.str());
    EXPECT_EQ(1u, mockLinkerInput.extFunDependencies.size());
    EXPECT_EQ(relocInfo.symbolName, mockLinkerInput.extFunDependencies[0].usedFuncName);
    EXPECT_EQ(funSym.first, mockLinkerInput.extFunDependencies[0].callerFuncName);

    mockLinkerInput.extFunDependencies.clear();
    relocInfo.offset = 8U;
    mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, NEO::Zebin::Elf::SectionNames::externalFunctions.str());
    EXPECT_EQ(1u, mockLinkerInput.extFunDependencies.size());
    EXPECT_EQ(relocInfo.symbolName, mockLinkerInput.extFunDependencies[0].usedFuncName);
    EXPECT_EQ(fun2Sym.first, mockLinkerInput.extFunDependencies[0].callerFuncName);
}

TEST(LinkerInputTests, GivenExternalFunctionsSymbolsUsedInKernelRelocationsWhenParsingRelocationsForExtFuncUsageForKernelThenAddKernelDependency) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;

    NEO::LinkerInput::RelocationInfo relocInfo{};
    relocInfo.symbolName = "fun";

    std::string kernelName = "kernel";
    mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, kernelName);
    EXPECT_TRUE(mockLinkerInput.extFunDependencies.empty());
    EXPECT_EQ(1u, mockLinkerInput.kernelDependencies.size());
    EXPECT_EQ(relocInfo.symbolName, mockLinkerInput.kernelDependencies[0].usedFuncName);
    EXPECT_EQ(kernelName, mockLinkerInput.kernelDependencies[0].kernelName);
}

TEST(LinkerInputTests, GivenNonFunctionSymbolRelocationInKernelRelocationsWhenParsingRelocationsForExtFuncUsageForKernelThenDoNotAddKernelDependency) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;

    auto &symbols = mockLinkerInput.symbols;
    SymbolInfo symbol0 = {
        .segment = SegmentType::globalStrings};
    SymbolInfo symbol1 = {
        .segment = SegmentType::globalVariables};
    SymbolInfo symbol2 = {
        .segment = SegmentType::instructions};
    symbols.emplace(".str", symbol0);
    symbols.emplace("globalVar", symbol1);
    symbols.emplace("fun", symbol2);

    for (auto nonFuncRelocationName : {
             std::string_view(".str"),
             std::string_view("globalVar"),
             std::string_view("")}) {

        NEO::LinkerInput::RelocationInfo relocInfo{};
        relocInfo.symbolName = nonFuncRelocationName;

        std::string kernelName = "kernel";
        mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, kernelName);
        EXPECT_TRUE(mockLinkerInput.extFunDependencies.empty());
        EXPECT_EQ(0u, mockLinkerInput.kernelDependencies.size());
    }
}

TEST(LinkerInputTests, GivenExternalSymbolRelocationInKernelRelocationsWhenParsingRelocationsForExtFuncUsageForKernelThenAddOptionalKernelDependency) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;

    auto &externalSymbols = mockLinkerInput.externalSymbols;

    externalSymbols.push_back(std::string(implicitArgsRelocationSymbolName));
    externalSymbols.push_back(std::string(Linker::perThreadOff));
    externalSymbols.push_back(std::string(Linker::subDeviceID));

    for (auto relocationName : {
             implicitArgsRelocationSymbolName,
             Linker::perThreadOff,
             Linker::subDeviceID}) {

        NEO::LinkerInput::RelocationInfo relocInfo{};
        relocInfo.symbolName = relocationName;

        std::string kernelName = "kernel";
        mockLinkerInput.parseRelocationForExtFuncUsage(relocInfo, kernelName);
        EXPECT_TRUE(mockLinkerInput.extFunDependencies.empty());
    }

    EXPECT_EQ(3u, mockLinkerInput.kernelDependencies.size());
    for (auto &kernelDependency : mockLinkerInput.kernelDependencies) {
        EXPECT_TRUE(kernelDependency.optional);
    }
}

HWTEST_F(LinkerTests, givenEmptyLinkerInputThenLinkerOutputIsEmpty) {
    NEO::LinkerInput linkerInput;
    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
}

HWTEST_F(LinkerTests, givenInvalidLinkerInputThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;
    mockLinkerInput.valid = false;
    NEO::Linker linker(mockLinkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT extFuncs;

    auto linkResult = linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, extFuncs);
    EXPECT_EQ(NEO::LinkingStatus::error, linkResult);
}

HWTEST_F(LinkerTests, givenUnresolvedExternalSymbolsWhenResolveBuiltinsIsCalledThenSubDeviceIDSymbolsAreRemoved) {
    struct LinkerMock : public NEO::Linker {
      public:
        using NEO::Linker::Linker;
        using NEO::Linker::resolveBuiltins;
    };

    NEO::LinkerInput linkerInput;
    LinkerMock linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    unresolvedExternals.push_back({{"__SubDeviceID", 0, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 156, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 140, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__SubDeviceID", 64, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});

    std::vector<char> instructionSegment;
    instructionSegment.resize(128u);
    NEO::Linker::PatchableSegments instructionsSegments;
    instructionsSegments.push_back({instructionSegment.data(), 64u});
    instructionsSegments.push_back({&instructionSegment[64], 64u});

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(1);

    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    linker.resolveBuiltins(pDevice, unresolvedExternals, instructionsSegments, kernelDescriptors);

    EXPECT_EQ(2U, unresolvedExternals.size());
    for (auto &symbol : unresolvedExternals) {
        EXPECT_NE(NEO::Linker::subDeviceID, symbol.unresolvedRelocation.symbolName);
    }

    auto gpuAddressAs64bit = pDevice->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(&instructionSegment[64]), static_cast<uint32_t>((gpuAddressAs64bit >> 32) & 0xffffffff));
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(instructionSegment.data()), static_cast<uint32_t>(gpuAddressAs64bit & 0xffffffff));
}

HWTEST_F(LinkerTests, givenUnresolvedExternalsWhenLinkThenSubDeviceIDSymbolsAreCorrectlyHandled) {
    NEO::LinkerInput linkerInput;

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    unresolvedExternals.push_back({{"__SubDeviceID", 0, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 156, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 140, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__SubDeviceID", 64, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});

    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    std::vector<char> instructionSegment;
    instructionSegment.resize(128u);
    NEO::Linker::PatchableSegments instructionsSegments;
    instructionsSegments.push_back({instructionSegment.data(), 0});
    instructionsSegments.push_back({&instructionSegment[64], 64u});

    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    debugManager.flags.EnableImplicitScaling.set(1);

    linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, instructionsSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);

    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    EXPECT_EQ(2u, unresolvedExternals.size());

    for (auto &symbol : unresolvedExternals) {
        EXPECT_NE(NEO::Linker::subDeviceID, symbol.unresolvedRelocation.symbolName);
    }

    auto gpuAddressAs64bit = pDevice->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(&instructionSegment[64]), static_cast<uint32_t>((gpuAddressAs64bit >> 32) & 0xffffffff));
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(instructionSegment.data()), static_cast<uint32_t>(gpuAddressAs64bit & 0xffffffff));
}

HWTEST_F(LinkerTests, givenUnresolvedExternalSymbolsWhenResolveBuiltinsIsCalledThenPerThreadOffSymbolsAreResolvedAndRemoved) {
    struct LinkerMock : public NEO::Linker {
      public:
        using NEO::Linker::Linker;
        using NEO::Linker::resolveBuiltins;
    };

    const uint64_t kernel1RelocOffset = 40;
    const uint64_t kernel2RelocOffset = 0;

    NEO::LinkerInput linkerInput;
    LinkerMock linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    unresolvedExternals.push_back({{"__INTEL_PER_THREAD_OFF", 0, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions, ".text.kernel_func2"}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 156, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 140, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__INTEL_PER_THREAD_OFF", kernel1RelocOffset, static_cast<NEO::Linker::RelocationInfo::Type>(7u), NEO::SegmentType::instructions, ".text.kernel_func1"}, 0u, false});

    std::vector<char> instructionSegment;
    instructionSegment.resize(kernel1RelocOffset + 16);
    NEO::Linker::PatchableSegments instructionsSegments;
    instructionsSegments.push_back({instructionSegment.data(), 0u});

    KernelDescriptor kernelDescriptor1;
    kernelDescriptor1.kernelMetadata.kernelName = "kernel_func1";
    kernelDescriptor1.kernelAttributes.crossThreadDataSize = 96;
    kernelDescriptor1.kernelAttributes.inlineDataPayloadSize = 64;

    KernelDescriptor kernelDescriptor2;
    kernelDescriptor2.kernelMetadata.kernelName = "kernel_func2";
    kernelDescriptor2.kernelAttributes.crossThreadDataSize = 192;
    kernelDescriptor2.kernelAttributes.inlineDataPayloadSize = 64;

    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    kernelDescriptors.push_back(&kernelDescriptor1);
    kernelDescriptors.push_back(&kernelDescriptor2);
    linker.resolveBuiltins(pDevice, unresolvedExternals, instructionsSegments, kernelDescriptors);

    EXPECT_EQ(2U, unresolvedExternals.size());
    for (auto &symbol : unresolvedExternals) {
        EXPECT_NE(NEO::Linker::perThreadOff, symbol.unresolvedRelocation.symbolName);
    }

    uint16_t gpuAddress1 = kernelDescriptor1.kernelAttributes.crossThreadDataSize -
                           kernelDescriptor1.kernelAttributes.inlineDataPayloadSize;

    uint16_t gpuAddress2 = kernelDescriptor2.kernelAttributes.crossThreadDataSize -
                           kernelDescriptor2.kernelAttributes.inlineDataPayloadSize;

    EXPECT_EQ(*reinterpret_cast<uint16_t *>(&instructionSegment[kernel1RelocOffset]), static_cast<uint16_t>(gpuAddress1));
    EXPECT_EQ(*reinterpret_cast<uint32_t *>(&instructionSegment[kernel2RelocOffset]), static_cast<uint32_t>(gpuAddress2 & 0xffffffff));
}

HWTEST_F(LinkerTests, givenPerThreadOffSymbolInUnresolvedExternalSymbolsAndMissingKernelDescriptorForPerThreadOffSymbolWhenResolveBuiltinsThenPerThreadOffSymbolIsNotResolved) {
    struct LinkerMock : public NEO::Linker {
      public:
        using NEO::Linker::Linker;
        using NEO::Linker::resolveBuiltins;
    };

    const uint64_t kernelRelocOffset = 40;

    NEO::LinkerInput linkerInput;
    LinkerMock linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 156, NEO::Linker::RelocationInfo::Type::addressLow, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__MaxHWThreadIDPerSubDevice", 140, NEO::Linker::RelocationInfo::Type::addressHigh, NEO::SegmentType::instructions}, 0u, false});
    unresolvedExternals.push_back({{"__INTEL_PER_THREAD_OFF", kernelRelocOffset, NEO::Linker::RelocationInfo::Type::address16, NEO::SegmentType::instructions, ".text.kernel_func"}, 0u, false});

    std::vector<char> instructionSegment;
    instructionSegment.resize(64);
    NEO::Linker::PatchableSegments instructionsSegments;
    instructionsSegments.push_back({instructionSegment.data(), 0u});

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelMetadata.kernelName = "kernel_name";
    kernelDescriptor.kernelAttributes.crossThreadDataSize = 96;

    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    kernelDescriptors.push_back(&kernelDescriptor);
    linker.resolveBuiltins(pDevice, unresolvedExternals, instructionsSegments, kernelDescriptors);

    EXPECT_EQ(3U, unresolvedExternals.size());

    bool isPerThreadOffUnresolved = false;
    for (auto &symbol : unresolvedExternals) {
        if (NEO::Linker::perThreadOff == symbol.unresolvedRelocation.symbolName) {
            isPerThreadOffUnresolved = true;
        }
    }
    EXPECT_TRUE(isPerThreadOffUnresolved);
}

HWTEST_F(LinkerTests, givenUnresolvedExternalSymbolsAndCrossThreadDataSmallerThanInlineDataWhenResolveBuiltinsIsCalledThenPerThreadOffSymbolIsResolvedAndRemoved) {
    struct LinkerMock : public NEO::Linker {
      public:
        using NEO::Linker::Linker;
        using NEO::Linker::resolveBuiltins;
    };

    const uint64_t kernelRelocOffset = 40u;

    NEO::LinkerInput linkerInput{};
    LinkerMock linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals{};
    unresolvedExternals.push_back({{std::string(NEO::Linker::perThreadOff), kernelRelocOffset, NEO::Linker::RelocationInfo::Type::address16, NEO::SegmentType::instructions, ".text.kernel_func1"}, 0u, false});

    std::vector<char> instructionSegment{};
    instructionSegment.resize(kernelRelocOffset + 16);
    NEO::Linker::PatchableSegments instructionsSegments{};
    instructionsSegments.push_back({instructionSegment.data(), 0u});

    KernelDescriptor kernelDescriptor1{};
    kernelDescriptor1.kernelMetadata.kernelName = "kernel_func1";
    kernelDescriptor1.kernelAttributes.crossThreadDataSize = 40u;
    kernelDescriptor1.kernelAttributes.inlineDataPayloadSize = 64u;

    NEO::Linker::KernelDescriptorsT kernelDescriptors{};
    kernelDescriptors.push_back(&kernelDescriptor1);
    linker.resolveBuiltins(pDevice, unresolvedExternals, instructionsSegments, kernelDescriptors);

    EXPECT_EQ(0U, unresolvedExternals.size());

    uint16_t gpuAddress = 0u;
    EXPECT_EQ(*reinterpret_cast<uint16_t *>(&instructionSegment[kernelRelocOffset]), gpuAddress);
}

HWTEST_F(LinkerTests, givenUnresolvedExternalWhenPatchingInstructionsThenLinkPartially) {
    NEO::LinkerInput linkerInput;
    vISA::GenRelocEntry entry = {};
    entry.r_symbol[0] = 'A';
    entry.r_offset = 8;
    entry.r_type = vISA::GenRelocType::R_SYM_ADDR;
    auto decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 0);
    EXPECT_TRUE(decodeResult);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedPartially, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_EQ(0U, unresolvedExternals[0].instructionsSegmentId);
    EXPECT_FALSE(unresolvedExternals[0].internalError);
    EXPECT_EQ(entry.r_offset, unresolvedExternals[0].unresolvedRelocation.offset);
    EXPECT_EQ(std::string(entry.r_symbol), std::string(unresolvedExternals[0].unresolvedRelocation.symbolName));
}

HWTEST_F(LinkerTests, givenValidSymbolsAndRelocationsThenInstructionSegmentsAreProperlyPatched) {
    NEO::LinkerInput linkerInput;

    vISA::GenSymEntry symGlobalVariable = {};
    symGlobalVariable.s_name[0] = 'A';
    symGlobalVariable.s_offset = 4;
    symGlobalVariable.s_size = 16;
    symGlobalVariable.s_type = vISA::GenSymType::S_GLOBAL_VAR;
    bool decodeSymSuccess = linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalVariable, 1);

    vISA::GenSymEntry symGlobalConstant = {};
    symGlobalConstant.s_name[0] = 'B';
    symGlobalConstant.s_offset = 20;
    symGlobalConstant.s_size = 8;
    symGlobalConstant.s_type = vISA::GenSymType::S_GLOBAL_VAR_CONST;
    decodeSymSuccess = decodeSymSuccess && linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalConstant, 1);

    vISA::GenSymEntry symExportedFunc = {};
    symExportedFunc.s_name[0] = 'C';
    symExportedFunc.s_offset = 16;
    symExportedFunc.s_size = 32;
    symExportedFunc.s_type = vISA::GenSymType::S_FUNC;

    decodeSymSuccess = decodeSymSuccess && linkerInput.decodeExportedFunctionsSymbolTable(&symExportedFunc, 1, 0);
    EXPECT_TRUE(decodeSymSuccess);

    vISA::GenRelocEntry relocA = {};
    relocA.r_symbol[0] = 'A';
    relocA.r_offset = 0;
    relocA.r_type = vISA::GenRelocType::R_SYM_ADDR;

    vISA::GenRelocEntry relocB = {};
    relocB.r_symbol[0] = 'B';
    relocB.r_offset = 8;
    relocB.r_type = vISA::GenRelocType::R_SYM_ADDR;

    vISA::GenRelocEntry relocC = {};
    relocC.r_symbol[0] = 'C';
    relocC.r_offset = 16;
    relocC.r_type = vISA::GenRelocType::R_SYM_ADDR;

    vISA::GenRelocEntry relocCPartHigh = {};
    relocCPartHigh.r_symbol[0] = 'C';
    relocCPartHigh.r_offset = 28;
    relocCPartHigh.r_type = vISA::GenRelocType::R_SYM_ADDR_32_HI;

    vISA::GenRelocEntry relocCPartLow = {};
    relocCPartLow.r_symbol[0] = 'C';
    relocCPartLow.r_offset = 36;
    relocCPartLow.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry relocPerThreadPayloadOffset = {};
    relocPerThreadPayloadOffset.r_symbol[0] = 'X';
    relocPerThreadPayloadOffset.r_offset = 44;
    relocPerThreadPayloadOffset.r_type = vISA::GenRelocType::R_PER_THREAD_PAYLOAD_OFFSET_32;

    vISA::GenRelocEntry relocs[] = {relocA, relocB, relocC, relocCPartHigh, relocCPartLow, relocPerThreadPayloadOffset};
    constexpr uint32_t numRelocations = sizeof(relocs) / sizeof(relocs[0]);
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 1);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 128;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> exportedFuncSegmentData(exportedFuncSegment.segmentSize, 0);

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(64, static_cast<char>(initData));
    NEO::Linker::PatchableSegments patchableInstructionSegments(2);
    patchableInstructionSegments[0].hostPointer = exportedFuncSegmentData.data();
    patchableInstructionSegments[0].gpuAddress = exportedFuncSegment.gpuAddress;
    patchableInstructionSegments[0].segmentSize = exportedFuncSegment.segmentSize;

    patchableInstructionSegments[1].hostPointer = instructionSegment.data();
    patchableInstructionSegments[1].gpuAddress = 0x0;
    patchableInstructionSegments[1].segmentSize = instructionSegment.size();

    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    kernelDescriptors.resize(2);
    KernelDescriptor dummyKernelDescriptor;
    KernelDescriptor kd;
    kd.kernelAttributes.crossThreadDataSize = 0x20;
    kernelDescriptors[0] = &dummyKernelDescriptor;
    kernelDescriptors[1] = &kd;

    auto linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals,
        pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(3U, relocatedSymbols.size());

    ASSERT_EQ(1U, relocatedSymbols.count(symGlobalVariable.s_name));
    ASSERT_EQ(1U, relocatedSymbols.count(symGlobalConstant.s_name));
    ASSERT_EQ(1U, relocatedSymbols.count(symExportedFunc.s_name));

    EXPECT_EQ(relocatedSymbols[symGlobalVariable.s_name].gpuAddress, globalVarSegment.gpuAddress + symGlobalVariable.s_offset);
    EXPECT_EQ(relocatedSymbols[symGlobalConstant.s_name].gpuAddress, globalConstSegment.gpuAddress + symGlobalConstant.s_offset);
    EXPECT_EQ(relocatedSymbols[symExportedFunc.s_name].gpuAddress, exportedFuncSegment.gpuAddress + symExportedFunc.s_offset);

    EXPECT_EQ(relocatedSymbols[symGlobalVariable.s_name].gpuAddress, *reinterpret_cast<const uintptr_t *>(instructionSegment.data() + relocA.r_offset));
    EXPECT_EQ(relocatedSymbols[symGlobalConstant.s_name].gpuAddress, *reinterpret_cast<const uintptr_t *>(instructionSegment.data() + relocB.r_offset));
    EXPECT_EQ(relocatedSymbols[symExportedFunc.s_name].gpuAddress, *reinterpret_cast<const uintptr_t *>(instructionSegment.data() + relocC.r_offset));

    auto funcGpuAddressAs64bit = static_cast<uint64_t>(relocatedSymbols[symExportedFunc.s_name].gpuAddress);
    auto funcAddressLow = static_cast<uint32_t>(funcGpuAddressAs64bit & 0xffffffff);
    auto funcAddressHigh = static_cast<uint32_t>((funcGpuAddressAs64bit >> 32) & 0xffffffff);
    EXPECT_EQ(funcAddressLow, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartLow.r_offset));
    EXPECT_EQ(initData, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartLow.r_offset - sizeof(uint32_t)));
    EXPECT_EQ(initData, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartLow.r_offset + sizeof(uint32_t)));

    EXPECT_EQ(funcAddressHigh, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartHigh.r_offset));
    EXPECT_EQ(initData, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartHigh.r_offset - sizeof(uint32_t)));
    EXPECT_EQ(initData, *reinterpret_cast<const uint32_t *>(instructionSegment.data() + relocCPartHigh.r_offset + sizeof(uint32_t)));

    auto perThreadPayloadOffsetPatchedValue = *reinterpret_cast<uint32_t *>(instructionSegment.data() + relocPerThreadPayloadOffset.r_offset);
    EXPECT_EQ(kd.kernelAttributes.crossThreadDataSize, perThreadPayloadOffsetPatchedValue);
}

HWTEST_F(LinkerTests, givenInvalidSymbolOffsetWhenPatchingInstructionsThenRelocationFails) {
    NEO::LinkerInput linkerInput;

    vISA::GenSymEntry symGlobalVariable = {};
    symGlobalVariable.s_name[0] = 'A';
    symGlobalVariable.s_offset = 64;
    symGlobalVariable.s_size = 16;
    symGlobalVariable.s_type = vISA::GenSymType::S_GLOBAL_VAR;
    bool decodeSymSuccess = linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalVariable, 1);
    EXPECT_TRUE(decodeSymSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = symGlobalVariable.s_offset;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;

    auto linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::error, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals,
        pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
}

HWTEST_F(LinkerTests, givenInvalidRelocationOffsetThenPatchingOfInstructionsFails) {
    NEO::LinkerInput linkerInput;

    vISA::GenSymEntry symGlobalVariable = {};
    symGlobalVariable.s_name[0] = 'A';
    symGlobalVariable.s_offset = 64;
    symGlobalVariable.s_size = 16;
    symGlobalVariable.s_type = vISA::GenSymType::S_GLOBAL_VAR;
    bool decodeSymSuccess = linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalVariable, 1);
    EXPECT_TRUE(decodeSymSuccess);

    vISA::GenRelocEntry relocA = {};
    relocA.r_symbol[0] = 'A';
    relocA.r_offset = 32;
    relocA.r_type = vISA::GenRelocType::R_SYM_ADDR;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocA, 1, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    std::vector<char> instructionSegment;
    instructionSegment.resize(relocA.r_offset + sizeof(uint64_t), 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = relocA.r_offset;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;

    auto linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedPartially, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(1U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_TRUE(unresolvedExternals[0].internalError);

    patchableInstructionSegments[0].segmentSize = relocA.r_offset + sizeof(uint64_t);
    linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
}

HWTEST_F(LinkerTests, givenUnknownSymbolTypeWhenPatchingInstructionsThenRelocationFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;

    vISA::GenSymEntry symGlobalVariable = {};
    symGlobalVariable.s_name[0] = 'A';
    symGlobalVariable.s_offset = 0;
    symGlobalVariable.s_size = 16;
    symGlobalVariable.s_type = vISA::GenSymType::S_GLOBAL_VAR;
    bool decodeSymSuccess = linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalVariable, 1);
    EXPECT_TRUE(decodeSymSuccess);

    vISA::GenRelocEntry relocA = {};
    relocA.r_symbol[0] = 'A';
    relocA.r_offset = 0;
    relocA.r_type = vISA::GenRelocType::R_SYM_ADDR;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocA, 1, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;

    ASSERT_EQ(1U, linkerInput.symbols.count("A"));
    linkerInput.symbols["A"].segment = NEO::SegmentType::unknown;
    auto linkResult = linker.link(
        globalVarSegment, globalConstSegment, exportedFuncSegment, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::error, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(0U, unresolvedExternals.size());

    linkerInput.symbols["A"].segment = NEO::SegmentType::globalVariables;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                             unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
}

HWTEST_F(LinkerTests, givenValidStringSymbolsAndRelocationsWhenPatchingThenItIsProperlyPatched) {
    NEO::Linker::SegmentInfo stringSegment;
    stringSegment.gpuAddress = 0x1234;
    stringSegment.segmentSize = 0x10;

    uint8_t instructionSegment[32] = {};
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment;
    seg0.segmentSize = sizeof(instructionSegment);
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    WhiteBox<NEO::LinkerInput> linkerInput;

    SymbolInfo strSymbol;
    strSymbol.segment = SegmentType::globalStrings;
    strSymbol.offset = 0U;
    strSymbol.size = 8U;
    linkerInput.symbols.insert({".str", strSymbol});

    NEO::LinkerInput::RelocationInfo relocation;
    relocation.offset = 0x8U;
    relocation.relocationSegment = NEO::SegmentType::instructions;
    relocation.symbolName = ".str";
    relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.textRelocations.push_back({relocation});

    linkerInput.traits.requiresPatchingOfInstructionSegments = true;

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(
        {}, {}, {}, stringSegment,
        nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
        pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_TRUE(linker.extractRelocatedSymbols().empty());

    uintptr_t strAddr = static_cast<uintptr_t>(stringSegment.gpuAddress) + static_cast<uintptr_t>(strSymbol.offset);
    uintptr_t patchAddr = reinterpret_cast<uintptr_t>(instructionSegment) + static_cast<uintptr_t>(relocation.offset);

    EXPECT_EQ(static_cast<size_t>(strAddr), *reinterpret_cast<const size_t *>(patchAddr));
}

TEST_F(LinkerTests, givenValidStringSymbolsAndRelocationsWithWordMisalignedOffsetWhenPatchingThenItIsProperlyPatched) {
    NEO::Linker::SegmentInfo stringSegment;
    stringSegment.gpuAddress = 0x1234000000567800;
    stringSegment.segmentSize = 0x20;

    uint8_t instructionSegment[32] = {};
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment;
    seg0.segmentSize = sizeof(instructionSegment);
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0, seg0, seg0, seg0};

    WhiteBox<NEO::LinkerInput> linkerInput;

    SymbolInfo strSymbol;
    strSymbol.segment = SegmentType::globalStrings;
    strSymbol.offset = 0U;
    strSymbol.size = 8U;
    linkerInput.symbols.insert({".str8", strSymbol});

    strSymbol.offset = 8U;
    strSymbol.size = 4U;
    linkerInput.symbols.insert({".str4H", strSymbol});

    strSymbol.offset = 12U;
    strSymbol.size = 4U;
    linkerInput.symbols.insert({".str4L", strSymbol});

    strSymbol.offset = 16U;
    strSymbol.size = 2U;
    linkerInput.symbols.insert({".str2", strSymbol});

    NEO::LinkerInput::RelocationInfo relocation;
    relocation.offset = 1U;
    relocation.relocationSegment = NEO::SegmentType::instructions;
    relocation.symbolName = ".str8";
    relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.textRelocations.push_back({relocation});

    relocation.offset = 9U;
    relocation.relocationSegment = NEO::SegmentType::instructions;
    relocation.symbolName = ".str4H";
    relocation.type = NEO::LinkerInput::RelocationInfo::Type::addressHigh;
    linkerInput.textRelocations.push_back({relocation});

    relocation.offset = 13U;
    relocation.relocationSegment = NEO::SegmentType::instructions;
    relocation.symbolName = ".str4L";
    relocation.type = NEO::LinkerInput::RelocationInfo::Type::addressLow;
    linkerInput.textRelocations.push_back({relocation});

    relocation.offset = 17U;
    relocation.relocationSegment = NEO::SegmentType::instructions;
    relocation.symbolName = ".str2";
    relocation.type = NEO::LinkerInput::RelocationInfo::Type::address16;
    linkerInput.textRelocations.push_back({relocation});

    linkerInput.traits.requiresPatchingOfInstructionSegments = true;

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(
        {}, {}, {}, stringSegment,
        nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
        pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_TRUE(linker.extractRelocatedSymbols().empty());

    uint64_t str8Addr = static_cast<uint64_t>(stringSegment.gpuAddress);
    uint32_t str4HAddr = static_cast<uint32_t>((stringSegment.gpuAddress + 8u) >> 32);
    uint32_t str4LAddr = static_cast<uint32_t>((stringSegment.gpuAddress + 12u) & 0xffffffff);
    uint16_t str2Addr = static_cast<uint16_t>((stringSegment.gpuAddress + 16u) & 0xffff);

    EXPECT_NE(0u, str8Addr);
    EXPECT_NE(0u, str4HAddr);
    EXPECT_NE(0u, str4LAddr);
    EXPECT_NE(0u, str2Addr);

    auto patchAddrStr8 = ptrOffset(instructionSegment, 1u);
    auto patchAddrStr4H = ptrOffset(instructionSegment, 9u);
    auto patchAddrStr4L = ptrOffset(instructionSegment, 13u);
    auto patchAddrStr2 = ptrOffset(instructionSegment, 17u);

    EXPECT_EQ(0, memcmp(patchAddrStr8, &str8Addr, 8));
    EXPECT_EQ(0, memcmp(patchAddrStr4H, &str4HAddr, 4));
    EXPECT_EQ(0, memcmp(patchAddrStr4L, &str4LAddr, 4));
    EXPECT_EQ(0, memcmp(patchAddrStr2, &str2Addr, 2));
}

HWTEST_F(LinkerTests, givenValidSymbolsAndRelocationsWhenPatchingDataSegmentsThenTheyAreProperlyPatched) {
    uint64_t initGlobalConstantData[3];
    initGlobalConstantData[0] = 0x10;   // var1 address will be added here
    initGlobalConstantData[1] = 0x1234; // <- const1
    initGlobalConstantData[2] = 0x0;    // fun1 address will be added here

    uint64_t initGlobalVariablesData[3];
    initGlobalVariablesData[0] = 0x20;   // const1 address will be added here
    initGlobalVariablesData[1] = 0x4321; // <- var1
    initGlobalVariablesData[2] = 0x0;    // fun2 address will be added here

    uint64_t exportedFunctionsInit[2];
    exportedFunctionsInit[0] = 0x12; // <- fun1
    exportedFunctionsInit[1] = 0x34; // <- fun2

    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{initGlobalConstantData, sizeof(initGlobalConstantData)};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{initGlobalVariablesData, sizeof(initGlobalVariablesData)};
    NEO::MockGraphicsAllocation exportedFunctionsMockGA{&exportedFunctionsInit, sizeof(exportedFunctionsInit)};

    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo, exportedFunctionsSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    exportedFunctionsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(exportedFunctionsMockGA.getUnderlyingBuffer());
    exportedFunctionsSegmentInfo.segmentSize = exportedFunctionsMockGA.getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &fun1 = linkerInput.symbols["fun1"];
    fun1.global = true;
    fun1.segment = SegmentType::instructions;
    fun1.offset = 0U;
    fun1.size = 8U;

    auto &fun2 = linkerInput.symbols["fun2"];
    fun2.global = true;
    fun2.segment = SegmentType::instructions;
    fun2.offset = 8U;
    fun2.size = 8U;

    auto &var1 = linkerInput.symbols["var1"];
    var1.global = true;
    var1.segment = SegmentType::globalVariables;
    var1.offset = 8U;
    var1.size = 8U;

    auto &const1 = linkerInput.symbols["const1"];
    const1.global = true;
    const1.segment = SegmentType::globalConstants;
    const1.offset = 8U;
    const1.size = 8U;

    /*
    Segments:
    Const:
    0x00    0x10
    0x08    0x1234 <- const1
    0x10    0x0

    Var:
    0x00    0x20
    0x08    0x4321 <- var1
    0x10    0x0

    ExportFun:
    0x00    0x12 <- fun1
    0x08    0x34 <- fun2

    After patching:
    Const:
    0x00    0x10 + &var1
    0x08    0x1234
    0x10    0x0 + &fun1

    Var:
    0x00    0x20 + &const1
    0x08    0x4321
    0x10    0x0 + &fun2
    */

    // var1 -> Constant
    // *(uint64_t*)(constant + 0) += *(uint64_t*)(var + 8)
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::globalConstants;
        relocation.symbolName = "var1";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.dataRelocations.push_back(relocation);
    }

    // const1 -> Var
    // *(uint64_t*)(var + 0) += *(uint64_t*)(const + 8)
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::globalVariables;
        relocation.symbolName = "const1";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.dataRelocations.push_back(relocation);
    }
    // fun1 -> Const
    // *(uint64_t*)(const + 0x10) += *(uint64_t*)(export_fun + 0)
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0x10U;
        relocation.relocationSegment = NEO::SegmentType::globalConstants;
        relocation.symbolName = "fun1";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.dataRelocations.push_back(relocation);
    }

    if (LinkerInput::Traits::PointerSize::Ptr64bit == linkerInput.getTraits().pointerSize) {
        // fun2_LO -> var
        // *(uint32_t*)(var + 0x10) += *(uint32_t*)((export_fun + 8) & 0xffffffff)
        {
            NEO::LinkerInput::RelocationInfo relocation;
            relocation.offset = 0x10U;
            relocation.relocationSegment = NEO::SegmentType::globalVariables;
            relocation.symbolName = "fun2";
            relocation.type = NEO::LinkerInput::RelocationInfo::Type::addressLow;
            linkerInput.dataRelocations.push_back(relocation);
        }
        // fun2_HI -> var
        // *(uint32_t*)(var + 0x14) += *(uint32_t*)(((export_fun + 8) >> 32) & 0xffffffff)
        {
            NEO::LinkerInput::RelocationInfo relocation;
            relocation.offset = 0x14U;
            relocation.relocationSegment = NEO::SegmentType::globalVariables;
            relocation.symbolName = "fun2";
            relocation.type = NEO::LinkerInput::RelocationInfo::Type::addressHigh;
            linkerInput.dataRelocations.push_back(relocation);
        }
    }

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, exportedFunctionsSegmentInfo, {},
                                  globalVariablesPatchableSegment.get(), globalConstantsPatchableSegment.get(), {},
                                  unresolvedExternals, pDevice, initGlobalConstantData, sizeof(initGlobalConstantData),
                                  initGlobalVariablesData, sizeof(initGlobalVariablesData), kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());

    auto constantsAddr = globalConstantsSegmentInfo.gpuAddress;
    auto varsAddr = globalVariablesSegmentInfo.gpuAddress;
    auto exportFunAddr = exportedFunctionsSegmentInfo.gpuAddress;

    auto const1Addr = constantsAddr + const1.offset;
    auto var1Addr = varsAddr + var1.offset;
    auto fun1Addr = exportFunAddr + fun1.offset;
    auto fun2Addr = exportFunAddr + fun2.offset;

    if (LinkerInput::Traits::PointerSize::Ptr64bit == linkerInput.getTraits().pointerSize) {
        EXPECT_EQ(0x10U + var1Addr, *(uint64_t *)(constantsAddr));
        EXPECT_EQ(0x1234U, *(uint64_t *)(constantsAddr + 0x8));
        EXPECT_EQ(fun1Addr, *(uint64_t *)(constantsAddr + 0x10));

        EXPECT_EQ(0x20U + const1Addr, *(uint64_t *)(varsAddr));
        EXPECT_EQ(0x4321U, *(uint64_t *)(varsAddr + 0x8));
        EXPECT_EQ(fun2Addr, *(uint64_t *)(varsAddr + 0x10));
    } else if (LinkerInput::Traits::PointerSize::Ptr32bit == linkerInput.getTraits().pointerSize) {
        EXPECT_EQ(0x10U + var1Addr, *(uint32_t *)(constantsAddr));
        EXPECT_EQ(0x1234U, *(uint32_t *)(constantsAddr + 0x8));
        EXPECT_EQ(fun1Addr, *(uint32_t *)(constantsAddr + 0x10));

        EXPECT_EQ(0x20U + const1Addr, *(uint32_t *)(varsAddr));
    }
}

HWTEST_F(LinkerTests, givenValidSymbolsAndRelocationsToBssDataSectionsWhenPatchingDataSegmentsThenTheyAreProperlyPatched) {
    uint64_t initGlobalConstantData[] = {0x1234};  //<- const1 - initValue should be ignored
    uint64_t initGlobalVariablesData[] = {0x4321}; // <- var1 - initValue should be ignored

    uint64_t constantsSegmentData[2]{0};       // size 2 * uint64_t - contains also bss at the end
    uint64_t globalVariablesSegmentData[2]{0}; // size 2 * uint64_t - contains also bss at the end

    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{constantsSegmentData, sizeof(constantsSegmentData)};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{globalVariablesSegmentData, sizeof(globalVariablesSegmentData)};
    globalConstantsPatchableSegmentMockGA.gpuAddress = 0xA0000000;
    globalVariablesPatchableSegmentMockGA.gpuAddress = 0xB0000000;

    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = static_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getGpuAddress());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    globalVariablesSegmentInfo.gpuAddress = static_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getGpuAddress());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    auto setUpInstructionSeg = [](std::vector<uint8_t> &instrData, NEO::Linker::PatchableSegments &patchableInstrSeg) -> void {
        uint64_t initData = 0x77777777;
        instrData.resize(8, static_cast<uint8_t>(initData));

        auto &emplaced = patchableInstrSeg.emplace_back();
        emplaced.hostPointer = instrData.data();
        emplaced.segmentSize = instrData.size();
    };
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    std::vector<uint8_t> instructionsData1, instructionsData2;
    setUpInstructionSeg(instructionsData1, patchableInstructionSegments);
    setUpInstructionSeg(instructionsData2, patchableInstructionSegments);

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfInstructionSegments = true;

    auto &var1 = linkerInput.symbols["var1"];
    var1.segment = SegmentType::globalVariables;
    var1.offset = 0U;
    var1.size = 8U;

    auto &bssVar = linkerInput.symbols["bssVar"];
    bssVar.segment = SegmentType::globalVariablesZeroInit;
    bssVar.offset = 0U;
    bssVar.size = 8U;

    auto &const1 = linkerInput.symbols["const1"];
    const1.segment = SegmentType::globalConstants;
    const1.offset = 0U;
    const1.size = 8U;

    auto &bssConst = linkerInput.symbols["bssConst"];
    bssConst.segment = SegmentType::globalConstantsZeroInit;
    bssConst.offset = 0U;
    bssConst.size = 8U;

    /*
    Segments:
    Const:
    0x00    0x1000 <- const 1
    0x08    0x0 <- bss

    Var:
    0x00    0x4000 <- var 1
    0x08    0x0 <- bss

    Instructions:
    0x0     0x0 <- will be patched with bss.const
    0x08    0x0 <- will be patched with bss.global

    1. Patch bss data from const segment with var 1
       Patch bss data from global variables segment with const 1
    2. Patch const 2 with symbol pointing to bss in const (patched in step 1).
       Patch var 2 with symbol pointing to bss in variables (patched in step 1).
    */

    // Relocations for step 1.
    // bssConst[0] = &var1
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::globalConstantsZeroInit;
        relocation.symbolName = "var1";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.dataRelocations.push_back(relocation);
    }
    // bssGlobal[0] = &const1
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::globalVariablesZeroInit;
        relocation.symbolName = "const1";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.dataRelocations.push_back(relocation);
    }
    // Relocation for step 2.
    // instructions[0] = &bssConst
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::instructions;
        relocation.symbolName = "bssConst";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.textRelocations.push_back({relocation});
    }
    // instructions[1] = &bssVar
    {
        NEO::LinkerInput::RelocationInfo relocation;
        relocation.offset = 0U;
        relocation.relocationSegment = NEO::SegmentType::instructions;
        relocation.symbolName = "bssVar";
        relocation.type = NEO::LinkerInput::RelocationInfo::Type::address;
        linkerInput.textRelocations.push_back({relocation});
    }

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {}, {},
                                  globalVariablesPatchableSegment.get(), globalConstantsPatchableSegment.get(), patchableInstructionSegments,
                                  unresolvedExternals, pDevice, initGlobalConstantData, sizeof(initGlobalConstantData),
                                  initGlobalVariablesData, sizeof(initGlobalVariablesData), kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());

    auto globalConstantsSegmentAddr = reinterpret_cast<uint64_t *>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    auto globalVariableSegmentAddr = reinterpret_cast<uint64_t *>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());

    auto var1Addr = globalVariablesPatchableSegment->getGraphicsAllocation()->getGpuAddress();
    auto const1Addr = globalConstantsPatchableSegment->getGraphicsAllocation()->getGpuAddress();
    auto bssConstAddrr = globalConstantsPatchableSegment->getGraphicsAllocation()->getGpuAddress() + sizeof(initGlobalConstantData);
    auto bssVarAddr = globalVariablesPatchableSegment->getGraphicsAllocation()->getGpuAddress() + sizeof(initGlobalVariablesData);

    EXPECT_EQ(var1Addr, *(globalConstantsSegmentAddr + 1));
    EXPECT_EQ(const1Addr, *(globalVariableSegmentAddr + 1));
    EXPECT_EQ(bssConstAddrr, *(reinterpret_cast<uint64_t *>(instructionsData1.data())));
    EXPECT_EQ(bssVarAddr, *(reinterpret_cast<uint64_t *>(instructionsData2.data())));
}

HWTEST_F(LinkerTests, givenInvalidSymbolWhenPatchingDataSegmentsThenRelocationIsUnresolved) {
    uint64_t initGlobalConstantData[3] = {};
    uint64_t initGlobalVariablesData[3] = {};

    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{initGlobalConstantData, sizeof(initGlobalConstantData)};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{initGlobalVariablesData, sizeof(initGlobalVariablesData)};

    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = 0U;
    relocationInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocationInfo.symbolName = "symbol";
    relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.dataRelocations.push_back(relocationInfo);

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {}, {},
                                  globalVariablesPatchableSegment.get(), globalConstantsPatchableSegment.get(), {},
                                  unresolvedExternals, pDevice, initGlobalConstantData, sizeof(initGlobalConstantData), initGlobalVariablesData,
                                  sizeof(initGlobalVariablesData), kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());
}

HWTEST_F(LinkerTests, givenInvalidRelocationOffsetWhenPatchingDataSegmentsThenRelocationIsUnresolved) {
    uint64_t initGlobalConstantData[3] = {};
    uint64_t initGlobalVariablesData[3] = {};

    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{initGlobalConstantData, sizeof(initGlobalConstantData)};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{initGlobalVariablesData, sizeof(initGlobalVariablesData)};

    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &symbol = linkerInput.symbols["symbol"];
    symbol.segment = SegmentType::globalVariables;
    symbol.offset = 0U;
    symbol.size = 8U;

    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = globalConstantsSegmentInfo.segmentSize + 1U;
    relocationInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocationInfo.symbolName = "symbol";
    relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.dataRelocations.push_back(relocationInfo);

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {}, {},
                                  globalVariablesPatchableSegment.get(), globalConstantsPatchableSegment.get(), {},
                                  unresolvedExternals, pDevice, initGlobalConstantData, sizeof(initGlobalConstantData),
                                  initGlobalVariablesData, sizeof(initGlobalVariablesData), kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());
}

HWTEST_F(LinkerTests, givenInvalidRelocationSegmentWhenPatchingDataSegmentsThenRelocationIsUnresolved) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    auto &symbol = linkerInput.symbols["symbol"];
    symbol.segment = SegmentType::globalVariables;
    symbol.offset = 0U;
    symbol.size = 8U;
    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = 0U;
    relocationInfo.relocationSegment = NEO::SegmentType::unknown;
    relocationInfo.symbolName = "symbol";
    relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.dataRelocations.push_back(relocationInfo);
    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;

    NEO::Linker::SegmentInfo globalSegment;
    globalSegment.segmentSize = 8u;
    auto linkResult = linker.link(globalSegment, {}, {}, {},
                                  nullptr, nullptr, {},
                                  unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());
}

HWTEST_F(LinkerTests, given32BitBinaryWithValidSymbolsAndRelocationsWhenPatchingDataSegmentsThenTreatAddressRelocationAsLowerHalfOfTheAddress) {
    uint64_t initGlobalConstantData[3] = {};
    uint64_t initGlobalVariablesData[3] = {};

    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{initGlobalConstantData, sizeof(initGlobalConstantData)};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{initGlobalVariablesData, sizeof(initGlobalVariablesData)};

    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.setPointerSize(NEO::LinkerInput::Traits::PointerSize::Ptr32bit);
    auto &fun1 = linkerInput.symbols["symbol"];
    fun1.segment = SegmentType::globalVariables;
    fun1.offset = 0U;
    fun1.size = 8U;

    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = 0U;
    relocationInfo.relocationSegment = NEO::SegmentType::globalConstants;
    relocationInfo.symbolName = "symbol";
    relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    linkerInput.dataRelocations.push_back(relocationInfo);

    NEO::Linker linker(linkerInput);
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {}, {},
                                  globalVariablesPatchableSegment.get(), globalConstantsPatchableSegment.get(), {},
                                  unresolvedExternals, pDevice, initGlobalConstantData, sizeof(initGlobalConstantData), initGlobalVariablesData,
                                  sizeof(initGlobalVariablesData), kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());

    EXPECT_EQ(globalVariablesSegmentInfo.gpuAddress & 0xffffffff, *(uint32_t *)(globalConstantsSegmentInfo.gpuAddress));
    EXPECT_EQ(0U, *(uint32_t *)(globalConstantsSegmentInfo.gpuAddress + 4));
}

TEST(LinkerErrorMessageTests, whenListOfUnresolvedExternalsIsEmptyThenErrorTypeDefaultsToInternalError) {
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    std::vector<std::string> segmentsNames{"kernel1", "kernel2"};
    auto err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);
    EXPECT_EQ(std::string("Internal linker error"), err);
}

TEST(LinkerErrorMessageTests, givenListOfUnresolvedExternalsThenSymbolNameOrSymbolSegmentTypeGetsEmbededInErrorMessage) {
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    std::vector<std::string> segmentsNames{"kernel1", "kernel2"};
    NEO::Linker::UnresolvedExternal unresolvedExternal = {};
    unresolvedExternal.instructionsSegmentId = 1;
    unresolvedExternal.internalError = false;
    unresolvedExternal.unresolvedRelocation.offset = 64;
    unresolvedExternal.unresolvedRelocation.symbolName = "arrayABC";
    unresolvedExternal.unresolvedRelocation.relocationSegment = NEO::SegmentType::instructions;
    unresolvedExternals.push_back(unresolvedExternal);
    auto err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);

    EXPECT_TRUE(hasSubstr(err, unresolvedExternal.unresolvedRelocation.symbolName));
    EXPECT_TRUE(hasSubstr(err, segmentsNames[unresolvedExternal.instructionsSegmentId]));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));
    EXPECT_FALSE(hasSubstr(err, "internal error"));

    unresolvedExternals[0].internalError = true;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);
    EXPECT_TRUE(hasSubstr(err, unresolvedExternal.unresolvedRelocation.symbolName));
    EXPECT_TRUE(hasSubstr(err, segmentsNames[unresolvedExternal.instructionsSegmentId]));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));
    EXPECT_TRUE(hasSubstr(err, "internal linker error"));

    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_TRUE(hasSubstr(err, unresolvedExternal.unresolvedRelocation.symbolName));
    EXPECT_FALSE(hasSubstr(err, segmentsNames[unresolvedExternal.instructionsSegmentId]));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));
    EXPECT_TRUE(hasSubstr(err, "internal linker error"));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::globalConstants;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_TRUE(hasSubstr(err, NEO::asString(NEO::SegmentType::globalConstants)));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::globalVariables;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_TRUE(hasSubstr(err, NEO::asString(NEO::SegmentType::globalVariables)));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::unknown;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_TRUE(hasSubstr(err, NEO::asString(NEO::SegmentType::unknown)));
    EXPECT_TRUE(hasSubstr(err, std::to_string(unresolvedExternal.unresolvedRelocation.offset)));
}

TEST(RelocationsDebugMessageTests, givenEmptyListOfRelocatedSymbolsThenReturnsEmptyString) {
    auto message = NEO::constructRelocationsDebugMessage({});
    EXPECT_EQ(0U, message.size()) << message;
}

TEST(RelocationsDebugMessageTests, givenListOfRelocatedSymbolsThenReturnProperDebugMessage) {
    NEO::Linker::RelocatedSymbolsMap symbols;

    auto &funcSymbol = symbols["foo"];
    auto &constDataSymbol = symbols["constInt"];
    auto &globalVarSymbol = symbols["intX"];
    funcSymbol.symbol.segment = NEO::SegmentType::instructions;
    funcSymbol.symbol.offset = 64U;
    funcSymbol.symbol.size = 1024U;
    funcSymbol.gpuAddress = 4096U;

    constDataSymbol.symbol.segment = NEO::SegmentType::globalConstants;
    constDataSymbol.symbol.offset = 32U;
    constDataSymbol.symbol.size = 16U;
    constDataSymbol.gpuAddress = 8U;

    globalVarSymbol.symbol.segment = NEO::SegmentType::globalVariables;
    globalVarSymbol.symbol.offset = 72U;
    globalVarSymbol.symbol.size = 8U;
    globalVarSymbol.gpuAddress = 256U;

    auto message = NEO::constructRelocationsDebugMessage(symbols);

    std::stringstream expected;
    expected << "Relocations debug information :\n";
    for (const auto &symbol : symbols) {
        if (symbol.first == "foo") {
            expected << " * \"foo\" [1024 bytes] INSTRUCTIONS_SEGMENT@64 -> 0x1000 GPUVA\n";
        } else if (symbol.first == "constInt") {
            expected << " * \"constInt\" [16 bytes] GLOBAL_CONSTANTS_SEGMENT@32 -> 0x8 GPUVA\n";
        } else {
            expected << " * \"intX\" [8 bytes] GLOBAL_VARIABLES_SEGMENT@72 -> 0x100 GPUVA\n";
        }
    }
    EXPECT_STREQ(expected.str().c_str(), message.c_str());
}

TEST_F(LinkerTests, GivenDebugDataWhenApplyingDebugDataRelocationsThenRelocationsAreAppliedInMemory) {
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    header.shOff = header.ehSize;
    header.shNum = 4;
    header.shStrNdx = 0;

    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> sectionHeader0;
    sectionHeader0.size = 80;
    sectionHeader0.offset = 80;

    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> sectionHeader1;
    sectionHeader1.size = 80;
    sectionHeader1.offset = 160;

    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> sectionHeader2;
    sectionHeader2.size = 80;
    sectionHeader2.offset = 240;

    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> sectionHeader3;
    sectionHeader3.size = 80;
    sectionHeader3.offset = 320;

    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> sectionHeader4;
    sectionHeader4.size = 80;
    sectionHeader4.offset = 400;

    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    uint64_t dummyData[100];
    uint8_t *storage = reinterpret_cast<uint8_t *>(dummyData);

    elf64.sectionHeaders.push_back({&sectionHeader0, {&storage[80], 80}});
    elf64.sectionHeaders.push_back({&sectionHeader1, {&storage[160], 80}});
    elf64.sectionHeaders.push_back({&sectionHeader2, {&storage[240], 80}});
    elf64.sectionHeaders.push_back({&sectionHeader3, {&storage[320], 80}});
    elf64.sectionHeaders.push_back({&sectionHeader4, {&storage[400], 80}});

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text";
    sectionNames[1] = ".data.global";
    sectionNames[2] = ".debug_info";
    sectionNames[3] = ".debug_abbrev";
    sectionNames[4] = ".debug_line";
    sectionNames[5] = ".data.const";

    elf64.setupSectionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc0 = {};
    reloc0.offset = 64;
    reloc0.relocType = static_cast<uint32_t>(Elf::RelocationX8664Type::relocation64);
    reloc0.symbolName = ".debug_abbrev";
    reloc0.symbolSectionIndex = 3;
    reloc0.symbolTableIndex = 0;
    reloc0.targetSectionIndex = 2;
    reloc0.addend = 0;

    elf64.debugInfoRelocations.emplace_back(reloc0);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc1 = {};
    reloc1.offset = 32;
    reloc1.relocType = static_cast<uint32_t>(Elf::RelocationX8664Type::relocation32);
    reloc1.symbolName = ".debug_line";
    reloc1.symbolSectionIndex = 4;
    reloc1.symbolTableIndex = 0;
    reloc1.targetSectionIndex = 2;
    reloc1.addend = 4;

    elf64.debugInfoRelocations.emplace_back(reloc1);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2 = {};
    reloc2.offset = 32;
    reloc2.relocType = static_cast<uint32_t>(Elf::RelocationX8664Type::relocation64);
    reloc2.symbolName = ".text";
    reloc2.symbolSectionIndex = 0;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 4;
    reloc2.addend = 18;

    elf64.debugInfoRelocations.emplace_back(reloc2);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc3 = {};
    reloc3.offset = 0;
    reloc3.relocType = static_cast<uint32_t>(Elf::RelocationX8664Type::relocation64);
    reloc3.symbolName = ".data";
    reloc3.symbolSectionIndex = 1;
    reloc3.symbolTableIndex = 0;
    reloc3.targetSectionIndex = 4;
    reloc3.addend = 55;

    elf64.debugInfoRelocations.emplace_back(reloc3);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc4 = {};
    reloc4.offset = 8;
    reloc4.relocType = static_cast<uint32_t>(0);
    reloc4.symbolName = ".text";
    reloc4.symbolSectionIndex = 0;
    reloc4.symbolTableIndex = 0;
    reloc4.targetSectionIndex = 4;
    reloc4.addend = 77;

    elf64.debugInfoRelocations.emplace_back(reloc4);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc5 = {};
    reloc5.offset = 16;
    reloc5.relocType = static_cast<uint32_t>(1);
    reloc5.symbolName = ".data.const";
    reloc5.symbolSectionIndex = 5;
    reloc5.symbolTableIndex = 0;
    reloc5.targetSectionIndex = 4;
    reloc5.addend = 20;

    elf64.debugInfoRelocations.emplace_back(reloc5);

    uint64_t *relocInDebugLine = reinterpret_cast<uint64_t *>(&storage[400]);
    *relocInDebugLine = 0;
    uint64_t *relocInDebugLine2 = reinterpret_cast<uint64_t *>(&storage[408]);
    *relocInDebugLine2 = 0;
    uint64_t *relocInDebugLine5 = reinterpret_cast<uint64_t *>(&storage[416]);
    *relocInDebugLine5 = 0;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol;
    symbol.value = 0;
    elf64.symbolTable.push_back(symbol);

    NEO::Linker::SegmentInfo text = {static_cast<uintptr_t>(0x80001000), 0x10000};
    NEO::Linker::SegmentInfo dataGlobal = {static_cast<uintptr_t>(0x123000), 0x10000};
    NEO::Linker::SegmentInfo dataConst = {static_cast<uintptr_t>(0xabc000), 0x10000};

    NEO::Linker::applyDebugDataRelocations(elf64, {storage, sizeof(dummyData)}, text, dataGlobal, dataConst);

    auto reloc0Location = reinterpret_cast<const uint64_t *>(&elf64.sectionHeaders[reloc0.targetSectionIndex].data[static_cast<size_t>(reloc0.offset)]);
    auto reloc1Location = reinterpret_cast<const uint32_t *>(&elf64.sectionHeaders[reloc1.targetSectionIndex].data[static_cast<size_t>(reloc1.offset)]);
    auto reloc2Location = reinterpret_cast<const uint64_t *>(&elf64.sectionHeaders[reloc2.targetSectionIndex].data[static_cast<size_t>(reloc2.offset)]);
    auto reloc3Location = reinterpret_cast<const uint64_t *>(&elf64.sectionHeaders[reloc3.targetSectionIndex].data[static_cast<size_t>(reloc3.offset)]);
    auto reloc4Location = reinterpret_cast<const uint64_t *>(&elf64.sectionHeaders[reloc4.targetSectionIndex].data[static_cast<size_t>(reloc4.offset)]);
    auto reloc5Location = reinterpret_cast<const uint64_t *>(&elf64.sectionHeaders[reloc5.targetSectionIndex].data[static_cast<size_t>(reloc5.offset)]);

    uint64_t expectedValue0 = reloc0.addend;
    uint64_t expectedValue1 = reloc1.addend;
    auto expectedValue2 = uint64_t(0x80001000) + reloc2.addend;
    uint64_t expectedValue3 = dataGlobal.gpuAddress + reloc3.addend;
    uint64_t expectedValue5 = dataConst.gpuAddress + reloc5.addend;

    EXPECT_EQ(expectedValue0, *reloc0Location);
    EXPECT_EQ(expectedValue1, *reloc1Location);
    EXPECT_EQ(expectedValue2, *reloc2Location);
    EXPECT_EQ(expectedValue3, *reloc3Location);
    EXPECT_EQ(0u, *reloc4Location);
    EXPECT_EQ(expectedValue5, *reloc5Location);
}

TEST_F(LinkerTests, givenImplicitArgRelocationAndStackCallsOrRequiredImplicitArgsThenPatchRelocationWithSizeOfImplicitArgStructAndUpdateKernelDescriptor) {
    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry relocs[] = {reloc};
    constexpr uint32_t numRelocations = 1;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    UltDeviceFactory deviceFactory{1, 0};

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    auto addressToPatch = reinterpret_cast<uint32_t *>(instructionSegment.data() + reloc.r_offset);
    EXPECT_EQ(ImplicitArgsTestHelper::getImplicitArgsSize(deviceFactory.rootDevices[0]->getGfxCoreHelper().getImplicitArgsVersion()), *addressToPatch);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);

    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = false;
    *addressToPatch = 0;

    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                             nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                             deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

HWTEST_F(LinkerTests, givenImplicitArgRelocationAndKernelDescriptorWithImplicitArgsV1WhenLinkingThenPatchRelocationWithSizeOfImplicitArgsV1) {
    DebugManagerStateRestore restore;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t getImplicitArgsVersion() const override {
            return 0;
        }
    };

    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry reloc64 = {};
    memcpy_s(reloc64.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc64.r_offset = 16;
    reloc64.r_type = vISA::GenRelocType::R_SYM_ADDR;

    vISA::GenRelocEntry relocs[] = {reloc, reloc64};
    constexpr uint32_t numRelocations = 2;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;
    kernelDescriptor.kernelMetadata.indirectAccessBuffer = 1;

    HardwareInfo hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};
    auto rootDeviceIndex = deviceFactory.rootDevices[0]->getRootDeviceIndex();
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*deviceFactory.rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]);

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);

    auto addressToPatch = reinterpret_cast<const uint32_t *>(instructionSegment.data() + reloc.r_offset);

    EXPECT_EQ(ImplicitArgsV1::getAlignedSize(), *addressToPatch);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));

    auto addressToPatch64 = (instructionSegment.data() + reloc64.r_offset);
    uint64_t patchedValue64 = 0;
    memcpy_s(&patchedValue64, sizeof(patchedValue64), addressToPatch64, sizeof(patchedValue64));
    EXPECT_EQ(ImplicitArgsV1::getAlignedSize(), patchedValue64);
}

HWTEST_F(LinkerTests, givenImplicitArgRelocationAndImplicitArgsV1WhenLinkingThenPatchRelocationWithSizeOfImplicitArgsV1) {
    DebugManagerStateRestore restore;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t getImplicitArgsVersion() const override {
            return 1;
        }
    };

    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry reloc64 = {};
    memcpy_s(reloc64.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc64.r_offset = 16;
    reloc64.r_type = vISA::GenRelocType::R_SYM_ADDR;

    vISA::GenRelocEntry relocs[] = {reloc, reloc64};
    constexpr uint32_t numRelocations = 2;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    HardwareInfo hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};
    auto rootDeviceIndex = deviceFactory.rootDevices[0]->getRootDeviceIndex();
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*deviceFactory.rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]);

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);

    auto addressToPatch = reinterpret_cast<const uint32_t *>(instructionSegment.data() + reloc.r_offset);

    EXPECT_EQ(ImplicitArgsV1::getAlignedSize(), *addressToPatch);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));

    auto addressToPatch64 = (instructionSegment.data() + reloc64.r_offset);
    uint64_t patchedValue64 = 0;
    memcpy_s(&patchedValue64, sizeof(patchedValue64), addressToPatch64, sizeof(patchedValue64));
    EXPECT_EQ(ImplicitArgsV1::getAlignedSize(), patchedValue64);

    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

HWTEST_F(LinkerTests, givenImplicitArgRelocationAndImplicitArgsWithUnknownVersionWhenLinkingThenUnrecoverableIfCalled) {
    DebugManagerStateRestore restore;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t getImplicitArgsVersion() const override {
            return 3; // unknown version
        }
    };

    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry relocs[] = {reloc};
    constexpr uint32_t numRelocations = 1;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    HardwareInfo hwInfo = *defaultHwInfo;
    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 0, executionEnvironment};
    auto rootDeviceIndex = deviceFactory.rootDevices[0]->getRootDeviceIndex();
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*deviceFactory.rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]);

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    EXPECT_THROW(linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                             nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                             deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions),
                 std::exception);
}

using LinkerDebuggingSupportedTests = ::testing::Test;

TEST_F(LinkerDebuggingSupportedTests, givenImplicitArgRelocationAndEnabledDebuggerThenPatchRelocationWithSizeOfImplicitArgStructAndUpdateKernelDescriptor) {
    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry relocs[] = {reloc};
    constexpr uint32_t numRelocations = 1;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = false;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    UltDeviceFactory deviceFactory{1, 0, *executionEnvironment};
    auto device = deviceFactory.rootDevices[0];
    executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(device);
    EXPECT_NE(nullptr, device->getDebugger());

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  device, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    auto addressToPatch = reinterpret_cast<const uint32_t *>(instructionSegment.data() + reloc.r_offset);
    EXPECT_EQ(ImplicitArgsTestHelper::getImplicitArgsSize(device->getGfxCoreHelper().getImplicitArgsVersion()), *addressToPatch);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));
    EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(LinkerDebuggingSupportedTests, givenNoImplicitArgRelocationAndEnabledDebuggerThenImplicitArgsAreNotRequired) {
    NEO::LinkerInput linkerInput;

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = false;

    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    auto executionEnvironment = MockDevice::prepareExecutionEnvironment(&hwInfo, 0u);
    UltDeviceFactory deviceFactory{1, 0, *executionEnvironment};
    auto device = deviceFactory.rootDevices[0];
    executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(device);
    EXPECT_NE(nullptr, device->getDebugger());

    std::vector<char> instructionSegment;
    char initData = 0x77;
    instructionSegment.resize(32, initData);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  device, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    for (auto &data : instructionSegment) {
        EXPECT_EQ(initData, data);
    }
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(LinkerTests, givenImplicitArgRelocationWithoutStackCallsAndDisabledDebuggerThenDontPatchRelocationAndUpdateKernelDescriptor) {
    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc.r_offset = 8;
    reloc.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry relocs[] = {reloc};
    constexpr uint32_t numRelocations = 1;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = false;

    UltDeviceFactory deviceFactory{1, 0};
    auto device = deviceFactory.rootDevices[0];
    EXPECT_EQ(nullptr, device->getDebugger());

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(32, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  device, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    auto addressToPatch = reinterpret_cast<const uint32_t *>(instructionSegment.data() + reloc.r_offset);
    EXPECT_EQ(initData, *addressToPatch);
    EXPECT_EQ(initData, *(addressToPatch - 1));
    EXPECT_EQ(initData, *(addressToPatch + 1));
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(LinkerTests, givenNoImplicitArgRelocationAndStackCallsThenImplicitArgsAreNotRequired) {
    NEO::LinkerInput linkerInput;

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    UltDeviceFactory deviceFactory{1, 0};

    std::vector<char> instructionSegment;
    char initData = 0x77;
    instructionSegment.resize(32, initData);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    for (auto &data : instructionSegment) {
        EXPECT_EQ(initData, data);
    }
    EXPECT_FALSE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
}

TEST_F(LinkerTests, givenMultipleImplicitArgsRelocationsWithinSingleKernelWhenLinkingThenPatchAllOfThem) {
    NEO::LinkerInput linkerInput;

    vISA::GenRelocEntry reloc0 = {};
    std::string relocationName{implicitArgsRelocationSymbolName};
    memcpy_s(reloc0.r_symbol, 1024, relocationName.c_str(), relocationName.size());
    reloc0.r_offset = 8;
    reloc0.r_type = vISA::GenRelocType::R_SYM_ADDR_32;

    vISA::GenRelocEntry reloc1 = reloc0;
    reloc1.r_offset = 24;

    vISA::GenRelocEntry relocs[] = {reloc0, reloc1};
    constexpr uint32_t numRelocations = 2;
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, numRelocations, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    KernelDescriptor kernelDescriptor;
    kernelDescriptors.push_back(&kernelDescriptor);
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    UltDeviceFactory deviceFactory{1, 0};

    std::vector<char> instructionSegment;
    char initData = 0x77;
    instructionSegment.resize(32, initData);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment, {},
                                  nullptr, nullptr, patchableInstructionSegments, unresolvedExternals,
                                  deviceFactory.rootDevices[0], nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(NEO::LinkingStatus::linkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    for (const auto &reloc : relocs) {
        auto addressToPatch = reinterpret_cast<const uint32_t *>(instructionSegment.data() + reloc.r_offset);
        EXPECT_EQ(ImplicitArgsTestHelper::getImplicitArgsSize(deviceFactory.rootDevices[0]->getGfxCoreHelper().getImplicitArgsVersion()), *addressToPatch);
        EXPECT_TRUE(kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    }
}

HWTEST_F(LinkerTests, givenDependencyOnMissingExternalFunctionWhenLinkingThenFail) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.extFunDependencies.push_back({"fun0", "fun1"});
    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions = {{"fun1", 0U, 128U, 8U}};
    auto linkResult = linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(LinkingStatus::error, linkResult);
}

HWTEST_F(LinkerTests, givenDependencyOnMissingExternalFunctionAndNoExternalFunctionInfosWhenLinkingThenDoNotResolveDependenciesAndReturnSuccess) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.extFunDependencies.push_back({"fun0", "fun1"});
    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::SharedPoolAllocation *patchableGlobalVarSeg = nullptr;
    NEO::SharedPoolAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    NEO::Linker::ExternalFunctionsT externalFunctions;
    auto linkResult = linker.link(
        globalVar, globalConst, exportedFunc, {},
        patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
        unresolvedExternals, pDevice, nullptr, 0, nullptr, 0, kernelDescriptors, externalFunctions);
    EXPECT_EQ(LinkingStatus::linkedFully, linkResult);
}

TEST_F(LinkerTests, givenRelaWhenPatchingInstructionsSegmentThenAddendIsAdded) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfInstructionSegments = true;
    NEO::LinkerInput::RelocationInfo rela;
    rela.offset = 0U;
    rela.addend = 128U;
    rela.type = NEO::LinkerInput::RelocationInfo::Type::address;
    rela.symbolName = "symbol";
    rela.relocationSegment = NEO::SegmentType::instructions;
    linkerInput.textRelocations.push_back({rela});

    WhiteBox<NEO::Linker> linker(linkerInput);
    constexpr uint64_t symValue = 64U;
    linker.relocatedSymbols[rela.symbolName].gpuAddress = symValue;

    uint64_t segmentData{0};
    NEO::Linker::PatchableSegment segmentToPatch;
    segmentToPatch.hostPointer = reinterpret_cast<void *>(&segmentData);
    segmentToPatch.segmentSize = sizeof(segmentData);

    NEO::Linker::UnresolvedExternals unresolvedExternals;
    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    linker.patchInstructionsSegments({segmentToPatch}, unresolvedExternals, kernelDescriptors);
    EXPECT_EQ(static_cast<uint64_t>(rela.addend + symValue), segmentData);
}

HWTEST_F(LinkerTests, givenRelaWhenPatchingDataSegmentThenAddendIsAdded) {
    uint64_t globalConstantSegmentData{0U};
    NEO::MockGraphicsAllocation globalConstantsPatchableSegmentMockGA{&globalConstantSegmentData, sizeof(globalConstantSegmentData)};
    auto globalConstantsPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalConstantsPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalConstantsSegmentInfo.segmentSize = globalConstantsPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfGlobalConstantsBuffer = true;
    NEO::LinkerInput::RelocationInfo rela;
    rela.offset = 0U;
    rela.addend = 128U;
    rela.type = NEO::LinkerInput::RelocationInfo::Type::address;
    rela.symbolName = "symbol";
    rela.relocationSegment = NEO::SegmentType::globalConstants;
    linkerInput.dataRelocations.push_back({rela});

    WhiteBox<NEO::Linker> linker(linkerInput);
    constexpr uint64_t symValue = 64U;
    linker.relocatedSymbols[rela.symbolName].gpuAddress = symValue;

    NEO::Linker::UnresolvedExternals unresolvedExternals;
    linker.patchDataSegments({}, globalConstantsSegmentInfo, {}, globalConstantsPatchableSegment.get(), unresolvedExternals, pDevice, &globalConstantSegmentData, sizeof(globalConstantSegmentData), nullptr, 0);
    EXPECT_EQ(static_cast<uint64_t>(rela.addend + symValue), globalConstantSegmentData);
}

HWTEST_F(LinkerTests, givenRelocationInfoWhenPatchingDataSegmentWithGlobalVariableSymbolThenAddendIsAdded) {
    uint64_t globalVariableSegmentData{0U};
    NEO::MockGraphicsAllocation globalVariablesPatchableSegmentMockGA{&globalVariableSegmentData, sizeof(globalVariableSegmentData)};
    auto globalVariablesPatchableSegment = std::make_unique<NEO::SharedPoolAllocation>(&globalVariablesPatchableSegmentMockGA);

    NEO::Linker::SegmentInfo globalVariablesSegmentInfo;
    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBuffer());
    globalVariablesSegmentInfo.segmentSize = globalVariablesPatchableSegment->getGraphicsAllocation()->getUnderlyingBufferSize();

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;
    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = 0U;
    relocationInfo.addend = 128U;
    relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::address;
    relocationInfo.symbolName = "symbol";
    relocationInfo.relocationSegment = NEO::SegmentType::globalVariables;
    linkerInput.dataRelocations.push_back({relocationInfo});

    WhiteBox<NEO::Linker> linker(linkerInput);
    constexpr uint64_t symValue = 64U;
    linker.relocatedSymbols[relocationInfo.symbolName].gpuAddress = symValue;

    NEO::Linker::UnresolvedExternals unresolvedExternals;
    linker.patchDataSegments(globalVariablesSegmentInfo, {}, globalVariablesPatchableSegment.get(), {}, unresolvedExternals, pDevice, nullptr, 0, &globalVariableSegmentData, sizeof(globalVariableSegmentData));
    EXPECT_EQ(static_cast<uint64_t>(relocationInfo.addend + symValue), globalVariableSegmentData);
}

TEST_F(LinkerTests, givenPerThreadPayloadOffsetRelocationWhenPatchingInstructionSegmentsThenPatchItWithCTDSize) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.requiresPatchingOfInstructionSegments = true;
    NEO::LinkerInput::RelocationInfo rel;
    rel.offset = 0x4;
    rel.type = NEO::LinkerInput::RelocationInfo::Type::perThreadPayloadOffset;
    rel.relocationSegment = NEO::SegmentType::instructions;
    linkerInput.textRelocations.push_back({rel});

    NEO::Linker::KernelDescriptorsT kernelDescriptors;
    KernelDescriptor kd;
    kd.kernelAttributes.crossThreadDataSize = 0x40;
    kd.kernelAttributes.inlineDataPayloadSize = 0x20;
    kernelDescriptors.push_back(&kd);

    WhiteBox<NEO::Linker> linker(linkerInput);

    uint64_t segmentData{0};
    NEO::Linker::PatchableSegment segmentToPatch;
    segmentToPatch.hostPointer = reinterpret_cast<void *>(&segmentData);
    segmentToPatch.segmentSize = sizeof(segmentData);

    NEO::Linker::UnresolvedExternals unresolvedExternals;
    linker.patchInstructionsSegments({segmentToPatch}, unresolvedExternals, kernelDescriptors);
    auto perThreadPayloadOffsetPatchedValue = reinterpret_cast<uint32_t *>(ptrOffset(segmentToPatch.hostPointer, static_cast<size_t>(rel.offset)));
    uint32_t expectedPatchedValue = kd.kernelAttributes.crossThreadDataSize - kd.kernelAttributes.inlineDataPayloadSize;
    EXPECT_EQ(expectedPatchedValue, static_cast<uint32_t>(*perThreadPayloadOffsetPatchedValue));
}

TEST_F(LinkerTests, givenPerThreadPayloadOffsetRelocationAndCrossThreadDataSmallerThanInlineDataWhenPatchingInstructionSegmentsThenPatchItWithOffsetZero) {
    WhiteBox<NEO::LinkerInput> linkerInput{};
    linkerInput.traits.requiresPatchingOfInstructionSegments = true;
    NEO::LinkerInput::RelocationInfo rel{};
    rel.offset = 0x4;
    rel.type = NEO::LinkerInput::RelocationInfo::Type::perThreadPayloadOffset;
    rel.relocationSegment = NEO::SegmentType::instructions;
    linkerInput.textRelocations.push_back({rel});

    NEO::Linker::KernelDescriptorsT kernelDescriptors{};
    KernelDescriptor kd{};
    kd.kernelAttributes.crossThreadDataSize = 40u;
    kd.kernelAttributes.inlineDataPayloadSize = 64u;
    kernelDescriptors.push_back(&kd);

    WhiteBox<NEO::Linker> linker(linkerInput);

    uint64_t segmentData{0};
    NEO::Linker::PatchableSegment segmentToPatch{};
    segmentToPatch.hostPointer = reinterpret_cast<void *>(&segmentData);
    segmentToPatch.segmentSize = sizeof(segmentData);

    NEO::Linker::UnresolvedExternals unresolvedExternals{};
    linker.patchInstructionsSegments({segmentToPatch}, unresolvedExternals, kernelDescriptors);
    auto perThreadPayloadOffsetPatchedValue = reinterpret_cast<uint32_t *>(ptrOffset(segmentToPatch.hostPointer, static_cast<size_t>(rel.offset)));
    uint32_t expectedPatchedValue = 0u;
    EXPECT_EQ(expectedPatchedValue, static_cast<uint32_t>(*perThreadPayloadOffsetPatchedValue));
}
