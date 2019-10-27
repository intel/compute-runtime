/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "runtime/compiler_interface/patchtokens_validator.inl"
#include "test.h"

#include "patchtokens_tests.h"

struct UknownTokenValidator {
    UknownTokenValidator(bool isSafeToSkip = true) : isSafeToSkip(isSafeToSkip) {
    }
    bool isSafeToSkipUnhandledToken(uint32_t token) const {
        return isSafeToSkip;
    }
    bool isSafeToSkip = true;
};

TEST(PatchtokensValidator, GivenValidProgramThenValidationSucceeds) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidOrUnknownStatusThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.decodeStatus = NEO::PatchTokenBinary::DecoderError::Undefined;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("ProgramFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_EQ(0U, warning.size());

    error.clear();
    warning.clear();

    prog.decodeStatus = NEO::PatchTokenBinary::DecoderError::InvalidBinary;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("ProgramFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithASingleConstantSurfaceThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithConstantSurface prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithMultipleConstantSurfacesThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurface prog;
    std::string error, warning;

    iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo constSurface2 = *prog.programScopeTokens.allocateConstantMemorySurface[0];
    prog.programScopeTokens.allocateConstantMemorySurface.push_back(&constSurface2);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled number of global constants surfaces > 1", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithASingleGlobalSurfaceThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithGlobalSurface prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithMultipleGlobalSurfacesThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurface prog;
    std::string error, warning;

    iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo globSurface2 = *prog.programScopeTokens.allocateGlobalMemorySurface[0];
    prog.programScopeTokens.allocateGlobalMemorySurface.push_back(&globSurface2);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled number of global variables surfaces > 1", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithValidConstantPointerThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->BufferIndex = 1;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerConstantBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->ConstantBufferIndex = 1;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerBufferTypeThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->BufferType = iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidConstantPointerOffsetThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.constantPointerMutable->ConstantPointerOffset = prog.constSurfMutable->InlineDataSize;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithoutConstantSurfaceButWithConstantPointerThenValidationFails) {
    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer prog;
    std::string error, warning;

    prog.programScopeTokens.allocateConstantMemorySurface.clear();
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchConstantPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithValidGlobalPointerThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->BufferIndex = 1;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerGlobalBufferIndexThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->GlobalBufferIndex = 1;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerBufferTypeThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->BufferType = iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithInvalidGlobalPointerOffsetThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.globalPointerMutable->GlobalPointerOffset = prog.globalSurfMutable->InlineDataSize;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithoutGlobalSurfaceButWithGlobalPointerThenValidationFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurfaceAndPointer prog;
    std::string error, warning;

    prog.programScopeTokens.allocateGlobalMemorySurface.clear();
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("Unhandled SPatchGlobalPointerProgramBinaryInfo", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithUnknownPatchTokenWhenUknownTokenCantBeSkippedThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.unhandledTokens.push_back(&unknownToken);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(false), error, warning));
    auto expectedError = "Unhandled required program-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithUnknownPatchTokenWhenUknownTokenCanBeSkippedThenValidationSucceedsAndEmitsWarning) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.unhandledTokens.push_back(&unknownToken);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(true), error, warning));
    auto expectedWarning = "Unknown program-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_EQ(0U, error.size());
    EXPECT_STREQ(expectedWarning.c_str(), warning.c_str());
}

TEST(PatchtokensValidator, GivenProgramWithUnsupportedPatchTokenVersionThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.headerMutable->Version = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(true), error, warning));
    auto expectedError = "Unhandled Version of Patchtokens: expected: " + std::to_string(iOpenCL::CURRENT_ICBE_VERSION) + ", got: " + std::to_string(prog.header->Version);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithUnsupportedPlatformThenValidationFails) {
    PatchTokensTestData::ValidEmptyProgram prog;
    std::string error, warning;

    prog.headerMutable->Device = IGFX_MAX_CORE;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(true), error, warning));
    auto expectedError = "Unsupported device binary, device GFXCORE_FAMILY : " + std::to_string(prog.header->Device);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelHasInvalidOrUnknownStatusThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    prog.kernels[0].decodeStatus = NEO::PatchTokenBinary::DecoderError::Undefined;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("KernelFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_EQ(0U, warning.size());

    error.clear();
    warning.clear();

    prog.kernels[0].decodeStatus = NEO::PatchTokenBinary::DecoderError::InvalidBinary;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("KernelFromPatchtokens wasn't successfully decoded", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithKernelWhenKernelHasInvalidChecksumThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    prog.kernelHeaderMutable->CheckSum += 1;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("KernelFromPatchtokens has invalid checksum", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelUsingSlmThenValidationSucceeds) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm prog;
    std::string error, warning;

    size_t slmSizeAvailable = 1 + prog.kernels[0].tokens.allocateLocalSurface->TotalInlineLocalMemorySize;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, slmSizeAvailable, UknownTokenValidator(), error, warning));
    EXPECT_EQ(0U, error.size());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenProgramWithKernelUsingSlmWhenKernelRequiresTooMuchSlmThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm prog;
    std::string error, warning;

    size_t slmSizeAvailable = -1 + prog.kernels[0].tokens.allocateLocalSurface->TotalInlineLocalMemorySize;
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::NotEnoughSlm, NEO::PatchTokenBinary::validate(prog, slmSizeAvailable, UknownTokenValidator(), error, warning));
    EXPECT_STREQ("KernelFromPatchtokens requires too much SLM", error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelContainingUnknownPatchTokenWhenUknownTokenCantBeSkippedThenValidationFails) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.kernels[0].unhandledTokens.push_back(&unknownToken);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::InvalidBinary, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(false), error, warning));
    auto expectedError = "Unhandled required kernel-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_STREQ(expectedError.c_str(), error.c_str());
    EXPECT_EQ(0U, warning.size());
}

TEST(PatchtokensValidator, GivenValidProgramWithKernelContainingUnknownPatchTokenWhenUknownTokenCanBeSkippedThenValidationSucceedsAndEmitsWarning) {
    PatchTokensTestData::ValidProgramWithKernel prog;
    std::string error, warning;

    iOpenCL::SPatchItemHeader unknownToken = {};
    unknownToken.Token = iOpenCL::NUM_PATCH_TOKENS + 1;
    prog.kernels[0].unhandledTokens.push_back(&unknownToken);
    EXPECT_EQ(NEO::PatchTokenBinary::ValidatorError::Success, NEO::PatchTokenBinary::validate(prog, 0U, UknownTokenValidator(true), error, warning));
    auto expectedWarning = "Unknown kernel-scope Patch Token : " + std::to_string(unknownToken.Token);
    EXPECT_EQ(0U, error.size());
    EXPECT_STREQ(expectedWarning.c_str(), warning.c_str());
}
