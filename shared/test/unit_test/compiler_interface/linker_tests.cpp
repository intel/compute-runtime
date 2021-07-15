/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_svm_manager.h"

#include "RelocationInfo.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "linker_mock.h"

#include <string>

TEST(SegmentTypeTests, givenSegmentTypeWhenAsStringIsCalledThenProperRepresentationIsReturned) {
    EXPECT_STREQ("UNKOWN", NEO::asString(NEO::SegmentType::Unknown));
    EXPECT_STREQ("GLOBAL_CONSTANTS", NEO::asString(NEO::SegmentType::GlobalConstants));
    EXPECT_STREQ("GLOBAL_VARIABLES", NEO::asString(NEO::SegmentType::GlobalVariables));
    EXPECT_STREQ("INSTRUCTIONS", NEO::asString(NEO::SegmentType::Instructions));
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
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, symbolB->second.segment);

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
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, symbolB->second.segment);

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
    EXPECT_EQ(NEO::SegmentType::Instructions, symbolA->second.segment);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SegmentType::Instructions, symbolB->second.segment);

    EXPECT_EQ(3, linkerInput.getExportedFunctionsSegmentId());
}

TEST(LinkerInputTests, givenFunctionsSymbolTableThenUndefIsNotAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenSymEntry entry = {};
    entry.s_name[0] = 'A';
    entry.s_offset = 8;
    entry.s_size = 16;
    entry.s_type = vISA::GenSymType::S_UNDEF;

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
    NEO::LinkerInput linkerInput;
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 7U;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.symbolName = "aaa";
    relocInfo.symbolSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.addDataRelocationInfo(relocInfo);
    ASSERT_EQ(1U, linkerInput.getDataRelocations().size());
    EXPECT_EQ(relocInfo.offset, linkerInput.getDataRelocations()[0].offset);
    EXPECT_EQ(relocInfo.relocationSegment, linkerInput.getDataRelocations()[0].relocationSegment);
    EXPECT_EQ(relocInfo.symbolName, linkerInput.getDataRelocations()[0].symbolName);
    EXPECT_EQ(relocInfo.symbolSegment, linkerInput.getDataRelocations()[0].symbolSegment);
    EXPECT_EQ(relocInfo.type, linkerInput.getDataRelocations()[0].type);
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
    EXPECT_TRUE(linkerInput.isValid());

    linkerInput = {};
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
    relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;
    linkerInput.addDataRelocationInfo(relocInfo);
    ASSERT_EQ(1U, linkerInput.getDataRelocations().size());
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);
    EXPECT_TRUE(linkerInput.isValid());
}

TEST(LinkerInputTests, WhenGettingSegmentForSectionNameThenCorrectSegmentIsReturned) {
    auto segmentConst = NEO::LinkerInput::getSegmentForSection(NEO::Elf::SectionsNamesZebin::dataConst.str());
    auto segmentGlobalConst = NEO::LinkerInput::getSegmentForSection(NEO::Elf::SectionsNamesZebin::dataGlobalConst.str());
    auto segmentGlobal = NEO::LinkerInput::getSegmentForSection(NEO::Elf::SectionsNamesZebin::dataGlobal.str());
    auto segmentInstructions = NEO::LinkerInput::getSegmentForSection(NEO::Elf::SectionsNamesZebin::textPrefix.str());
    auto segmentInstructions2 = NEO::LinkerInput::getSegmentForSection(".text.abc");

    EXPECT_EQ(NEO::SegmentType::GlobalConstants, segmentConst);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, segmentGlobalConst);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, segmentGlobal);
    EXPECT_EQ(NEO::SegmentType::Instructions, segmentInstructions);
    EXPECT_EQ(NEO::SegmentType::Instructions, segmentInstructions2);
}

TEST(LinkerInputTests, WhenGettingSegmentForUnknownSectionNameThenUnknownSegmentIsReturned) {
    auto segment = NEO::LinkerInput::getSegmentForSection("Not_a_valid_section_name");
    EXPECT_EQ(NEO::SegmentType::Unknown, segment);
}

TEST(LinkerInputTests, WhenAddingElfTextRelocationForSegmentIndexThenInstructionSegmentForRelocationAndProperTraitsAreSet) {
    NEO::LinkerInput linkerInput = {};
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalConstantsBuffer);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfGlobalVariablesBuffer);

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 7u;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.symbolName = "test";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;

    linkerInput.addElfTextSegmentRelocation(relocInfo, 5);

    ASSERT_EQ(0u, linkerInput.getDataRelocations().size());
    ASSERT_EQ(6u, linkerInput.getRelocationsInInstructionSegments().size());

    auto relocs = linkerInput.getRelocationsInInstructionSegments()[5];

    ASSERT_EQ(1u, relocs.size());

    EXPECT_EQ(NEO::SegmentType::Instructions, relocs[0].relocationSegment);
    EXPECT_EQ(NEO::SegmentType::Unknown, relocs[0].symbolSegment);
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
    relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.symbolName = "test";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;

    linkerInput.addElfTextSegmentRelocation(relocInfo, 0);

    ASSERT_EQ(1u, linkerInput.getRelocationsInInstructionSegments().size());
    auto &relocs = linkerInput.getRelocationsInInstructionSegments()[0];
    EXPECT_EQ(1u, relocs.size());

    relocInfo.offset = 24u;
    linkerInput.addElfTextSegmentRelocation(relocInfo, 0);

    EXPECT_EQ(2u, relocs.size());
    EXPECT_TRUE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);
}

