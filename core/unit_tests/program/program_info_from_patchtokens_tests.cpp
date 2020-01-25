/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/compiler_interface/linker.h"
#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/program/program_info.h"
#include "core/program/program_info_from_patchtokens.h"
#include "core/unit_tests/compiler_interface/linker_mock.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "runtime/program/kernel_info.h"

#include "RelocationInfo.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(PopulateProgramInfoFromPatchtokensTests, WhenRequiresLocalMemoryWindowVAIsCalledThenReturnsTrueOnlyIfAnyOfKernelsRequireLocalMemoryWindowVA) {
    NEO::PatchTokenBinary::ProgramFromPatchtokens emptyProgram;
    EXPECT_FALSE(NEO::requiresLocalMemoryWindowVA(emptyProgram));

    NEO::PatchTokenBinary::KernelFromPatchtokens kernelWithoutRequirementForLocalMemoryWindowWA;
    NEO::PatchTokenBinary::KernelFromPatchtokens kernelWithRequirementForLocalMemoryWindowWA;
    iOpenCL::SPatchDataParameterBuffer param;
    kernelWithRequirementForLocalMemoryWindowWA.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress = &param;

    NEO::PatchTokenBinary::ProgramFromPatchtokens programWithOneKernelWithoutLocalMemWindow;
    programWithOneKernelWithoutLocalMemWindow.kernels.push_back(kernelWithoutRequirementForLocalMemoryWindowWA);
    EXPECT_FALSE(NEO::requiresLocalMemoryWindowVA(programWithOneKernelWithoutLocalMemWindow));

    NEO::PatchTokenBinary::ProgramFromPatchtokens programWithOneKernelWithLocalMemWindow;
    programWithOneKernelWithLocalMemWindow.kernels.push_back(kernelWithRequirementForLocalMemoryWindowWA);
    EXPECT_TRUE(NEO::requiresLocalMemoryWindowVA(programWithOneKernelWithLocalMemWindow));

    NEO::PatchTokenBinary::ProgramFromPatchtokens programWithTwoKernelsWithLocalMemWindow;
    programWithTwoKernelsWithLocalMemWindow.kernels.push_back(kernelWithoutRequirementForLocalMemoryWindowWA);
    programWithTwoKernelsWithLocalMemWindow.kernels.push_back(kernelWithRequirementForLocalMemoryWindowWA);
    EXPECT_TRUE(NEO::requiresLocalMemoryWindowVA(programWithTwoKernelsWithLocalMemWindow));

    std::swap(programWithTwoKernelsWithLocalMemWindow.kernels[0], programWithTwoKernelsWithLocalMemWindow.kernels[1]);
    EXPECT_TRUE(NEO::requiresLocalMemoryWindowVA(programWithTwoKernelsWithLocalMemWindow));
}

TEST(PopulateProgramInfoFromPatchtokensTests, WhenProgramRequiresGlobalConstantsSurfaceThenGlobalConstantsSectionIsPopulated) {
    PatchTokensTestData::ValidProgramWithConstantSurface programTokens;
    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programTokens);
    EXPECT_EQ(programInfo.globalConstants.size, programTokens.programScopeTokens.allocateConstantMemorySurface[0]->InlineDataSize);
    EXPECT_EQ(programInfo.globalConstants.initData, NEO::PatchTokenBinary::getInlineData(programTokens.programScopeTokens.allocateConstantMemorySurface[0]));
    EXPECT_TRUE(programInfo.kernelInfos.empty());
    EXPECT_EQ(nullptr, programInfo.linkerInput);
}

TEST(PopulateProgramInfoFromPatchtokensTests, WhenProgramRequiresGlobalVariablesSurfaceThenGlobalConstantsSectionIsPopulated) {
    PatchTokensTestData::ValidProgramWithGlobalSurface programTokens;
    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programTokens);
    EXPECT_EQ(programInfo.globalVariables.size, programTokens.programScopeTokens.allocateGlobalMemorySurface[0]->InlineDataSize);
    EXPECT_EQ(programInfo.globalVariables.initData, NEO::PatchTokenBinary::getInlineData(programTokens.programScopeTokens.allocateGlobalMemorySurface[0]));
    EXPECT_TRUE(programInfo.kernelInfos.empty());
    EXPECT_EQ(nullptr, programInfo.linkerInput);
}

