/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info_from_patchtokens.h"

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/program/program_info.h"

#include "opencl/source/program/kernel_info.h"
#include "opencl/source/program/kernel_info_from_patchtokens.h"

namespace NEO {

bool requiresLocalMemoryWindowVA(const PatchTokenBinary::ProgramFromPatchtokens &src) {
    for (const auto kernel : src.kernels) {
        if (kernel.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress) {
            return true;
        }
    }
    return false;
}

void populateSingleKernelInfo(ProgramInfo &dst, const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram, uint32_t kernelNum) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    const PatchTokenBinary::KernelFromPatchtokens &decodedKernel = decodedProgram.kernels[kernelNum];

    NEO::populateKernelInfo(*kernelInfo, decodedKernel, decodedProgram.header->GPUPointerSizeInBytes);

    if (decodedKernel.tokens.programSymbolTable) {
        dst.prepareLinkerInputStorage();
        dst.linkerInput->decodeExportedFunctionsSymbolTable(decodedKernel.tokens.programSymbolTable + 1, decodedKernel.tokens.programSymbolTable->NumEntries, kernelNum);
    }

    if (decodedKernel.tokens.programRelocationTable) {
        dst.prepareLinkerInputStorage();
        dst.linkerInput->decodeRelocationTable(decodedKernel.tokens.programRelocationTable + 1, decodedKernel.tokens.programRelocationTable->NumEntries, kernelNum);
    }

    dst.kernelInfos.push_back(kernelInfo.release());
}

void populateProgramInfo(ProgramInfo &dst, const PatchTokenBinary::ProgramFromPatchtokens &src) {
    for (uint32_t i = 0; i < src.kernels.size(); ++i) {
        populateSingleKernelInfo(dst, src, i);
    }

    if (src.programScopeTokens.allocateConstantMemorySurface.empty() == false) {
        auto surface = src.programScopeTokens.allocateConstantMemorySurface[0];
        dst.globalConstants.size = surface->InlineDataSize;
        dst.globalConstants.initData = NEO::PatchTokenBinary::getInlineData(surface);
    }

    if (src.programScopeTokens.allocateGlobalMemorySurface.empty() == false) {
        auto surface = src.programScopeTokens.allocateGlobalMemorySurface[0];
        dst.globalVariables.size = surface->InlineDataSize;
        dst.globalVariables.initData = NEO::PatchTokenBinary::getInlineData(surface);
    }

    if (false == (src.programScopeTokens.constantPointer.empty() && src.programScopeTokens.globalPointer.empty() && (src.programScopeTokens.symbolTable == nullptr))) {
        UNRECOVERABLE_IF((src.header->GPUPointerSizeInBytes != 4) && (src.header->GPUPointerSizeInBytes != 8));
        dst.prepareLinkerInputStorage();
        dst.linkerInput->setPointerSize((src.header->GPUPointerSizeInBytes == 4) ? LinkerInput::Traits::PointerSize::Ptr32bit : LinkerInput::Traits::PointerSize::Ptr64bit);
    }

    for (const auto &globalConstantPointerToken : src.programScopeTokens.constantPointer) {
        NEO::LinkerInput::RelocationInfo relocInfo = {};
        relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
        relocInfo.offset = readMisalignedUint64(&globalConstantPointerToken->ConstantPointerOffset);
        relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;
        if (globalConstantPointerToken->BufferType != iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER) {
            UNRECOVERABLE_IF(globalConstantPointerToken->BufferType != iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER);
            relocInfo.symbolSegment = NEO::SegmentType::GlobalVariables;
        }
        relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
        dst.linkerInput->addDataRelocationInfo(relocInfo);
    }

    for (const auto &globalVariablePointerToken : src.programScopeTokens.globalPointer) {
        NEO::LinkerInput::RelocationInfo relocInfo = {};
        relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
        relocInfo.offset = readMisalignedUint64(&globalVariablePointerToken->GlobalPointerOffset);
        relocInfo.symbolSegment = NEO::SegmentType::GlobalVariables;
        if (globalVariablePointerToken->BufferType != iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER) {
            UNRECOVERABLE_IF(globalVariablePointerToken->BufferType != iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER);
            relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;
        }
        relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
        dst.linkerInput->addDataRelocationInfo(relocInfo);
    }

    if (src.programScopeTokens.symbolTable != nullptr) {
        const auto patch = src.programScopeTokens.symbolTable;
        dst.linkerInput->decodeGlobalVariablesSymbolTable(patch + 1, patch->NumEntries);
    }
}

} // namespace NEO