TEST(LinkerInputTests, WhenDecodingElfTextRelocationsThenCorrectRelocationsAreAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 0;

    elf64.relocations.emplace_back(reloc);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2;
    reloc2.offset = 32;
    reloc2.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_32);
    reloc2.symbolName = "symbol2";
    reloc2.symbolSectionIndex = 1;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 2;

    elf64.relocations.emplace_back(reloc2);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getRelocationsInInstructionSegments();
    ASSERT_EQ(2u, relocations.size());

    auto &segment0Relocs = relocations[0];
    ASSERT_EQ(1u, segment0Relocs.size());

    EXPECT_EQ(64u, segment0Relocs[0].offset);
    EXPECT_EQ(NEO::SegmentType::Instructions, segment0Relocs[0].relocationSegment);
    EXPECT_EQ("symbol1", segment0Relocs[0].symbolName);
    EXPECT_EQ(NEO::SegmentType::Unknown, segment0Relocs[0].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, segment0Relocs[0].type);

    auto &segment1Relocs = relocations[1];
    ASSERT_EQ(1u, segment1Relocs.size());

    EXPECT_EQ(32u, segment1Relocs[0].offset);
    EXPECT_EQ(NEO::SegmentType::Instructions, segment1Relocs[0].relocationSegment);
    EXPECT_EQ("symbol2", segment1Relocs[0].symbolName);
    EXPECT_EQ(NEO::SegmentType::Unknown, segment1Relocs[0].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::AddressLow, segment1Relocs[0].type);
}

TEST(LinkerInputTests, GivenNoKernelNameToIdWhenDecodingElfTextRelocationsThenNoRelocationIsAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 0;

    elf64.relocations.emplace_back(reloc);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["wrong_name"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getRelocationsInInstructionSegments();
    ASSERT_EQ(0u, relocations.size());
}

TEST(LinkerInputTests, GivenInvalidTextSectionNameWhenDecodingElfTextRelocationsThenNoRelocationIsAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".invalid.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 0;

    elf64.relocations.emplace_back(reloc);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getRelocationsInInstructionSegments();
    ASSERT_EQ(0u, relocations.size());
}

TEST(LinkerInputTests, GivenValidZebinRelocationTypesWhenDecodingElfTextRelocationsThenCorrectTypeIsSet) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc1;
    reloc1.offset = 0;
    reloc1.relocType = NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_NONE;
    reloc1.symbolName = "symbol1";
    reloc1.symbolSectionIndex = 1;
    reloc1.symbolTableIndex = 0;
    reloc1.targetSectionIndex = 0;
    elf64.relocations.emplace_back(reloc1);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2;
    reloc2.offset = 0;
    reloc2.relocType = NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;
    reloc2.symbolName = "symbol2";
    reloc2.symbolSectionIndex = 1;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 0;
    elf64.relocations.emplace_back(reloc2);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc3;
    reloc3.offset = 0;
    reloc3.relocType = NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32;
    reloc3.symbolName = "symbol3";
    reloc3.symbolSectionIndex = 1;
    reloc3.symbolTableIndex = 0;
    reloc3.targetSectionIndex = 0;
    elf64.relocations.emplace_back(reloc3);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc4;
    reloc4.offset = 0;
    reloc4.relocType = NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32_HI;
    reloc4.symbolName = "symbol4";
    reloc4.symbolSectionIndex = 1;
    reloc4.symbolTableIndex = 0;
    reloc4.targetSectionIndex = 0;
    elf64.relocations.emplace_back(reloc4);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc5;
    reloc5.offset = 0;
    reloc5.relocType = NEO::Elf::RELOC_TYPE_ZEBIN::R_PER_THREAD_PAYLOAD_OFFSET;
    reloc5.symbolName = "symbol5";
    reloc5.symbolSectionIndex = 1;
    reloc5.symbolTableIndex = 0;
    reloc5.targetSectionIndex = 0;
    elf64.relocations.emplace_back(reloc5);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["abc"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getRelocationsInInstructionSegments();
    ASSERT_EQ(1u, relocations.size());
    ASSERT_EQ(5u, relocations[0].size());
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Unknown, relocations[0][0].type);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocations[0][1].type);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::AddressLow, relocations[0][2].type);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::AddressHigh, relocations[0][3].type);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::PerThreadPayloadOffset, relocations[0][4].type);
}