TEST(PopulateProgramInfoFromPatchtokensTests, WhenProgramRequiresGlobalConstantsPointersThenLinkerInputContainsProperRelocations) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer programFromTokens;
    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programFromTokens);
    EXPECT_TRUE(programInfo.kernelInfos.empty());
    ASSERT_NE(nullptr, programInfo.linkerInput);
    ASSERT_EQ(1U, programInfo.linkerInput->getDataRelocations().size());
    auto relocation = programInfo.linkerInput->getDataRelocations()[0];
    EXPECT_EQ(programFromTokens.programScopeTokens.constantPointer[0]->ConstantPointerOffset, relocation.offset);
    EXPECT_TRUE(relocation.symbolName.empty());
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocation.relocationSegment);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocation.symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocation.type);
}

TEST(PopulateProgramInfoFromPatchtokensTests, WhenProgramRequiresGlobalVariablesPointersThenLinkerInputContainsProperRelocations) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer programFromTokens;
    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programFromTokens);
    EXPECT_TRUE(programInfo.kernelInfos.empty());
    ASSERT_NE(nullptr, programInfo.linkerInput);
    ASSERT_EQ(1U, programInfo.linkerInput->getDataRelocations().size());
    auto relocation = programInfo.linkerInput->getDataRelocations()[0];
    EXPECT_EQ(NEO::readMisalignedUint64(&programFromTokens.programScopeTokens.globalPointer[0]->GlobalPointerOffset), relocation.offset);
    EXPECT_TRUE(relocation.symbolName.empty());
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocation.relocationSegment);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocation.symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocation.type);
}

TEST(PopulateProgramInfoFromPatchtokensTests, WhenProgramRequiresMixedGlobalVarAndConstPointersThenLinkerInputContainsProperRelocations) {
    PatchTokensTestData::ValidProgramWithMixedGlobalVarAndConstSurfacesAndPointers programFromTokens;
    NEO::ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programFromTokens);
    EXPECT_TRUE(programInfo.kernelInfos.empty());
    ASSERT_NE(nullptr, programInfo.linkerInput);

    const NEO::LinkerInput::RelocationInfo *relocationGlobalConst, *relocationGlobalVar;
    ASSERT_EQ(2U, programInfo.linkerInput->getDataRelocations().size());
    for (const auto &reloc : programInfo.linkerInput->getDataRelocations()) {
        switch (reloc.relocationSegment) {
        default:
            break;
        case NEO::SegmentType::GlobalConstants:
            relocationGlobalConst = &reloc;
            break;
        case NEO::SegmentType::GlobalVariables:
            relocationGlobalVar = &reloc;
            break;
        }
    }

    ASSERT_NE(nullptr, relocationGlobalConst);
    EXPECT_EQ(NEO::readMisalignedUint64(&programFromTokens.programScopeTokens.constantPointer[0]->ConstantPointerOffset), relocationGlobalConst->offset);
    EXPECT_TRUE(relocationGlobalConst->symbolName.empty());
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocationGlobalConst->relocationSegment);
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocationGlobalConst->symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocationGlobalConst->type);

    ASSERT_NE(nullptr, relocationGlobalVar);
    EXPECT_EQ(NEO::readMisalignedUint64(&programFromTokens.programScopeTokens.globalPointer[0]->GlobalPointerOffset), relocationGlobalVar->offset);
    EXPECT_TRUE(relocationGlobalVar->symbolName.empty());
    EXPECT_EQ(NEO::SegmentType::GlobalVariables, relocationGlobalVar->relocationSegment);
    EXPECT_EQ(NEO::SegmentType::GlobalConstants, relocationGlobalVar->symbolSegment);
    EXPECT_EQ(NEO::LinkerInput::RelocationInfo::Type::Address, relocationGlobalVar->type);
}

