/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/patchtokens_validator.h"

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/hw_info.h"

#include "opencl/source/program/kernel_arg_info.h"

#include "igfxfmid.h"

#include <string>

namespace NEO {

namespace PatchTokenBinary {

bool allowUnhandledTokens = true;

DecodeError validate(const ProgramFromPatchtokens &decodedProgram,
                     std::string &outErrReason, std::string &outWarnings) {
    if (decodedProgram.decodeStatus != DecodeError::Success) {
        outErrReason = "ProgramFromPatchtokens wasn't successfully decoded";
        return DecodeError::InvalidBinary;
    }

    if (decodedProgram.programScopeTokens.allocateConstantMemorySurface.size() > 1) {
        outErrReason = "Unhandled number of global constants surfaces > 1";
        return DecodeError::UnhandledBinary;
    }

    if (decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size() > 1) {
        outErrReason = "Unhandled number of global variables surfaces > 1";
        return DecodeError::UnhandledBinary;
    }

    for (const auto &globalConstantPointerToken : decodedProgram.programScopeTokens.constantPointer) {
        bool isUnhandled = (globalConstantPointerToken->ConstantBufferIndex != 0);
        isUnhandled |= (globalConstantPointerToken->BufferIndex != 0);
        isUnhandled |= (globalConstantPointerToken->BufferType != PROGRAM_SCOPE_CONSTANT_BUFFER) && (globalConstantPointerToken->BufferType != PROGRAM_SCOPE_GLOBAL_BUFFER);
        isUnhandled |= (globalConstantPointerToken->BufferType == PROGRAM_SCOPE_GLOBAL_BUFFER) && (decodedProgram.programScopeTokens.allocateGlobalMemorySurface.empty());
        isUnhandled |= (decodedProgram.programScopeTokens.allocateConstantMemorySurface.empty()) || decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]->InlineDataSize < globalConstantPointerToken->ConstantPointerOffset + sizeof(uint32_t);

        if (isUnhandled) {
            outErrReason = "Unhandled SPatchConstantPointerProgramBinaryInfo";
            return DecodeError::UnhandledBinary;
        }
    }

    for (const auto &globalVariablePointerToken : decodedProgram.programScopeTokens.globalPointer) {
        bool isUnhandled = (globalVariablePointerToken->GlobalBufferIndex != 0);
        isUnhandled |= (globalVariablePointerToken->BufferIndex != 0);
        isUnhandled |= (globalVariablePointerToken->BufferType != PROGRAM_SCOPE_GLOBAL_BUFFER) && (globalVariablePointerToken->BufferType != PROGRAM_SCOPE_CONSTANT_BUFFER);
        isUnhandled |= (globalVariablePointerToken->BufferType == PROGRAM_SCOPE_CONSTANT_BUFFER) && (decodedProgram.programScopeTokens.allocateConstantMemorySurface.empty());
        isUnhandled |= (decodedProgram.programScopeTokens.allocateGlobalMemorySurface.empty()) || decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]->InlineDataSize < globalVariablePointerToken->GlobalPointerOffset + sizeof(uint32_t);

        if (isUnhandled) {
            outErrReason = "Unhandled SPatchGlobalPointerProgramBinaryInfo";
            return DecodeError::UnhandledBinary;
        }
    }

    for (const auto &unhandledToken : decodedProgram.unhandledTokens) {
        if (allowUnhandledTokens) {
            outWarnings = "Unknown program-scope Patch Token : " + std::to_string(unhandledToken->Token);
        } else {
            outErrReason = "Unhandled required program-scope Patch Token : " + std::to_string(unhandledToken->Token);
            return DecodeError::UnhandledBinary;
        }
    }

    UNRECOVERABLE_IF(nullptr == decodedProgram.header);
    if (decodedProgram.header->Version != CURRENT_ICBE_VERSION) {
        outErrReason = "Unhandled Version of Patchtokens: expected: " + std::to_string(CURRENT_ICBE_VERSION) + ", got: " + std::to_string(decodedProgram.header->Version);
        return DecodeError::UnhandledBinary;
    }

    if ((decodedProgram.header->GPUPointerSizeInBytes != 4U) && (decodedProgram.header->GPUPointerSizeInBytes != 8U)) {
        outErrReason = "Invalid pointer size";
        return DecodeError::UnhandledBinary;
    }

    for (const auto &decodedKernel : decodedProgram.kernels) {
        if (decodedKernel.decodeStatus != DecodeError::Success) {
            outErrReason = "KernelFromPatchtokens wasn't successfully decoded";
            return DecodeError::UnhandledBinary;
        }

        UNRECOVERABLE_IF(nullptr == decodedKernel.header);
        if (hasInvalidChecksum(decodedKernel)) {
            outErrReason = "KernelFromPatchtokens has invalid checksum";
            return DecodeError::UnhandledBinary;
        }

        if (nullptr == decodedKernel.tokens.executionEnvironment) {
            outErrReason = "Missing execution environment";
            return DecodeError::UnhandledBinary;
        } else {
            switch (decodedKernel.tokens.executionEnvironment->LargestCompiledSIMDSize) {
            case 1:
                break;
            case 8:
                break;
            case 16:
                break;
            case 32:
                break;
            default:
                outErrReason = "Invalid LargestCompiledSIMDSize";
                return DecodeError::UnhandledBinary;
            }
        }

        for (auto &kernelArg : decodedKernel.tokens.kernelArgs) {
            if (kernelArg.argInfo == nullptr) {
                continue;
            }
            auto argInfoInlineData = getInlineData(kernelArg.argInfo);
            auto accessQualifier = KernelArgMetadata::parseAccessQualifier(parseLimitedString(argInfoInlineData.accessQualifier.begin(), argInfoInlineData.accessQualifier.size()));
            if (KernelArgMetadata::AccessUnknown == accessQualifier) {
                outErrReason = "Unhandled access qualifier";
                return DecodeError::UnhandledBinary;
            }
            auto addressQualifier = KernelArgMetadata::parseAddressSpace(parseLimitedString(argInfoInlineData.addressQualifier.begin(), argInfoInlineData.addressQualifier.size()));
            if (KernelArgMetadata::AddrUnknown == addressQualifier) {
                outErrReason = "Unhandled address qualifier";
                return DecodeError::UnhandledBinary;
            }
        }

        for (const auto &unhandledToken : decodedKernel.unhandledTokens) {
            if (allowUnhandledTokens) {
                outWarnings = "Unknown kernel-scope Patch Token : " + std::to_string(unhandledToken->Token);
            } else {
                outErrReason = "Unhandled required kernel-scope Patch Token : " + std::to_string(unhandledToken->Token);
                return DecodeError::UnhandledBinary;
            }
        }
    }

    return DecodeError::Success;
}

} // namespace PatchTokenBinary

} // namespace NEO