TEST(LinkerInputTests, GivenInvalidRelocationTypeWhenDecodingElfTextRelocationsThenUnknownTypeIsSet) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = 0xffffffff; // invalid type
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 0;

    elf64.relocations.emplace_back(reloc);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getRelocationsInInstructionSegments();
    ASSERT_EQ(1u, relocations.size());
    ASSERT_EQ(1u, relocations[0].size());
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Unknown, relocations[0][0].type);
}

TEST(LinkerInputTests, WhenDecodingElfGlobalDataRelocationsThenCorrectRelocationsAreAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.global";
    sectionNames[2] = ".data.const";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 1;

    elf64.relocations.emplace_back(reloc);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2;
    reloc2.offset = 32;
    reloc2.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc2.symbolName = "symbol2";
    reloc2.symbolSectionIndex = 2;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 1;

    elf64.relocations.emplace_back(reloc2);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getDataRelocations();
    ASSERT_EQ(2u, relocations.size());

    EXPECT_EQ(64u, relocations[0].offset);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocations[0].relocationSegment);
    EXPECT_EQ("symbol1", relocations[0].symbolName);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocations[0].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocations[0].type);

    EXPECT_EQ(32u, relocations[1].offset);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocations[1].relocationSegment);
    EXPECT_EQ("symbol2", relocations[1].symbolName);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocations[1].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocations[1].type);
}

TEST(LinkerInputTests, WhenDecodingElfConstantDataRelocationsThenCorrectRelocationsAreAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.global";
    sectionNames[2] = ".data.const";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 2;

    elf64.relocations.emplace_back(reloc);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2;
    reloc2.offset = 32;
    reloc2.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc2.symbolName = "symbol2";
    reloc2.symbolSectionIndex = 2;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 2;

    elf64.relocations.emplace_back(reloc2);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getDataRelocations();
    ASSERT_EQ(2u, relocations.size());

    EXPECT_EQ(64u, relocations[0].offset);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocations[0].relocationSegment);
    EXPECT_EQ("symbol1", relocations[0].symbolName);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocations[0].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocations[0].type);

    EXPECT_EQ(32u, relocations[1].offset);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocations[1].relocationSegment);
    EXPECT_EQ("symbol2", relocations[1].symbolName);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocations[1].symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocations[1].type);
}

TEST(LinkerInputTests, GivenUnsupportedDataSegmentWhenDecodingElfDataRelocationThenNoRelocationsAreAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data";
    sectionNames[2] = ".data.const";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 2;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 1;

    elf64.relocations.emplace_back(reloc);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getDataRelocations();
    ASSERT_EQ(0u, relocations.size());
}

TEST(LinkerInputTests, GivenUnsupportedSymbolSegmentWhenDecodingElfDataRelocationThenNoRelocationsAreAddedAndDebugBreakIsCalled) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data";
    sectionNames[2] = ".data.const";

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc;
    reloc.offset = 64;
    reloc.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc.symbolName = "symbol1";
    reloc.symbolSectionIndex = 1;
    reloc.symbolTableIndex = 0;
    reloc.targetSectionIndex = 2;

    elf64.relocations.emplace_back(reloc);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto relocations = linkerInput.getDataRelocations();
    ASSERT_EQ(0u, relocations.size());
}

TEST(LinkerInputTests, GivenGlobalAndLocalElfSymbolsWhenDecodingThenOnlyGlobalSymbolsAreAdded) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol;
    symbol.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_OBJECT | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol.name = 0;
    symbol.other = 0;
    symbol.shndx = 1;
    symbol.size = 8;
    symbol.value = 0x1234000;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol2;
    symbol2.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol2.name = 8;
    symbol2.other = 0;
    symbol2.shndx = 0;
    symbol2.size = 16;
    symbol2.value = 0x5000;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol3;
    symbol3.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_LOCAL << 4;
    symbol3.name = 16;
    symbol3.other = 0;
    symbol3.shndx = 0;
    symbol3.size = 8;
    symbol3.value = 0;

    elf64.symbolTable.emplace_back(symbol);
    elf64.symbolTable.emplace_back(symbol2);
    elf64.symbolTable.emplace_back(symbol3);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto symbols = linkerInput.getSymbols();
    ASSERT_EQ(2u, symbols.size());

    EXPECT_EQ(0x1234000u, symbols[std::to_string(symbol.name)].offset);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, symbols[std::to_string(symbol.name)].segment);
    EXPECT_EQ(8u, symbols[std::to_string(symbol.name)].size);

    EXPECT_EQ(0x5000u, symbols[std::to_string(symbol2.name)].offset);
    EXPECT_EQ(NEO::SegmentType::Instructions, symbols[std::to_string(symbol2.name)].segment);
    EXPECT_EQ(16u, symbols[std::to_string(symbol2.name)].size);

    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_TRUE(linkerInput.getTraits().exportsFunctions);
}