TEST(PopulateProgramInfoFromPatchtokensTests, GivenProgramWithSpecificPointerSizeThenLinkerIsUpdatedToUseRequiredPointerSize) {
    PatchTokensTestData::ValidProgramWithMixedGlobalVarAndConstSurfacesAndPointers programFromTokens;
    {
        programFromTokens.headerMutable->GPUPointerSizeInBytes = 8;
        NEO::ProgramInfo programInfo;
        NEO::populateProgramInfo(programInfo, programFromTokens);
        ASSERT_NE(nullptr, programInfo.linkerInput);
        EXPECT_EQ(NEO::LinkerInput::Traits::Ptr64bit, programInfo.linkerInput->getTraits().pointerSize);
    }

    {
        programFromTokens.headerMutable->GPUPointerSizeInBytes = 4;
        NEO::ProgramInfo programInfo;
        NEO::populateProgramInfo(programInfo, programFromTokens);
        ASSERT_NE(nullptr, programInfo.linkerInput);
        EXPECT_EQ(NEO::LinkerInput::Traits::Ptr32bit, programInfo.linkerInput->getTraits().pointerSize);
    }
}

TEST(PopulateProgramInfoFromPatchtokensTests, GivenProgramWithProgramSymbolTableThenLinkerDecodesAllSymbolsCorrectly) {
    PatchTokensTestData::ValidEmptyProgram programFromTokens;
    NEO::ProgramInfo programInfo = {};
    Mock<NEO::LinkerInput> *mockLinkerInput = new Mock<NEO::LinkerInput>;
    programInfo.linkerInput.reset(mockLinkerInput);

    vISA::GenSymEntry entry[1] = {};

    iOpenCL::SPatchFunctionTableInfo symbolTable = {};
    symbolTable.NumEntries = 1;
    std::vector<uint8_t> symbolTableTokenStorage = {};
    symbolTableTokenStorage.insert(symbolTableTokenStorage.end(),
                                   reinterpret_cast<uint8_t *>(&symbolTable), reinterpret_cast<uint8_t *>(&symbolTable + 1));
    symbolTableTokenStorage.insert(symbolTableTokenStorage.end(),
                                   reinterpret_cast<uint8_t *>(entry), reinterpret_cast<uint8_t *>(entry + 1));
    programFromTokens.programScopeTokens.symbolTable = reinterpret_cast<iOpenCL::SPatchFunctionTableInfo *>(symbolTableTokenStorage.data());

    const void *receivedData = nullptr;
    uint32_t receivedNumEntries = 0U;
    mockLinkerInput->decodeGlobalVariablesSymbolTableMockConfig.overrideFunc = [&](Mock<NEO::LinkerInput> *, const void *data, uint32_t numEntries) -> bool {
        receivedData = data;
        receivedNumEntries = numEntries;
        return true;
    };
    NEO::populateProgramInfo(programInfo, programFromTokens);

    ASSERT_EQ(mockLinkerInput, programInfo.linkerInput.get());
    EXPECT_EQ(1U, mockLinkerInput->decodeGlobalVariablesSymbolTableMockConfig.timesCalled);
    EXPECT_EQ(reinterpret_cast<void *>(symbolTableTokenStorage.data() + sizeof(iOpenCL::SPatchFunctionTableInfo)), receivedData);
    EXPECT_EQ(1U, receivedNumEntries);
}

TEST(PopulateProgramInfoFromPatchtokensTests, GivenProgramWithKernelsThenKernelInfosIsProperlyPopulated) {
    PatchTokensTestData::ValidProgramWithKernel programFromTokens;
    programFromTokens.kernels.push_back(programFromTokens.kernels[0]);
    programFromTokens.kernels.push_back(programFromTokens.kernels[0]);
    NEO::ProgramInfo programInfo = {};
    NEO::populateProgramInfo(programInfo, programFromTokens);
    ASSERT_EQ(3U, programInfo.kernelInfos.size());
    EXPECT_EQ(programFromTokens.header->GPUPointerSizeInBytes, programInfo.kernelInfos[0]->gpuPointerSize);
    EXPECT_EQ(programFromTokens.header->GPUPointerSizeInBytes, programInfo.kernelInfos[1]->gpuPointerSize);
    EXPECT_EQ(programFromTokens.header->GPUPointerSizeInBytes, programInfo.kernelInfos[2]->gpuPointerSize);
}

