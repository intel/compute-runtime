/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "linker_mock.h"

#include <string>

#if __has_include("RelocationInfo.h")
#include "RelocationInfo.h"
#else
namespace vISA {
static const uint32_t MAX_SYMBOL_NAME_LENGTH = 256;

enum GenSymType {
    S_NOTYPE = 0,
    S_UNDEF = 1,
    S_FUNC = 2,
    S_GLOBAL_VAR = 3,
    S_GLOBAL_VAR_CONST = 4
};

typedef struct {
    uint32_t s_type;
    uint32_t s_offset;
    uint32_t s_size;
    char s_name[MAX_SYMBOL_NAME_LENGTH];
} GenSymEntry;

enum GenRelocType {
    R_NONE = 0,
    R_SYM_ADDR = 1
};

typedef struct {
    uint32_t r_type;
    uint32_t r_offset;
    char r_symbol[MAX_SYMBOL_NAME_LENGTH];
} GenRelocEntry;
} // namespace vISA
#endif

TEST(LinkerInputTests, givenGlobalsSymbolTableThenProperlyDecodesGlobalVariablesAndGlobalConstants) {
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

    auto decodeResult = linkerInput.decodeGlobalVariablesSymbolTable(entry, 2);
    EXPECT_TRUE(decodeResult);

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SymbolInfo::GlobalVariable, symbolA->second.type);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SymbolInfo::GlobalConstant, symbolB->second.type);

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

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_TRUE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_FALSE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SymbolInfo::GlobalVariable, symbolA->second.type);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SymbolInfo::GlobalConstant, symbolB->second.type);

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

    EXPECT_EQ(2U, linkerInput.getSymbols().size());
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalVariables);
    EXPECT_FALSE(linkerInput.getTraits().exportsGlobalConstants);
    EXPECT_TRUE(linkerInput.getTraits().exportsFunctions);
    EXPECT_FALSE(linkerInput.getTraits().requiresPatchingOfInstructionSegments);

    auto symbolA = linkerInput.getSymbols().find("A");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolA);
    EXPECT_EQ(entry[0].s_offset, symbolA->second.offset);
    EXPECT_EQ(entry[0].s_size, symbolA->second.size);
    EXPECT_EQ(NEO::SymbolInfo::Function, symbolA->second.type);

    auto symbolB = linkerInput.getSymbols().find("B");
    ASSERT_NE(linkerInput.getSymbols().end(), symbolB);
    EXPECT_EQ(entry[1].s_offset, symbolB->second.offset);
    EXPECT_EQ(entry[1].s_size, symbolB->second.size);
    EXPECT_EQ(NEO::SymbolInfo::Function, symbolB->second.type);

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
}

TEST(LinkerInputTests, givenRelocationTableThenNoneIsNotAllowed) {
    NEO::LinkerInput linkerInput;
    vISA::GenRelocEntry entry = {};
    entry.r_symbol[0] = 'A';
    entry.r_offset = 8;
    entry.r_type = vISA::GenRelocType::R_NONE;

    auto decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 3);
    EXPECT_FALSE(decodeResult);
}

TEST(LinkerTests, givenEmptyLinkerInputThenLinkerOutputIsEmpty) {
    NEO::LinkerInput linkerInput;
    NEO::Linker linker(linkerInput);
    NEO::Linker::Segment globalVar, globalConst, exportedFunc;
    NEO::Linker::PatchableSegments patchableInstructionSegments;
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    bool linkResult = linker.link(globalVar, globalConst, exportedFunc, patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
    EXPECT_EQ(0U, unresolvedExternals.size());
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
}

TEST(LinkerTests, givenUnresolvedExternalThenLinkerFails) {
    NEO::LinkerInput linkerInput;
    vISA::GenRelocEntry entry = {};
    entry.r_symbol[0] = 'A';
    entry.r_offset = 8;
    entry.r_type = vISA::GenRelocType::R_SYM_ADDR;
    auto decodeResult = linkerInput.decodeRelocationTable(&entry, 1, 0);
    EXPECT_TRUE(decodeResult);

    NEO::Linker linker(linkerInput);
    NEO::Linker::Segment globalVar, globalConst, exportedFunc;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    bool linkResult = linker.link(globalVar, globalConst, exportedFunc, patchableInstructionSegments, unresolvedExternals);
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

    vISA::GenRelocEntry relocs[] = {relocA, relocB, relocC};
    bool decodeRelocSuccess = linkerInput.decodeRelocationTable(&relocs, 3, 0);
    EXPECT_TRUE(decodeRelocSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::Segment globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    globalConstSegment.gpuAddress = 128;
    globalConstSegment.segmentSize = 256;
    exportedFuncSegment.gpuAddress = 4096;
    exportedFuncSegment.segmentSize = 1024;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableInstructionSegments, unresolvedExternals);
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
}

TEST(LinkerTests, givenInvalidSymbolOffsetThenRelocationFails) {
    NEO::LinkerInput linkerInput;

    vISA::GenSymEntry symGlobalVariable = {};
    symGlobalVariable.s_name[0] = 'A';
    symGlobalVariable.s_offset = 64;
    symGlobalVariable.s_size = 16;
    symGlobalVariable.s_type = vISA::GenSymType::S_GLOBAL_VAR;
    bool decodeSymSuccess = linkerInput.decodeGlobalVariablesSymbolTable(&symGlobalVariable, 1);
    EXPECT_TRUE(decodeSymSuccess);

    NEO::Linker linker(linkerInput);
    NEO::Linker::Segment globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = symGlobalVariable.s_offset;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableInstructionSegments, unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, unresolvedExternals.size());
    EXPECT_EQ(0U, relocatedSymbols.size());

    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
}

