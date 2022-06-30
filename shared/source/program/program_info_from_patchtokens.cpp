/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info_from_patchtokens.h"

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/kernel_info_from_patchtokens.h"
#include "shared/source/program/program_info.h"

#include <RelocationInfo.h>

namespace NEO {

bool requiresLocalMemoryWindowVA(const PatchTokenBinary::ProgramFromPatchtokens &src) {
    for (const auto &kernel : src.kernels) {
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

    if (decodedKernel.tokens.hostAccessTable) {
        parseHostAccessTable(dst, decodedKernel.tokens.hostAccessTable);
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

    constexpr ConstStringRef globalConstantsSymbolName = "globalConstants";
    constexpr ConstStringRef globalVariablesSymbolName = "globalVariables";

    if (false == (src.programScopeTokens.constantPointer.empty() && src.programScopeTokens.globalPointer.empty() && (src.programScopeTokens.symbolTable == nullptr))) {
        UNRECOVERABLE_IF((src.header->GPUPointerSizeInBytes != 4) && (src.header->GPUPointerSizeInBytes != 8));
        dst.prepareLinkerInputStorage();
        dst.linkerInput->setPointerSize((src.header->GPUPointerSizeInBytes == 4) ? LinkerInput::Traits::PointerSize::Ptr32bit : LinkerInput::Traits::PointerSize::Ptr64bit);

        dst.linkerInput->addSymbol(globalConstantsSymbolName.data(), {0U, 8U, SegmentType::GlobalConstants});
        dst.linkerInput->addSymbol(globalVariablesSymbolName.data(), {0U, 8U, SegmentType::GlobalVariables});
    }

    for (const auto &globalConstantPointerToken : src.programScopeTokens.constantPointer) {
        NEO::LinkerInput::RelocationInfo relocInfo = {};
        relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
        relocInfo.offset = readMisalignedUint64(&globalConstantPointerToken->ConstantPointerOffset);
        relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
        relocInfo.symbolName = std::string(globalConstantsSymbolName);
        if (iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER != globalConstantPointerToken->BufferType) {
            UNRECOVERABLE_IF(iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER != globalConstantPointerToken->BufferType);
            relocInfo.symbolName = std::string(globalVariablesSymbolName);
        }

        dst.linkerInput->addDataRelocationInfo(relocInfo);
    }

    for (const auto &globalVariablePointerToken : src.programScopeTokens.globalPointer) {
        NEO::LinkerInput::RelocationInfo relocInfo = {};
        relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
        relocInfo.offset = readMisalignedUint64(&globalVariablePointerToken->GlobalPointerOffset);
        relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
        relocInfo.symbolName = std::string(globalVariablesSymbolName);
        if (iOpenCL::PROGRAM_SCOPE_GLOBAL_BUFFER != globalVariablePointerToken->BufferType) {
            UNRECOVERABLE_IF(iOpenCL::PROGRAM_SCOPE_CONSTANT_BUFFER != globalVariablePointerToken->BufferType);
            relocInfo.symbolName = std::string(globalConstantsSymbolName);
        }

        dst.linkerInput->addDataRelocationInfo(relocInfo);
    }

    if (src.programScopeTokens.symbolTable != nullptr) {
        const auto patch = src.programScopeTokens.symbolTable;
        dst.linkerInput->decodeGlobalVariablesSymbolTable(patch + 1, patch->NumEntries);
    }
}

void parseHostAccessTable(ProgramInfo &dst, const void *hostAccessTable) {
    hostAccessTable = ptrOffset(hostAccessTable, sizeof(iOpenCL::SPatchItemHeader));
    const uint32_t numEntries = *reinterpret_cast<const uint32_t *>(hostAccessTable);
    hostAccessTable = ptrOffset(hostAccessTable, sizeof(uint32_t));
    auto &deviceToHostNameMap = dst.globalsDeviceToHostNameMap;

    struct HostAccessTableEntry {
        const char deviceName[vISA::MAX_SYMBOL_NAME_LENGTH];
        const char hostName[vISA::MAX_SYMBOL_NAME_LENGTH];
    };
    ArrayRef<const HostAccessTableEntry> hostAccessTableEntries = {reinterpret_cast<const HostAccessTableEntry *>(hostAccessTable),
                                                                   numEntries};
    for (size_t i = 0; i < hostAccessTableEntries.size(); i++) {
        auto &entry = hostAccessTableEntries[i];
        deviceToHostNameMap[entry.deviceName] = entry.hostName;
    }
}
} // namespace NEO