TEST(LinkerInputTests, GivenGlobalFunctionsInTwoSegementsWhenDecodingThenUnrecoverableIsCalled) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol;
    symbol.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol.name = 0;
    symbol.other = 0;
    symbol.shndx = 0;
    symbol.size = 8;
    symbol.value = 0x1234000;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol2;
    symbol2.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol2.name = 8;
    symbol2.other = 0;
    symbol2.shndx = 2;
    symbol2.size = 16;
    symbol2.value = 0x5000;

    elf64.symbolTable.emplace_back(symbol);
    elf64.symbolTable.emplace_back(symbol2);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    EXPECT_THROW(linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId), std::exception);

    auto symbols = linkerInput.getSymbols();
    ASSERT_EQ(2u, symbols.size());
    EXPECT_EQ(0, linkerInput.getExportedFunctionsSegmentId());
}

TEST(LinkerInputTests, GivenNoKernelNameToIdWhenDecodingGlobalFunctionThenExportedFunctionsSegmentIsNotSet) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";

    elf64.setupSecionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol;
    symbol.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol.name = 0;
    symbol.other = 0;
    symbol.shndx = 0;
    symbol.size = 8;
    symbol.value = 0x1234000;

    elf64.symbolTable.emplace_back(symbol);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto symbols = linkerInput.getSymbols();
    ASSERT_EQ(1u, symbols.size());
    EXPECT_EQ(-1, linkerInput.getExportedFunctionsSegmentId());
}

TEST(LinkerInputTests, GivenGlobalElfSymbolOfNoTypeWhenDecodingThenSymbolWithUnknownSegmentIsAddedAndDebugBreakCalled) {
    NEO::LinkerInput linkerInput = {};
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> header;
    MockElf<NEO::Elf::EI_CLASS_64> elf64;
    elf64.elfFileHeader = &header;

    std::unordered_map<uint32_t, std::string> sectionNames;
    sectionNames[0] = ".text.abc";
    sectionNames[1] = ".data.const";
    sectionNames[2] = ".text.hello";

    elf64.setupSecionNames(std::move(sectionNames));
    elf64.overrideSymbolName = true;

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbol;
    symbol.info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_NOTYPE | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
    symbol.name = 0;
    symbol.other = 0;
    symbol.shndx = 1;
    symbol.size = 8;
    symbol.value = 0x1234000;

    elf64.symbolTable.emplace_back(symbol);

    NEO::LinkerInput::SectionNameToSegmentIdMap nameToKernelId;

    nameToKernelId["abc"] = 0;
    nameToKernelId["hello"] = 1;

    linkerInput.decodeElfSymbolTableAndRelocations(elf64, nameToKernelId);

    auto symbols = linkerInput.getSymbols();
    ASSERT_EQ(1u, symbols.size());
    EXPECT_EQ(SegmentType::Unknown, symbols.begin()->second.segment);

    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
}

TEST(LinkerTests, givenEmptyLinkerInputThenLinkerOutputIsEmpty) {
    NEO::LinkerInput linkerInput;
    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    auto linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
}

TEST(LinkerTests, givenInvalidLinkerInputThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;
    mockLinkerInput.valid = false;
    NEO::Linker linker(mockLinkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    auto linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::Error, linkResult);
}

TEST(LinkerTests, givenUnresolvedExternalWhenPatchingInstructionsThenLinkPartially) {
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

    std::vector<char> instructionSegment;
    instructionSegment.resize(64);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    auto linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_EQ(0U, unresolvedExternals[0].instructionsSegmentId);
    EXPECT_FALSE(unresolvedExternals[0].internalError);
    EXPECT_EQ(entry.r_offset, unresolvedExternals[0].unresolvedRelocation.offset);
    EXPECT_EQ(std::string(entry.r_symbol), std::string(unresolvedExternals[0].unresolvedRelocation.symbolName));
}

TEST(LinkerTests, givenValidSymbolsAndRelocationsThenInstructionSegmentsAreProperlyPatched) {
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

    vISA::GenRelocEntry relocIgnore = {};
    relocIgnore.r_symbol[0] = 'X';
    relocIgnore.r_offset = 36;
    relocIgnore.r_type = vISA::GenRelocType::R_PER_THREAD_PAYLOAD_OFFSET_32;

    vISA::GenRelocEntry relocs[] = {relocA, relocB, relocC, relocCPartHigh, relocCPartLow, relocIgnore};
    constexpr uint32_t numRelocations = sizeof(relocs) / sizeof(relocs[0]);
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

    std::vector<char> instructionSegment;
    uint32_t initData = 0x77777777;
    instructionSegment.resize(64, static_cast<char>(initData));
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals,
                                  nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(3U, relocatedSymbols.size());

    ASSERT_EQ(1U, relocatedSymbols.count(symGlobalVariable.s_name));
    ASSERT_EQ(1U, relocatedSymbols.count(symGlobalConstant.s_name));
    ASSERT_EQ(1U, relocatedSymbols.count(symGlobalVariable.s_name));

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
}

