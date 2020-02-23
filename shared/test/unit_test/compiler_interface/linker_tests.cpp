/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

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

TEST(LinkerTests, givenEmptyLinkerInputThenLinkerOutputIsEmpty) {
    NEO::LinkerInput linkerInput;
    NEO::Linker linker(linkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    bool linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
}

TEST(LinkerTests, givenInvalidLinkerInputThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> mockLinkerInput;
    mockLinkerInput.valid = false;
    NEO::Linker linker(mockLinkerInput);
    NEO::Linker::SegmentInfo globalVar, globalConst, exportedFunc;
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    bool linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_FALSE(linkResult);
}

TEST(LinkerTests, givenUnresolvedExternalWhenPatchingInstructionsThenLinkerFails) {
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
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    bool linkResult = linker.link(globalVar, globalConst, exportedFunc,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_FALSE(linkResult);
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

    vISA::GenRelocEntry relocs[] = {relocA, relocB, relocC, relocCPartHigh, relocCPartLow};
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, 5, 0);
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
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
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
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
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
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(1U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_TRUE(unresolvedExternals[0].internalError);

    patchableInstructionSegments[0].segmentSize = relocA.r_offset + sizeof(uintptr_t);
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                             unresolvedExternals);
    EXPECT_TRUE(linkResult);
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
    NEO::Linker::PatchableSegment patchableGlobalVarSeg, patchableConstVarSeg;

    ASSERT_EQ(1U, linkerInput.symbols.count("A"));
    linkerInput.symbols["A"].segment = NEO::SegmentType::Unknown;
    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                                  unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(0U, unresolvedExternals.size());

    linkerInput.symbols["A"].segment = NEO::SegmentType::GlobalVariables;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableGlobalVarSeg, patchableConstVarSeg, patchableInstructionSegments,
                             unresolvedExternals);
    EXPECT_TRUE(linkResult);
}

TEST(LinkerTests, givenInvalidSourceSegmentWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::Linker::PatchableSegment emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::Linker::PatchableSegment nonEmptypatchableSegment;
    nonEmptypatchableSegment.hostPointer = nonEmptypatchableSegmentData.data();
    nonEmptypatchableSegment.segmentSize = nonEmptypatchableSegmentData.size();

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
        bool linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                      nonEmptypatchableSegment, emptyPatchableSegment, {},
                                      unresolvedExternals);

        EXPECT_FALSE(linkResult);
        EXPECT_EQ(1U, unresolvedExternals.size());

        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
        unresolvedExternals.clear();
        linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                 nonEmptypatchableSegment, emptyPatchableSegment, {},
                                 unresolvedExternals);

        EXPECT_TRUE(linkResult);
        EXPECT_EQ(0U, unresolvedExternals.size());
    }

    {
        linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = false;
        linkerInput.traits.requiresPatchingOfGlobalConstantsBuffer = true;
        linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalConstants;
        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::Unknown;
        unresolvedExternals.clear();
        bool linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                      emptyPatchableSegment, nonEmptypatchableSegment, {},
                                      unresolvedExternals);

        EXPECT_FALSE(linkResult);
        EXPECT_EQ(1U, unresolvedExternals.size());

        linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
        unresolvedExternals.clear();
        linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                 emptyPatchableSegment, nonEmptypatchableSegment, {},
                                 unresolvedExternals);

        EXPECT_TRUE(linkResult);
        EXPECT_EQ(0U, unresolvedExternals.size());
    }
}

TEST(LinkerTests, givenUnknownRelocationSegmentWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::Linker::PatchableSegment emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::Linker::PatchableSegment nonEmptypatchableSegment;
    nonEmptypatchableSegment.hostPointer = nonEmptypatchableSegmentData.data();
    nonEmptypatchableSegment.segmentSize = nonEmptypatchableSegmentData.size();

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::Unknown;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    bool linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  nonEmptypatchableSegment, emptyPatchableSegment, {},
                                  unresolvedExternals);

    EXPECT_FALSE(linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             nonEmptypatchableSegment, emptyPatchableSegment, {},
                             unresolvedExternals);

    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
}