TEST(LinkerTests, givenInvalidRelocationOffsetThenPatchingOfIsaFails) {
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
    NEO::Linker::Segment globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = symGlobalVariable.s_offset + symGlobalVariable.s_size;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> instructionSegment;
    instructionSegment.resize(relocA.r_offset + sizeof(uintptr_t), 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = relocA.r_offset;
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableInstructionSegments, unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(1U, relocatedSymbols.size());
    ASSERT_EQ(1U, unresolvedExternals.size());
    EXPECT_TRUE(unresolvedExternals[0].internalError);

    patchableInstructionSegments[0].segmentSize = relocA.r_offset + sizeof(uintptr_t);
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
}

TEST(LinkerTests, givenUnknownSymbolTypeThenRelocationFails) {
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
    NEO::Linker::Segment globalVarSegment, globalConstSegment, exportedFuncSegment;
    globalVarSegment.gpuAddress = 8;
    globalVarSegment.segmentSize = 64;
    NEO::Linker::UnresolvedExternals unresolvedExternals;

    std::vector<char> instructionSegment;
    instructionSegment.resize(64, 0);
    NEO::Linker::PatchableSegment seg0;
    seg0.hostPointer = instructionSegment.data();
    seg0.segmentSize = instructionSegment.size();
    NEO::Linker::PatchableSegments patchableInstructionSegments{seg0};

    ASSERT_EQ(1U, linkerInput.symbols.count("A"));
    linkerInput.symbols["A"].type = NEO::SymbolInfo::Unknown;
    bool linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                                  patchableInstructionSegments, unresolvedExternals);
    EXPECT_FALSE(linkResult);
    auto relocatedSymbols = linker.extractRelocatedSymbols();
    EXPECT_EQ(0U, relocatedSymbols.size());
    ASSERT_EQ(0U, unresolvedExternals.size());

    linkerInput.symbols["A"].type = NEO::SymbolInfo::GlobalVariable;
    linkResult = linker.link(globalVarSegment, globalConstSegment, exportedFuncSegment,
                             patchableInstructionSegments, unresolvedExternals);
    EXPECT_TRUE(linkResult);
}

TEST(LinkerErrorMessageTests, whenNoUnresolvedExternalsThenInternalError) {
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    std::vector<std::string> segmentsNames{"kernel1", "kernel2"};
    auto err = NEO::constructLinkerErrorMessage(unresolvedExternals, segmentsNames);
    EXPECT_EQ(std::string("Internal linker error"), err);
}

TEST(LinkerErrorMessageTests, whenUnresolvedExternalsThenSymbolNameInErrorMessage) {
    NEO::Linker::UnresolvedExternals unresolvedExternals;
    std::vector<std::string> segmentsNames{"kernel1", "kernel2"};
    NEO::Linker::UnresolvedExternal unresolvedExternal = {};
    unresolvedExternal.instructionsSegmentId = 1;
    unresolvedExternal.internalError = false;
    unresolvedExternal.unresolvedRelocation.offset = 64;
    unresolvedExternal.unresolvedRelocation.symbolName = "arrayABC";
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
}