TEST(LinkerTests, givenInvalidSymbolOffsetWhenPatchingInstructionsThenRelocationFails) {
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

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::Error, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals,
                             nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
}

TEST(LinkerTests, givenInvalidRelocationOffsetThenPatchingOfInstructionsFails) {
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

    std::vector<char> instructionSegment;
    instructionSegment.resize(relocA.r_offset + sizeof(uintptr_t), 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = relocA.r_offset;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;

    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(1U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_TRUE(unresolvedExternals[0].internalError);

    patchableInstructionSegments[0].segmentSize = relocA.r_offset + sizeof(uintptr_t);
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                             unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
}

TEST(LinkerTests, givenUnknownSymbolTypeWhenPatchingInstructionsThenRelocationFails) {
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

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};
    NEO::GraphicsAllocation *patchableGlobalVarSeg = nullptr;
    NEO::GraphicsAllocation *patchableConstVarSeg = nullptr;

    ASSERT_EQ(1U, linkerInput.symbols.count("A"));
    linkerInput.symbols["A"].segment = NEO::SegmentType::Unknown;
    auto linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::Error, linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(0U, unresolvedExternals.size());

    linkerInput.symbols["A"].segment = NEO::SegmentType::GlobalVariables;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                             unresolvedExternals, nullptr, nullptr, nullptr);
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
}

TEST(LinkerTests, givenInvalidSourceSegmentWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::MockGraphicsAllocation emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::MockGraphicsAllocation nonEmptypatchableSegment{nonEmptypatchableSegmentData.data(), nonEmptypatchableSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.dataRelocations.push_back(relocInfo);

    {
        linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;
        linkerInput.traits.requiresPatchingOfGlobalConstantsBuffer = false;
        linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::Unknown;
        auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                      &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                                      unresolvedExternals, nullptr, nullptr, nullptr);

        EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
        EXPECT_EQ(1U, unresolvedExternals.size());

        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
        unresolvedExternals.clear();
        linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                 &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                                 unresolvedExternals, nullptr, nullptr, nullptr);

        EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
        EXPECT_EQ(0U, unresolvedExternals.size());
    }

    {
        linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = false;
        linkerInput.traits.requiresPatchingOfGlobalConstantsBuffer = true;
        linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalConstants;
        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::Unknown;
        unresolvedExternals.clear();
        auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                      &emptyPatchableSegment, &nonEmptypatchableSegment, {},
                                      unresolvedExternals, nullptr, nullptr, nullptr);

        EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
        EXPECT_EQ(1U, unresolvedExternals.size());

        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
        unresolvedExternals.clear();
        linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                 &emptyPatchableSegment, &nonEmptypatchableSegment, {},
                                 unresolvedExternals, nullptr, nullptr, nullptr);

        EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
        EXPECT_EQ(0U, unresolvedExternals.size());
    }
}

