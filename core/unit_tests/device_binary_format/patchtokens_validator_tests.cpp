/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/device_binary_format/patchtokens_validator.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(PatchtokensValidator, GivenValidProgramThenValidationSucceeds) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidOrUnknownStatusThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.decodeStatus = NEO::DecodeError::Undefined;
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("ProgramFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    warning.clear();

    prog.decodeStatus = NEO::DecodeError::InvalidBinary;
    EXPECT_EQ(NEO::DecodeError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("ProgramFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithASingleConstantSurfaceThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithConstantSurface prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithMultipleConstantSurfacesThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurface prog;
    std::string error, warning;

    iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo constSurface2 = *prog.programScopeTokens.allocateConstantMemorySurface[0];
    prog.programScopeTokens.allocateConstantMemorySurface.push_back(&constSurface2);
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled number of global constants surfaces > 1", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithASingleGlobalSurfaceThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithGlobalSurface prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithMultipleGlobalSurfacesThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurface prog;
    std::string error, warning;

    iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo globSurface2 = *prog.programScopeTokens.allocateGlobalMemorySurface[0];
    prog.programScopeTokens.allocateGlobalMemorySurface.push_back(&globSurface2);
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled number of global variables surfaces > 1", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithValidConstantPointerThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->BufferIndex = 1;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGpuPointerSizeThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.headerMutable->GPUPointerSizeInBytes = 0U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Invalid pointer size", error.c_str());
    EXPECT_TRUE(warning.empty());

    prog.headerMutable->GPUPointerSizeInBytes = 1U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 2U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 3U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 4U;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 5U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 6U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 7U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 8U;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));

    prog.headerMutable->GPUPointerSizeInBytes = 9U;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerConstantBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->ConstantBufferIndex = 1;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerBufferTypeThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->BufferType = iOpenCL::NUM_PROGRAM_SCOPE_BUFFER_TYPE;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerOffsetThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->ConstantPointerOffset = prog.constSurfMutable->InlineDataSize;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithoutConstantSurfaceButWithConstantPointerThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.programScopeTokens.allocateConstantMemorySurface.clear();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithValidGlobalPointerThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithMixedGlobalVarAndConstSurfacesAndPointersThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithMixedGlobalVarAndConstSurfacesAndPointers prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenInvalidProgramWithMixedGlobalVarAndConstSurfacesAndPointersThenValidationFails) {
    std::string error, warning;

    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer progGlobalVar;
    progGlobalVar.globalPointerMutable->BufferType = iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(progGlobalVar, error, warning));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(warning.empty());

    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer progConstVar;
    progConstVar.constantPointerMutable->BufferType = iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(progConstVar, error, warning));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->BufferIndex = 1;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerGlobalBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->GlobalBufferIndex = 1;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerBufferTypeThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->BufferType = iOpenCL::NUM_PROGRAM_SCOPE_BUFFER_TYPE;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerOffsetThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->GlobalPointerOffset = prog.globalSurfMutable->InlineDataSize;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithoutGlobalSurfaceButWithGlobalPointerThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.programScopeTokens.allocateGlobalMemorySurface.clear();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithUnknownPatchTokenWhenUknownTokenCantBeSkippedThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.unhandledTokens.push_back(&unknownToken);
    auto prevValue = NEO::PatchTokenBinary::allowUnhandledTokens;
    NEO::PatchTokenBinary::allowUnhandledTokens = false;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    NEO::PatchTokenBinary::allowUnhandledTokens = prevValue;
    auto expectedError = "Unhandled required program-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithUnknownPatchTokenWhenUknownTokenCanBeSkippedThenValidationSucceedsAndEmitsWarning) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.unhandledTokens.push_back(&unknownToken);
    auto prevValue = NEO::PatchTokenBinary::allowUnhandledTokens;
    NEO::PatchTokenBinary::allowUnhandledTokens = true;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    NEO::PatchTokenBinary::allowUnhandledTokens = prevValue;
    auto expectedWarning = "Unknown program-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_TRUE(error.empty());
    EXPECT_STREQ(expectedWarning.c_str(), warning.c_str());
}