TEST(LinkerTests, givenRelocationTypeWithHighPartOfAddressWhenPatchingDataSegmentsThenLinkerFails) {
    WhiteBox<NEO::LinkerInput> linkerInput;
    NEO::Linker linker(linkerInput);

    NEO::Linker::SegmentInfo emptySegmentInfo;
    NEO::Linker::PatchableSegment emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::Linker::PatchableSegment nonEmptypatchableSegment;
    nonEmptypatchableSegment.hostPointer = nonEmptypatchableSegmentData.data();
    nonEmptypatchableSegment.segmentSize = nonEmptypatchableSegmentData.size();

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::AddressHigh;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    bool linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  nonEmptypatchableSegment, emptyPatchableSegment, {},
                                  unresolvedExternals);

    EXPECT_FALSE(linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             nonEmptypatchableSegment, emptyPatchableSegment, {},
                             unresolvedExternals);

    EXPECT_TRUE(linkResult);
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

    NEO::Linker::PatchableSegment globalConstantsPatchableSegment, globalVariablesPatchableSegment;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsSegmentData.data());
    globalConstantsSegmentInfo.segmentSize = globalConstantsSegmentData.size();
    globalConstantsPatchableSegment.hostPointer = globalConstantsSegmentData.data();
    globalConstantsPatchableSegment.segmentSize = globalConstantsSegmentData.size();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesSegmentData.data());
    globalVariablesSegmentInfo.segmentSize = globalVariablesSegmentData.size();
    globalVariablesPatchableSegment.hostPointer = globalVariablesSegmentData.data();
    globalVariablesPatchableSegment.segmentSize = globalVariablesSegmentData.size();

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
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
        if (reloc.type == NEO::LinkerInput::RelocationInfo::Type::Address) {
            *reinterpret_cast<uintptr_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        } else {
            *reinterpret_cast<uint32_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        }
        ++initValue;
    }

    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {},
                                  globalVariablesPatchableSegment, globalConstantsPatchableSegment, {},
                                  unresolvedExternals);
    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(7U, *reinterpret_cast<uint8_t *>(globalConstantsPatchableSegment.hostPointer));
    EXPECT_EQ(13U, *reinterpret_cast<uint8_t *>(globalVariablesPatchableSegment.hostPointer));

    initValue = 0;
    for (const auto &reloc : relocationInfo) {
        void *srcRaw = (reloc.symbolSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
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

    NEO::Linker::PatchableSegment globalConstantsPatchableSegment, globalVariablesPatchableSegment;
    globalConstantsSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalConstantsSegmentData.data());
    globalConstantsSegmentInfo.segmentSize = globalConstantsSegmentData.size();
    globalConstantsPatchableSegment.hostPointer = globalConstantsSegmentData.data();
    globalConstantsPatchableSegment.segmentSize = globalConstantsSegmentData.size();

    globalVariablesSegmentInfo.gpuAddress = reinterpret_cast<uintptr_t>(globalVariablesSegmentData.data());
    globalVariablesSegmentInfo.segmentSize = globalVariablesSegmentData.size();
    globalVariablesPatchableSegment.hostPointer = globalVariablesSegmentData.data();
    globalVariablesPatchableSegment.segmentSize = globalVariablesSegmentData.size();

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
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
        *reinterpret_cast<uint32_t *>(ptrOffset(dstRaw, static_cast<size_t>(reloc.offset))) = initValue * 4; // relocations to global data are currently based on patchIncrement, simulate init data
        ++initValue;
    }

    auto linkResult = linker.link(globalVariablesSegmentInfo, globalConstantsSegmentInfo, {},
                                  globalVariablesPatchableSegment, globalConstantsPatchableSegment, {},
                                  unresolvedExternals);
    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(7U, *reinterpret_cast<uint8_t *>(globalConstantsPatchableSegment.hostPointer));
    EXPECT_EQ(13U, *reinterpret_cast<uint8_t *>(globalVariablesPatchableSegment.hostPointer));

    initValue = 0;
    for (const auto &reloc : relocationInfo) {
        void *srcRaw = (reloc.symbolSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
        void *dstRaw = (reloc.relocationSegment == NEO::SegmentType::GlobalVariables) ? globalVariablesPatchableSegment.hostPointer : globalConstantsPatchableSegment.hostPointer;
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
    NEO::Linker::PatchableSegment emptyPatchableSegment;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> nonEmptypatchableSegmentData;
    nonEmptypatchableSegmentData.resize(64, 8U);
    NEO::Linker::PatchableSegment nonEmptypatchableSegment;
    nonEmptypatchableSegment.hostPointer = nonEmptypatchableSegmentData.data();
    nonEmptypatchableSegment.segmentSize = nonEmptypatchableSegmentData.size();

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 64U;
    relocInfo.symbolName = "aaa";
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput.dataRelocations.push_back(relocInfo);
    linkerInput.dataRelocations[0].relocationSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.dataRelocations[0].symbolSegment = NEO::SegmentType::GlobalVariables;
    linkerInput.traits.requiresPatchingOfGlobalVariablesBuffer = true;

    bool linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                                  nonEmptypatchableSegment, emptyPatchableSegment, {},
                                  unresolvedExternals);

    EXPECT_FALSE(linkResult);
    EXPECT_EQ(1U, unresolvedExternals.size());

    linkerInput.dataRelocations[0].offset = 32;
    unresolvedExternals.clear();
    linkResult = linker.link(emptySegmentInfo, emptySegmentInfo, emptySegmentInfo,
                             nonEmptypatchableSegment, emptyPatchableSegment, {},
                             unresolvedExternals);

    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
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