TEST(LinkerTests, givenUnknownRelocationSegmentWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::MockGraphicsAllocation emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::MockGraphicsAllocation nonEmptypatchableSegment{nonEmptypatchableSegmentData.data(), nonEmptypatchableSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::Unknown;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                                  unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                             unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
}

TEST(LinkerTests, givenRelocationTypeWithHighPartOfAddressWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::MockGraphicsAllocation emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::MockGraphicsAllocation nonEmptypatchableSegment{nonEmptypatchableSegmentData.data(), nonEmptypatchableSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::AddressHigh;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                                  unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                             unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
}

TEST(LinkerTests, givenValidSymbolsAndRelocationsThenDataSegmentsAreProperlyPatched) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    std::vector<char> globalConstantsSegmentData;
    globalConstantsSegmentData.resize(128, 7U);

    std::vector<char> globalVariablesSegmentData;
    globalVariablesSegmentData.resize(256, 13U);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsSegmentData.data());
    globalConstantsSegmentInfo.segmentSize = globalConstantsSegmentData.size();
    NEO::MockGraphicsAllocation globalConstantsPatchableSegment{globalConstantsSegmentData.data(), globalConstantsSegmentData.size()};

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesSegmentData.data());
    globalVariablesSegmentInfo.segmentSize = globalVariablesSegmentData.size();
    NEO::MockGraphicsAllocation globalVariablesPatchableSegment{globalVariablesSegmentData.data(), globalVariablesSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocationInfo[5];
    // GlobalVar -> GlobalVar
    relocationInfo[0].offset = 8U;
    relocationInfo[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[0].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalConst -> GlobalVar
    relocationInfo[1].offset = 24U;
    relocationInfo[1].relocationSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[1].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[1].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalConst -> GlobalConst
    relocationInfo[2].offset = 40U;
    relocationInfo[2].relocationSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[2].symbolSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[2].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalVar -> GlobalConst
    relocationInfo[3].offset = 56U;
    relocationInfo[3].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[3].symbolSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[3].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalVar Low -> GlobalVar
    relocationInfo[4].offset = 72;
    relocationInfo[4].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[4].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[4].type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;

    uint32_t initValue = 0;
    for (const auto &reloc : relocationInfo) {
        linkerInput.addDataRelocationInfo(reloc);
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        if (reloc.type == NEO::LinkerInput::RelocationInfo::Type::Address) {
            *reinterpret_cast<uintptr_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        } else {
            *reinterpret_cast<uint32_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        }
        ++initValue;
    }

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get()));

    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {},
                                  &globalVariablesPatchableSegment, &globalConstantsPatchableSegment, {},
                                  unresolvedExternals, device.get(), globalConstantsPatchableSegment.getUnderlyingBuffer(),
                                  globalVariablesPatchableSegment.getUnderlyingBuffer());
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(7U, *reinterpret_cast<uint8_t *>(globalConstantsPatchableSegment.getUnderlyingBuffer()));
    EXPECT_EQ(13U, *reinterpret_cast<uint8_t *>(globalVariablesPatchableSegment.getUnderlyingBuffer()));

    initValue = 0;
    for (const auto &reloc : relocationInfo) {
        void *srcRaw = (reloc.symbolSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        uint8_t *src = reinterpret_cast<uint8_t *>(srcRaw);
        uint8_t *dst = reinterpret_cast<uint8_t *>(dstRaw);

        // make sure no buffer underflow occured
        EXPECT_EQ(dst[0], dst[reloc.offset - 1]);

        // check patch-incremented value
        if (reloc.type == NEO::LinkerInput::RelocationInfo::Type::Address) {
            // make sure no buffer overflow occured
            EXPECT_EQ(dst[0], dst[reloc.offset + sizeof(uintptr_t)]);
            EXPECT_EQ(reinterpret_cast<uintptr_t>(src) + initValue * 4, *reinterpret_cast<uintptr_t *>(dst + reloc.offset)) << initValue;
        } else {
            // make sure no buffer overflow occured
            EXPECT_EQ(dst[0], dst[reloc.offset + sizeof(uint32_t)]);
            EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(src)) + initValue * 4, *reinterpret_cast<uint32_t *>(dst + reloc.offset)) << initValue;
        }
        ++initValue;
    }
}

TEST(LinkerTests, givenValidSymbolsAndRelocationsWhenPatchin32bitBinaryThenDataSegmentsAreProperlyPatchedWithLowerPartOfTheAddress) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.setPointerSize(NEO::LinkerInput::Traits::PointerSize::Ptr32bit);
    NEO::Linker linker(linkerInput);

    std::vector<char> globalConstantsSegmentData;
    globalConstantsSegmentData.resize(128, 7U);

    std::vector<char> globalVariablesSegmentData;
    globalVariablesSegmentData.resize(256, 13U);

    NEO::Linker::SegmentInfo globalConstantsSegmentInfo, globalVariablesSegmentInfo;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsSegmentData.data());
    globalConstantsSegmentInfo.segmentSize = globalConstantsSegmentData.size();
    NEO::MockGraphicsAllocation globalConstantsPatchableSegment{globalConstantsSegmentData.data(), globalConstantsSegmentData.size()};

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesSegmentData.data());
    globalVariablesSegmentInfo.segmentSize = globalVariablesSegmentData.size();
    NEO::MockGraphicsAllocation globalVariablesPatchableSegment{globalVariablesSegmentData.data(), globalVariablesSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocationInfo[5];
    // GlobalVar -> GlobalVar
    relocationInfo[0].offset = 8U;
    relocationInfo[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[0].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalConst -> GlobalVar
    relocationInfo[1].offset = 24U;
    relocationInfo[1].relocationSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[1].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[1].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalConst -> GlobalConst
    relocationInfo[2].offset = 40U;
    relocationInfo[2].relocationSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[2].symbolSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[2].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalVar -> GlobalConst
    relocationInfo[3].offset = 56U;
    relocationInfo[3].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[3].symbolSegment = NEO::SegmentType::GlobalConstants;
    relocationInfo[3].type = NEO::LinkerInput::RelocationInfo::Type::Address;

    // GlobalVar Low -> GlobalVar
    relocationInfo[4].offset = 72;
    relocationInfo[4].relocationSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[4].symbolSegment = NEO::SegmentType::GlobalVariables;
    relocationInfo[4].type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;

    uint32_t initValue = 0;
    for (const auto &reloc : relocationInfo) {
        linkerInput.addDataRelocationInfo(reloc);
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        *reinterpret_cast<uint32_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        ++initValue;
    }

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get()));

    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {},
                                  &globalVariablesPatchableSegment, &globalConstantsPatchableSegment, {},
                                  unresolvedExternals, device.get(), globalConstantsPatchableSegment.getUnderlyingBuffer(),
                                  globalVariablesPatchableSegment.getUnderlyingBuffer());
    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(7U, *reinterpret_cast<uint8_t *>(globalConstantsPatchableSegment.getUnderlyingBuffer()));
    EXPECT_EQ(13U, *reinterpret_cast<uint8_t *>(globalVariablesPatchableSegment.getUnderlyingBuffer()));

    initValue = 0;
    for (const auto &reloc : relocationInfo) {
        void *srcRaw = (reloc.symbolSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables)
                           ? globalVariablesPatchableSegment.getUnderlyingBuffer()
                           : globalConstantsPatchableSegment.getUnderlyingBuffer();
        uint8_t *src = reinterpret_cast<uint8_t *>(srcRaw);
        uint8_t *dst = reinterpret_cast<uint8_t *>(dstRaw);

        // make sure no buffer under/overflow occured
        EXPECT_EQ(dst[0], dst[reloc.offset - 1]);
        EXPECT_EQ(dst[0], dst[reloc.offset + sizeof(uint32_t)]);

        // check patch-incremented value
        EXPECT_EQ(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(src)) + initValue * 4, *reinterpret_cast<uint32_t *>(dst + reloc.offset)) << initValue;
        ++initValue;
    }
}