TEST(PopulateProgramInfoFromPatchtokensTests, GivenProgramWithKernelsWhenKernelHasSymbolTableThenLinkerIsUpdatedWithAdditionalSymbolInfo) {
    NEO::ProgramInfo programInfo = {};
    Mock<NEO::LinkerInput> *mockLinkerInput = new Mock<NEO::LinkerInput>;
    programInfo.linkerInput.reset(mockLinkerInput);

    PatchTokensTestData::ValidProgramWithKernel programFromTokens;
    programFromTokens.kernels.push_back(programFromTokens.kernels[0]);

    vISA::GenSymEntry entryA[1] = {};

    iOpenCL::SPatchFunctionTableInfo symbolTableA = {};
    symbolTableA.NumEntries = 1;
    std::vector<uint8_t> symbolTableATokenStorage = {};
    symbolTableATokenStorage.insert(symbolTableATokenStorage.end(),
                                    reinterpret_cast<uint8_t *>(&symbolTableA), reinterpret_cast<uint8_t *>(&symbolTableA + 1));
    symbolTableATokenStorage.insert(symbolTableATokenStorage.end(),
                                    reinterpret_cast<uint8_t *>(entryA), reinterpret_cast<uint8_t *>(entryA + 1));
    programFromTokens.kernels[0].tokens.programSymbolTable = reinterpret_cast<iOpenCL::SPatchFunctionTableInfo *>(symbolTableATokenStorage.data());

    vISA::GenSymEntry entryB[2] = {};

    iOpenCL::SPatchFunctionTableInfo symbolTableB = {};
    symbolTableB.NumEntries = 2;
    std::vector<uint8_t> symbolTableBTokenStorage = {};
    symbolTableBTokenStorage.insert(symbolTableBTokenStorage.end(),
                                    reinterpret_cast<uint8_t *>(&symbolTableB), reinterpret_cast<uint8_t *>(&symbolTableB + 1));
    symbolTableBTokenStorage.insert(symbolTableBTokenStorage.end(),
                                    reinterpret_cast<uint8_t *>(entryB), reinterpret_cast<uint8_t *>(entryB + 2));
    programFromTokens.kernels[1].tokens.programSymbolTable = reinterpret_cast<iOpenCL::SPatchFunctionTableInfo *>(symbolTableBTokenStorage.data());

    std::vector<const void *> receivedData;
    std::vector<uint32_t> receivedNumEntries;
    std::vector<uint32_t> receivedSegmentIds;
    mockLinkerInput->decodeExportedFunctionsSymbolTableMockConfig.overrideFunc = [&](Mock<NEO::LinkerInput> *, const void *data, uint32_t numEntries, uint32_t instructionsSegmentId) -> bool {
        receivedData.push_back(data);
        receivedNumEntries.push_back(numEntries);
        receivedSegmentIds.push_back(instructionsSegmentId);
        return true;
    };
    NEO::populateProgramInfo(programInfo, programFromTokens);
    ASSERT_EQ(2U, mockLinkerInput->decodeExportedFunctionsSymbolTableMockConfig.timesCalled);
    ASSERT_EQ(2U, receivedData.size());

    EXPECT_EQ(1U, receivedNumEntries[0]);
    EXPECT_EQ(2U, receivedNumEntries[1]);
    EXPECT_EQ(reinterpret_cast<void *>(symbolTableATokenStorage.data() + sizeof(iOpenCL::SPatchFunctionTableInfo)), receivedData[0]);
    EXPECT_EQ(reinterpret_cast<void *>(symbolTableBTokenStorage.data() + sizeof(iOpenCL::SPatchFunctionTableInfo)), receivedData[1]);
    EXPECT_EQ(0U, receivedSegmentIds[0]);
    EXPECT_EQ(1U, receivedSegmentIds[1]);
}