TEST(PatchtokensValidator, GivenProgramWithUnsupportedPatchTokenVersionThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.headerMutable->Version = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    auto expectedError = "Unhandled Version of Patchtokens: expected: " + std::to_string(iOpenCL::CURRENT_ICBE_VERSION) + ", got: " + std::to_string(prog.header->Version);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelHasInvalidOrUnknownStatusThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    prog.kernels[0].decodeStatus = NEO::DecodeError::Undefined;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("KernelFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    warning.clear();

    prog.kernels[0].decodeStatus = NEO::DecodeError::InvalidBinary;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("KernelFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelHasInvalidChecksumThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    prog.kernelHeaderMutable->CheckSum += 1;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ("KernelFromPatchtokens has invalid checksum", error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelUsingSlmThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm prog;
    std::string error, warning;

    //size_t slmSizeAvailable = 1 + prog.kernels[0].tokens.allocateLocalSurface->TotalInlineLocalMemorySize;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelContainingUnknownPatchTokenWhenUknownTokenCantBeSkippedThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.kernels[0].unhandledTokens.push_back(&unknownToken);
    auto prevValue = NEO::PatchTokenBinary::allowUnhandledTokens;
    NEO::PatchTokenBinary::allowUnhandledTokens = false;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    NEO::PatchTokenBinary::allowUnhandledTokens = prevValue;
    auto expectedError = "Unhandled required kernel-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelContainingUnknownPatchTokenWhenUknownTokenCanBeSkippedThenValidationSucceedsAndEmitsWarning) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.kernels[0].unhandledTokens.push_back(&unknownToken);
    auto prevValue = NEO::PatchTokenBinary::allowUnhandledTokens;
    NEO::PatchTokenBinary::allowUnhandledTokens = true;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    NEO::PatchTokenBinary::allowUnhandledTokens = prevValue;
    auto expectedWarning = "Unknown kernel-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_TRUE(error.empty());
    EXPECT_STREQ(expectedWarning.c_str(), warning.c_str());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelArgsHasProperQualifiersThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithKernelAndArg prog;
    std::string error, warning;

    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithKernelAndArgThenKernelArgInfoIsOptional) {
    PatchTokensTestData::ValidProgramWithKernelAndArg prog;
    std::string error, warning;
    prog.kernels[0].tokens.kernelArgs[0].argInfo = nullptr;
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelsArgHasUnknownAddressSpaceThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernelAndArg prog;
    std::string error, warning;
    prog.arg0InfoAddressQualifierMutable[2] = '\0';
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    auto expectedError = "Unhandled address qualifier";
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelsArgHasUnknownAccessQualifierThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernelAndArg prog;
    std::string error, warning;
    prog.arg0InfoAccessQualifierMutable[2] = '\0';
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    auto expectedError = "Unhandled access qualifier";
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenKernelWhenExecutionEnvironmentIsMissingThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;
    prog.kernels[0].tokens.executionEnvironment = nullptr;
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    auto expectedError = "Missing execution environment";
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenKernelWhenExecutionEnvironmentHasInvalidSimdSizeThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 0U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    auto expectedError = "Invalid LargestCompiledSIMDSize";
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 1U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 2U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 3U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 4U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 5U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 6U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 7U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 8U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 9U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::UnhandledBinary, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_STREQ(expectedError, error.c_str());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 16U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());

    error.clear();
    prog.kernelExecEnvMutable->LargestCompiledSIMDSize = 32U;
    prog.recalcTokPtr();
    EXPECT_EQ(NEO::DecodeError::Success, NEO::PatchTokenBinary::validate(prog, error, warning));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(warning.empty());
}

TEST(PatchtokensValidator, GivenDefaultStateThenUnhandledPatchtokensAreAllowed) {
    EXPECT_TRUE(NEO::PatchTokenBinary::allowUnhandledTokens);
}