TEST(LinkerTests, givenInvalidRelocationOffsetThenPatchingOfDataSegmentsFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::MockGraphicsAllocation emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::MockGraphicsAllocation nonEmptypatchableSegment{nonEmptypatchableSegmentData.data(), nonEmptypatchableSegmentData.size()};

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 64U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                                  unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedPartially, linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].offset = 32;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             &nonEmptypatchableSegment, &emptyPatchableSegment, {},
                             unresolvedExternals, nullptr, nullptr, nullptr);

    EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
}

TEST(LinkerTests, GivenAllocationInLocalMemoryWhichRequiresBlitterWhenPatchingDataSegmentsAllocationThenBlitterIsUsed) {
    DebugManagerStateRestore restorer;

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    uint32_t blitsCounter = 0;
    uint32_t expectedBlitsCount = 0;
    auto mockBlitMemoryToAllocation = [&blitsCounter](const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                      Vec3<size_t> size) -> BlitOperationResult {
        blitsCounter++;
        return BlitOperationResult::Success;
    };
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation};

    LocalMemoryAccessMode localMemoryAccessModes[] = {
        LocalMemoryAccessMode::Default,
        LocalMemoryAccessMode::CpuAccessAllowed,
        LocalMemoryAccessMode::CpuAccessDisallowed};

    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    for (auto localMemoryAccessMode : localMemoryAccessModes) {
        DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(localMemoryAccessMode));
        for (auto isLocalMemorySupported : ::testing::Bool()) {
            DebugManager.flags.EnableLocalMemory.set(isLocalMemorySupported);
            auto pDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
            MockSVMAllocsManager svmAllocsManager(pDevice->getMemoryManager(), false);

            WhiteBox<NEO::LinkerInput> linkerInput;
            NEO::Linker linker(linkerInput);

            NEO::Linker::SegmentInfo emptySegmentInfo;
            NEO::Linker::UnresolvedExternals unresolvedExternals;

            std::vector<char> nonEmptypatchableSegmentData;
            nonEmptypatchableSegmentData.resize(64, 8U);
            auto pNonEmptypatchableSegment = allocateGlobalsSurface(&svmAllocsManager, *pDevice, nonEmptypatchableSegmentData.size(),
                                                                    true /* constant */, nullptr /* linker input */,
                                                                    nonEmptypatchableSegmentData.data());

            NEO::LinkerInput::RelocationInfo relocInfo;
            relocInfo.offset = 0U;
            relocInfo.symbolName = "aaa";
            relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
            linkerInput.dataRelocations.push_back(relocInfo);
            linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
            linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
            linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;
            uint64_t initData = 0x1234;

            auto linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                          pNonEmptypatchableSegment, pNonEmptypatchableSegment, {},
                                          unresolvedExternals, pDevice.get(), &initData, &initData);

            EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
            EXPECT_EQ(0U, unresolvedExternals.size());

            linkerInput.dataRelocations[0].type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;
            linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                     pNonEmptypatchableSegment, pNonEmptypatchableSegment, {},
                                     unresolvedExternals, pDevice.get(), &initData, &initData);

            EXPECT_EQ(NEO::LinkingStatus::LinkedFully, linkResult);
            EXPECT_EQ(0U, unresolvedExternals.size());

            if (pNonEmptypatchableSegment->isAllocatedInLocalMemoryPool() &&
                (localMemoryAccessMode == LocalMemoryAccessMode::CpuAccessDisallowed)) {
                auto blitsOnAllocationInitialization = 1;
                auto blitsOnPatchingDataSegments = 2;
                expectedBlitsCount += blitsOnAllocationInitialization + blitsOnPatchingDataSegments;
            }
            EXPECT_EQ(expectedBlitsCount, blitsCounter);
            pDevice->getMemoryManager()->freeGraphicsMemory(pNonEmptypatchableSegment);
        }
    }
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
    unresolvedExternal.unresolvedRelocation.relocationSegment = NEO::SegmentType::Instructions;
    unresolvedExternals.push_back(unresolvedExternal);
    auto err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(unresolvedExternal.unresolvedRelocation.symbolName.c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(segmentsNames[unresolvedExternal.instructionsSegmentId].c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));
    EXPECT_THAT(err.c_str(), ::testing::Not(::testing::HasSubstr("internal error")));

    unresolvedExternals[0].internalError = true;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(unresolvedExternal.unresolvedRelocation.symbolName.c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(segmentsNames[unresolvedExternal.instructionsSegmentId].c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr("internal linker error"));

    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(unresolvedExternal.unresolvedRelocation.symbolName.c_str()));
    EXPECT_THAT(err.c_str(), ::testing::Not(::testing::HasSubstr(segmentsNames[unresolvedExternal.instructionsSegmentId].c_str())));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr("internal linker error"));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::GlobalConstants;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(NEO::asString(NEO::SegmentType::GlobalConstants)));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::GlobalVariables;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(NEO::asString(NEO::SegmentType::GlobalVariables)));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));

    unresolvedExternals[0].unresolvedRelocation.relocationSegment = NEO::SegmentType::Unknown;
    err = NEO::constructLinkerErrorMessage(unresolvedExternals, {});
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(NEO::asString(NEO::SegmentType::Unknown)));
    EXPECT_THAT(err.c_str(), ::testing::HasSubstr(std::to_string(unresolvedExternal.unresolvedRelocation.offset).c_str()));
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
    funcSymbol.symbol.segment = NEO::SegmentType::Instructions;
    funcSymbol.symbol.offset = 64U;
    funcSymbol.symbol.size = 1024U;
    funcSymbol.gpuAddress = 4096U;

    constDataSymbol.symbol.segment = NEO::SegmentType::GlobalConstants;
    constDataSymbol.symbol.offset = 32U;
    constDataSymbol.symbol.size = 16U;
    constDataSymbol.gpuAddress = 8U;

    globalVarSymbol.symbol.segment = NEO::SegmentType::GlobalVariables;
    globalVarSymbol.symbol.offset = 72U;
    globalVarSymbol.symbol.size = 8U;
    globalVarSymbol.gpuAddress = 256U;

    auto message = NEO::constructRelocationsDebugMessage(symbols);

    std::stringstream expected;
    expected << "Relocations debug informations :\n";
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