TEST(PopulateProgramInfoFromPatchtokensTests, GivenProgramWithKernelsWhenKernelHasRelocationTableThenLinkerIsUpdatedWithAdditionalRelocationInfo) {
    NEO::ProgramInfo programInfo = {};
    Mock<NEO::LinkerInput> *mockLinkerInput = new Mock<NEO::LinkerInput>;
    programInfo.linkerInput.reset(mockLinkerInput);

    PatchTokensTestData::ValidProgramWithKernel programFromTokens;
    programFromTokens.kernels.push_back(programFromTokens.kernels[0]);

    vISA::GenRelocEntry entryA[1] = {};

    iOpenCL::SPatchFunctionTableInfo relocationTableA = {};
    relocationTableA.NumEntries = 1;
    std::vector<uint8_t> relocationTableATokenStorage = {};
    relocationTableATokenStorage.insert(relocationTableATokenStorage.end(),
                                        reinterpret_cast<uint8_t *>(&relocationTableA), reinterpret_cast<uint8_t *>(&relocationTableA + 1));
    relocationTableATokenStorage.insert(relocationTableATokenStorage.end(),
                                        reinterpret_cast<uint8_t *>(entryA), reinterpret_cast<uint8_t *>(entryA + 1));
    programFromTokens.kernels[0].tokens.programRelocationTable = reinterpret_cast<iOpenCL::SPatchFunctionTableInfo *>(relocationTableATokenStorage.data());

    vISA::GenRelocEntry entryB[2] = {};

    iOpenCL::SPatchFunctionTableInfo relocationTableB = {};
    relocationTableB.NumEntries = 2;
    std::vector<uint8_t> relocationTableBTokenStorage = {};
    relocationTableBTokenStorage.insert(relocationTableBTokenStorage.end(),
                                        reinterpret_cast<uint8_t *>(&relocationTableB), reinterpret_cast<uint8_t *>(&relocationTableB + 1));
    relocationTableBTokenStorage.insert(relocationTableBTokenStorage.end(),
                                        reinterpret_cast<uint8_t *>(entryB), reinterpret_cast<uint8_t *>(entryB + 2));
    programFromTokens.kernels[1].tokens.programRelocationTable = reinterpret_cast<iOpenCL::SPatchFunctionTableInfo *>(relocationTableBTokenStorage.data());

    std::vector<const void *> receivedData;
    std::vector<uint32_t> receivedNumEntries;
    std::vector<uint32_t> receivedSegmentIds;
    mockLinkerInput->decodeRelocationTableMockConfig.overrideFunc = [&](Mock<NEO::LinkerInput> *, const void *data, uint32_t numEntries, uint32_t instructionsSegmentId) -> bool {
        receivedData.push_back(data);
        receivedNumEntries.push_back(numEntries);
        receivedSegmentIds.push_back(instructionsSegmentId);
        return true;
    };
    NEO::populateProgramInfo(programInfo, programFromTokens);
    ASSERT_EQ(2U, mockLinkerInput->decodeRelocationTableMockConfig.timesCalled);
    ASSERT_EQ(2U, receivedData.size());

    EXPECT_EQ(1U, receivedNumEntries[0]);
    EXPECT_EQ(2U, receivedNumEntries[1]);
    EXPECT_EQ(reinterpret_cast<void *>(relocationTableATokenStorage.data() + sizeof(iOpenCL::SPatchFunctionTableInfo)), receivedData[0]);
    EXPECT_EQ(reinterpret_cast<void *>(relocationTableBTokenStorage.data() + sizeof(iOpenCL::SPatchFunctionTableInfo)), receivedData[1]);
    EXPECT_EQ(0U, receivedSegmentIds[0]);
    EXPECT_EQ(1U, receivedSegmentIds[1]);
}
