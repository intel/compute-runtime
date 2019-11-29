/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_info.h"
#include "runtime/compiler_interface/patchtokens_decoder.h"

#include "igfxfmid.h"

#include <string>

namespace NEO {

namespace PatchTokenBinary {

enum class ValidatorError {
    Success = 0,
    Undefined = 1,
    InvalidBinary = 2,
    NotEnoughSlm = 3,
};

constexpr bool isDeviceSupported(GFXCORE_FAMILY device) {
    return (device < (sizeof(familyEnabled) / sizeof(familyEnabled[0]))) && familyEnabled[device];
}

template <typename UknownTokenValidatorT>
inline ValidatorError validate(const ProgramFromPatchtokens &decodedProgram,
                               size_t sharedLocalMemorySize,
                               const UknownTokenValidatorT &tokenValidator,
                               std::string &outErrReason, std::string &outWarnings) {
    if (decodedProgram.decodeStatus != PatchTokenBinary::DecoderError::Success) {
        outErrReason = "ProgramFromPatchtokens wasn't successfully decoded";
        return ValidatorError::InvalidBinary;
    }

    if (decodedProgram.programScopeTokens.allocateConstantMemorySurface.size() > 1) {
        outErrReason = "Unhandled number of global constants surfaces > 1";
        return ValidatorError::InvalidBinary;
    }

    if (decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size() > 1) {
        outErrReason = "Unhandled number of global variables surfaces > 1";
        return ValidatorError::InvalidBinary;
    }

    for (const auto &globalConstantPointerToken : decodedProgram.programScopeTokens.constantPointer) {
        bool isUnhandled = (globalConstantPointerToken->ConstantBufferIndex != 0);
        isUnhandled |= (globalConstantPointerToken->BufferIndex != 0);
        isUnhandled |= (globalConstantPointerToken->BufferType != PROGRAM_SCOPE_CONSTANT_BUFFER);
        isUnhandled |= (0 == decodedProgram.programScopeTokens.allocateConstantMemorySurface.size()) || decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]->InlineDataSize < globalConstantPointerToken->ConstantPointerOffset + sizeof(uint32_t);

        if (isUnhandled) {
            outErrReason = "Unhandled SPatchConstantPointerProgramBinaryInfo";
            return ValidatorError::InvalidBinary;
        }
    }

    for (const auto &globalVariablePointerToken : decodedProgram.programScopeTokens.globalPointer) {
        bool isUnhandled = (globalVariablePointerToken->GlobalBufferIndex != 0);
        isUnhandled |= (globalVariablePointerToken->BufferIndex != 0);
        isUnhandled |= (globalVariablePointerToken->BufferType != PROGRAM_SCOPE_GLOBAL_BUFFER);
        isUnhandled |= (0 == decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size()) || decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]->InlineDataSize < globalVariablePointerToken->GlobalPointerOffset + sizeof(uint32_t);

        if (isUnhandled) {
            outErrReason = "Unhandled SPatchGlobalPointerProgramBinaryInfo";
            return ValidatorError::InvalidBinary;
        }
    }

    for (const auto &unhandledToken : decodedProgram.unhandledTokens) {
        if (false == tokenValidator.isSafeToSkipUnhandledToken(unhandledToken->Token)) {
            outErrReason = "Unhandled required program-scope Patch Token : " + std::to_string(unhandledToken->Token);
            return ValidatorError::InvalidBinary;
        } else {
            outWarnings = "Unknown program-scope Patch Token : " + std::to_string(unhandledToken->Token);
        }
    }

    UNRECOVERABLE_IF(nullptr == decodedProgram.header);
    if (decodedProgram.header->Version != CURRENT_ICBE_VERSION) {
        outErrReason = "Unhandled Version of Patchtokens: expected: " + std::to_string(CURRENT_ICBE_VERSION) + ", got: " + std::to_string(decodedProgram.header->Version);
        return ValidatorError::InvalidBinary;
    }

    if (false == isDeviceSupported(static_cast<GFXCORE_FAMILY>(decodedProgram.header->Device))) {
        outErrReason = "Unsupported device binary, device GFXCORE_FAMILY : " + std::to_string(decodedProgram.header->Device);
        return ValidatorError::InvalidBinary;
    }

    for (const auto &decodedKernel : decodedProgram.kernels) {
        if (decodedKernel.decodeStatus != PatchTokenBinary::DecoderError::Success) {
            outErrReason = "KernelFromPatchtokens wasn't successfully decoded";
            return ValidatorError::InvalidBinary;
        }

        UNRECOVERABLE_IF(nullptr == decodedKernel.header);
        if (hasInvalidChecksum(decodedKernel)) {
            outErrReason = "KernelFromPatchtokens has invalid checksum";
            return ValidatorError::InvalidBinary;
        }

        if (decodedKernel.tokens.allocateLocalSurface) {
            if (sharedLocalMemorySize < decodedKernel.tokens.allocateLocalSurface->TotalInlineLocalMemorySize) {
                outErrReason = "KernelFromPatchtokens requires too much SLM";
                return ValidatorError::NotEnoughSlm;
            }
        }

        for (const auto &unhandledToken : decodedKernel.unhandledTokens) {
            if (false == tokenValidator.isSafeToSkipUnhandledToken(unhandledToken->Token)) {
                outErrReason = "Unhandled required kernel-scope Patch Token : " + std::to_string(unhandledToken->Token);
                return ValidatorError::InvalidBinary;
            } else {
                outWarnings = "Unknown kernel-scope Patch Token : " + std::to_string(unhandledToken->Token);
            }
        }
    }

    return ValidatorError::Success;
}

} // namespace PatchTokenBinary

} // namespace NEO