TEST(LinkerTests, GivenDebugDataWhenApplyingDebugDataRelocationsThenRelocationsAreAppliedInMemory) {
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

    elf64.setupSecionNames(std::move(sectionNames));

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc0 = {};
    reloc0.offset = 64;
    reloc0.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc0.symbolName = ".debug_abbrev";
    reloc0.symbolSectionIndex = 3;
    reloc0.symbolTableIndex = 0;
    reloc0.targetSectionIndex = 2;
    reloc0.addend = 0;

    elf64.debugInfoRelocations.emplace_back(reloc0);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc1 = {};
    reloc1.offset = 32;
    reloc1.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_32);
    reloc1.symbolName = ".debug_line";
    reloc1.symbolSectionIndex = 4;
    reloc1.symbolTableIndex = 0;
    reloc1.targetSectionIndex = 2;
    reloc1.addend = 4;

    elf64.debugInfoRelocations.emplace_back(reloc1);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc2 = {};
    reloc2.offset = 32;
    reloc2.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
    reloc2.symbolName = ".text";
    reloc2.symbolSectionIndex = 0;
    reloc2.symbolTableIndex = 0;
    reloc2.targetSectionIndex = 4;
    reloc2.addend = 18;

    elf64.debugInfoRelocations.emplace_back(reloc2);

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>::RelocationInfo reloc3 = {};
    reloc3.offset = 0;
    reloc3.relocType = static_cast<uint32_t>(Elf::RELOCATION_X8664_TYPE::R_X8664_64);
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