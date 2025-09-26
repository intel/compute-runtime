/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/linker.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/program_info.h"
#include "shared/source/release_helper/release_helper.h"

#include "RelocationInfo.h"

#include <sstream>
#include <unordered_map>

namespace NEO {

LinkerInput::LinkerInput() = default;
LinkerInput::~LinkerInput() = default;

SegmentType LinkerInput::getSegmentForSection(ConstStringRef name) {
    if (name == NEO::Zebin::Elf::SectionNames::dataConst || name == NEO::Zebin::Elf::SectionNames::dataGlobalConst) {
        return NEO::SegmentType::globalConstants;
    } else if (name == NEO::Zebin::Elf::SectionNames::dataGlobal) {
        return NEO::SegmentType::globalVariables;
    } else if (name == NEO::Zebin::Elf::SectionNames::dataConstString) {
        return NEO::SegmentType::globalStrings;
    } else if (name.startsWith(NEO::Elf::SpecialSectionNames::text.data())) {
        return NEO::SegmentType::instructions;
    } else if (name == NEO::Zebin::Elf::SectionNames::dataConstZeroInit) {
        return NEO::SegmentType::globalConstantsZeroInit;
    } else if (name == NEO::Zebin::Elf::SectionNames::dataGlobalZeroInit) {
        return NEO::SegmentType::globalVariablesZeroInit;
    }
    return NEO::SegmentType::unknown;
}

bool LinkerInput::decodeGlobalVariablesSymbolTable(const void *data, uint32_t numEntries) {
    auto symbolEntryIt = reinterpret_cast<const vISA::GenSymEntry *>(data);
    auto symbolEntryEnd = symbolEntryIt + numEntries;
    symbols.reserve(symbols.size() + numEntries);
    for (; symbolEntryIt != symbolEntryEnd; ++symbolEntryIt) {
        DEBUG_BREAK_IF(symbols.count(symbolEntryIt->s_name) > 0);
        SymbolInfo &symbolInfo = symbols[symbolEntryIt->s_name];
        symbolInfo.offset = symbolEntryIt->s_offset;
        symbolInfo.size = symbolEntryIt->s_size;
        symbolInfo.global = true;
        switch (symbolEntryIt->s_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.segment = SegmentType::globalVariables;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.segment = SegmentType::globalConstants;
            traits.exportsGlobalConstants = true;
            break;
        }
    }
    return true;
}

bool LinkerInput::decodeExportedFunctionsSymbolTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId) {
    auto symbolEntryIt = reinterpret_cast<const vISA::GenSymEntry *>(data);
    auto symbolEntryEnd = symbolEntryIt + numEntries;
    symbols.reserve(symbols.size() + numEntries);
    for (; symbolEntryIt != symbolEntryEnd; ++symbolEntryIt) {
        SymbolInfo &symbolInfo = symbols[symbolEntryIt->s_name];
        symbolInfo.offset = symbolEntryIt->s_offset;
        symbolInfo.size = symbolEntryIt->s_size;
        symbolInfo.global = true;
        switch (symbolEntryIt->s_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::S_UNDEF:
            symbols.erase(symbolEntryIt->s_name);
            break;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.segment = SegmentType::globalVariables;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.segment = SegmentType::globalConstants;
            traits.exportsGlobalConstants = true;
            break;
        case vISA::S_FUNC:
            symbolInfo.segment = SegmentType::instructions;
            symbolInfo.instructionSegmentId = instructionsSegmentId;
            traits.exportsFunctions = true;
            UNRECOVERABLE_IF((this->exportedFunctionsSegmentId != -1) && (this->exportedFunctionsSegmentId != static_cast<int32_t>(instructionsSegmentId)));
            this->exportedFunctionsSegmentId = static_cast<int32_t>(instructionsSegmentId);
            break;
        }
    }
    return true;
}

bool LinkerInput::decodeRelocationTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId) {
    this->traits.requiresPatchingOfInstructionSegments = true;
    auto relocEntryIt = reinterpret_cast<const vISA::GenRelocEntry *>(data);
    auto relocEntryEnd = relocEntryIt + numEntries;
    if (instructionsSegmentId >= textRelocations.size()) {
        static_assert(std::is_nothrow_move_constructible<decltype(textRelocations[0])>::value, "");
        textRelocations.resize(instructionsSegmentId + 1);
    }

    auto &outRelocInfo = textRelocations[instructionsSegmentId];
    outRelocInfo.reserve(numEntries);
    for (; relocEntryIt != relocEntryEnd; ++relocEntryIt) {
        RelocationInfo relocInfo{};
        relocInfo.offset = relocEntryIt->r_offset;
        relocInfo.symbolName = relocEntryIt->r_symbol;
        relocInfo.relocationSegment = SegmentType::instructions;
        switch (relocEntryIt->r_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::R_SYM_ADDR:
            relocInfo.type = RelocationInfo::Type::address;
            break;
        case vISA::R_SYM_ADDR_32:
            relocInfo.type = RelocationInfo::Type::addressLow;
            break;
        case vISA::R_SYM_ADDR_32_HI:
            relocInfo.type = RelocationInfo::Type::addressHigh;
            break;
        case vISA::R_PER_THREAD_PAYLOAD_OFFSET_32:
            relocInfo.type = RelocationInfo::Type::perThreadPayloadOffset;
            break;
        }
        outRelocInfo.push_back(std::move(relocInfo));
    }
    return true;
}

void LinkerInput::addDataRelocationInfo(const RelocationInfo &relocationInfo) {
    DEBUG_BREAK_IF((relocationInfo.relocationSegment != SegmentType::globalConstants) && (relocationInfo.relocationSegment != SegmentType::globalVariables));
    this->traits.requiresPatchingOfGlobalVariablesBuffer |= (relocationInfo.relocationSegment == SegmentType::globalVariables);
    this->traits.requiresPatchingOfGlobalConstantsBuffer |= (relocationInfo.relocationSegment == SegmentType::globalConstants);
    this->dataRelocations.push_back(relocationInfo);
}

void LinkerInput::addElfTextSegmentRelocation(RelocationInfo relocationInfo, uint32_t instructionsSegmentId) {
    this->traits.requiresPatchingOfInstructionSegments = true;

    if (instructionsSegmentId >= textRelocations.size()) {
        static_assert(std::is_nothrow_move_constructible<decltype(textRelocations[0])>::value, "");
        textRelocations.resize(instructionsSegmentId + 1);
    }

    auto &outRelocInfo = textRelocations[instructionsSegmentId];

    relocationInfo.relocationSegment = SegmentType::instructions;

    outRelocInfo.push_back(std::move(relocationInfo));
}

template bool LinkerInput::addRelocation(Elf::Elf<Elf::EI_CLASS_32> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, const typename Elf::Elf<Elf::EI_CLASS_32>::RelocationInfo &reloc);
template bool LinkerInput::addRelocation(Elf::Elf<Elf::EI_CLASS_64> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, const typename Elf::Elf<Elf::EI_CLASS_64>::RelocationInfo &reloc);
template <Elf::ElfIdentifierClass numBits>
bool LinkerInput::addRelocation(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, const typename Elf::Elf<numBits>::RelocationInfo &reloc) {
    auto sectionName = elf.getSectionName(reloc.targetSectionIndex);

    NEO::LinkerInput::RelocationInfo relocationInfo;
    relocationInfo.offset = reloc.offset;
    relocationInfo.addend = reloc.addend;
    relocationInfo.symbolName = reloc.symbolName;
    relocationInfo.type = static_cast<LinkerInput::RelocationInfo::Type>(reloc.relocType);
    relocationInfo.relocationSegment = getSegmentForSection(sectionName);
    relocationInfo.relocationSegmentName = sectionName;

    if (SegmentType::instructions == relocationInfo.relocationSegment) {
        auto kernelName = Zebin::getKernelNameFromSectionName(ConstStringRef(sectionName)).str();
        if (auto instructionSegmentId = getInstructionSegmentId(nameToSegmentId, kernelName)) {
            addElfTextSegmentRelocation(relocationInfo, *instructionSegmentId);
            parseRelocationForExtFuncUsage(relocationInfo, kernelName);
            return true;
        } else {
            valid = false;
            return false;
        }
    } else if (isDataSegment(relocationInfo.relocationSegment)) {
        addDataRelocationInfo(relocationInfo);
        return true;
    }
    return false;
}

std::optional<uint32_t> LinkerInput::getInstructionSegmentId(const SectionNameToSegmentIdMap &kernelNameToSegId, const std::string &kernelName) {
    auto segmentIdIter = kernelNameToSegId.find(kernelName);
    if (segmentIdIter == kernelNameToSegId.end()) {
        valid = false;
        return std::nullopt;
    }
    return segmentIdIter->second;
}

template bool LinkerInput::addSymbol(Elf::Elf<Elf::EI_CLASS_32> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, size_t symId);
template bool LinkerInput::addSymbol(Elf::Elf<Elf::EI_CLASS_64> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, size_t symId);
template <Elf::ElfIdentifierClass numBits>
bool LinkerInput::addSymbol(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, size_t symId) {
    auto &elfSymbols = elf.getSymbols();
    if (symId >= elfSymbols.size()) {
        valid = false;
        return false;
    }

    auto &elfSymbol = elfSymbols[symId];
    auto symbolName = elf.getSymbolName(elfSymbol.name);
    auto symbolSectionName = elf.getSectionName(elfSymbol.shndx);
    auto segment = getSegmentForSection(symbolSectionName);
    if (segment == SegmentType::unknown) {
        externalSymbols.push_back(symbolName);
        return false;
    }

    SymbolInfo symbolInfo{};
    symbolInfo.segment = segment;
    symbolInfo.global = elfSymbol.getBinding() == Elf::STB_GLOBAL;
    symbolInfo.offset = static_cast<uint64_t>(elfSymbol.value);
    symbolInfo.size = static_cast<uint64_t>(elfSymbol.size);

    auto symbolType = elfSymbol.getType();
    if (symbolType == Elf::STT_OBJECT) {
        if (symbolInfo.global) {
            traits.exportsGlobalVariables |= isVarDataSegment(segment);
            traits.exportsGlobalConstants |= isConstDataSegment(segment);
        }
    } else if (symbolType == Elf::STT_FUNC) {
        auto kernelName = Zebin::getKernelNameFromSectionName(ConstStringRef(symbolSectionName)).str();
        if (auto segId = getInstructionSegmentId(nameToSegmentId, kernelName)) {
            symbolInfo.instructionSegmentId = *segId;
        } else {
            valid = false;
            return false;
        }

        if (symbolInfo.global) {
            if (exportedFunctionsSegmentId != -1 && exportedFunctionsSegmentId != static_cast<int32_t>(symbolInfo.instructionSegmentId)) {
                valid = false;
                return false;
            }

            traits.exportsFunctions = true;
            exportedFunctionsSegmentId = static_cast<int32_t>(symbolInfo.instructionSegmentId);
            extFuncSymbols.push_back({symbolName, symbolInfo});
        }
    } else {
        return false;
    }

    symbols.insert({symbolName, symbolInfo});
    return true;
}

template void LinkerInput::decodeElfSymbolTableAndRelocations<Elf::EI_CLASS_32>(Elf::Elf<Elf::EI_CLASS_32> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);
template void LinkerInput::decodeElfSymbolTableAndRelocations<Elf::EI_CLASS_64>(Elf::Elf<Elf::EI_CLASS_64> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);
template <Elf::ElfIdentifierClass numBits>
void LinkerInput::decodeElfSymbolTableAndRelocations(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId) {
    symbols.reserve(elf.getSymbols().size());

    auto &elfSymbols = elf.getSymbols();
    for (size_t i = 0; i < elfSymbols.size(); i++) {
        if (elfSymbols[i].getBinding() == Elf::STB_GLOBAL) {
            addSymbol(elf, nameToSegmentId, i);
        }
    }

    for (auto &reloc : elf.getRelocations()) {
        if (addRelocation(elf, nameToSegmentId, reloc)) {          // relocation was added
            if (symbols.find(reloc.symbolName) == symbols.end()) { // symbol used in relocation is not present
                addSymbol(elf, nameToSegmentId, reloc.symbolTableIndex);
            }
        }
    }
}

void LinkerInput::parseRelocationForExtFuncUsage(const RelocationInfo &relocInfo, const std::string &kernelName) {

    bool isExternalSymbol = false;
    auto shouldIgnoreRelocation = [&](const RelocationInfo &relocInfo) {
        if (relocInfo.symbolName.empty()) {
            return true;
        }

        // ignore relocations for non-instruction symbols
        if (std::find_if(symbols.begin(), symbols.end(), [relocInfo](auto &pair) {
                auto &symbol = pair.second;
                return relocInfo.symbolName == pair.first && symbol.segment != SegmentType::instructions;
            }) != symbols.end()) {
            return true;
        }

        if (std::ranges::find(externalSymbols, relocInfo.symbolName) != externalSymbols.end()) {
            isExternalSymbol = true;
        }
        return false;
    };

    if (shouldIgnoreRelocation(relocInfo)) {
        return;
    }
    if (kernelName == Zebin::Elf::SectionNames::externalFunctions.str()) {
        auto callerIt = std::find_if(extFuncSymbols.begin(), extFuncSymbols.end(), [relocInfo](auto &pair) {
            auto &symbol = pair.second;
            return relocInfo.offset >= symbol.offset && relocInfo.offset < symbol.offset + symbol.size;
        });
        if (callerIt != extFuncSymbols.end()) {
            extFunDependencies.push_back({relocInfo.symbolName, callerIt->first, isExternalSymbol});
        }
    } else {
        kernelDependencies.push_back({relocInfo.symbolName, kernelName, isExternalSymbol});
    }
}

LinkingStatus Linker::link(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo, const SegmentInfo &exportedFunctionsSegInfo,
                           const SegmentInfo &globalStringsSegInfo, SharedPoolAllocation *globalVariablesSeg, SharedPoolAllocation *globalConstantsSeg,
                           const PatchableSegments &instructionsSegments, UnresolvedExternals &outUnresolvedExternals, Device *pDevice, const void *constantsInitData,
                           size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize, const KernelDescriptorsT &kernelDescriptors,
                           ExternalFunctionsT &externalFunctions) {
    bool success = data.isValid();
    auto initialUnresolvedExternalsCount = outUnresolvedExternals.size();
    success = success && relocateSymbols(globalVariablesSegInfo, globalConstantsSegInfo, exportedFunctionsSegInfo, globalStringsSegInfo, instructionsSegments, constantsInitDataSize, variablesInitDataSize);
    if (!success) {
        return LinkingStatus::error;
    }
    patchInstructionsSegments(instructionsSegments, outUnresolvedExternals, kernelDescriptors);
    patchDataSegments(globalVariablesSegInfo, globalConstantsSegInfo, globalVariablesSeg, globalConstantsSeg,
                      outUnresolvedExternals, pDevice, constantsInitData, constantsInitDataSize, variablesInitData, variablesInitDataSize);
    removeLocalSymbolsFromRelocatedSymbols();
    resolveImplicitArgs(kernelDescriptors, pDevice);
    resolveBuiltins(pDevice, outUnresolvedExternals, instructionsSegments, kernelDescriptors);

    if (initialUnresolvedExternalsCount < outUnresolvedExternals.size()) {
        return LinkingStatus::linkedPartially;
    }
    success = resolveExternalFunctions(kernelDescriptors, externalFunctions);
    if (!success) {
        return LinkingStatus::error;
    }
    return LinkingStatus::linkedFully;
}

bool Linker::relocateSymbols(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions, const SegmentInfo &globalStrings,
                             const PatchableSegments &instructionsSegments, size_t globalConstantsInitDataSize, size_t globalVariablesInitDataSize) {
    relocatedSymbols.reserve(data.getSymbols().size());
    for (const auto &[symbolName, symbolInfo] : data.getSymbols()) {
        if (symbolInfo.segment == SegmentType::instructions && false == symbolInfo.global) {
            if (symbolInfo.instructionSegmentId >= instructionsSegments.size()) {
                return false;
            }
            auto &segment = instructionsSegments[symbolInfo.instructionSegmentId];
            if (symbolInfo.offset + symbolInfo.size > segment.segmentSize) {
                return false;
            }
            relocatedSymbols[symbolName] = {symbolInfo, segment.gpuAddress + symbolInfo.offset};
        } else {
            const SegmentInfo *seg = nullptr;
            uint64_t offset = symbolInfo.offset;
            switch (symbolInfo.segment) {
            default:
                DEBUG_BREAK_IF(true);
                return false;
            case SegmentType::globalVariables:
                seg = &globalVariables;
                break;
            case SegmentType::globalVariablesZeroInit:
                seg = &globalVariables;
                offset += globalVariablesInitDataSize;
                break;
            case SegmentType::globalConstants:
                seg = &globalConstants;
                break;
            case SegmentType::globalConstantsZeroInit:
                seg = &globalConstants;
                offset += globalConstantsInitDataSize;
                break;
            case SegmentType::globalStrings:
                seg = &globalStrings;
                break;
            case SegmentType::instructions:
                seg = &exportedFunctions;
                break;
            }

            if (offset + symbolInfo.size > seg->segmentSize) {
                DEBUG_BREAK_IF(true);
                return false;
            }
            relocatedSymbols[symbolName] = {symbolInfo, seg->gpuAddress + offset};
        }
    }
    return true;
}

uint32_t addressSizeInBytes(LinkerInput::RelocationInfo::Type relocationtype) {
    return (relocationtype == LinkerInput::RelocationInfo::Type::address) ? sizeof(uintptr_t) : sizeof(uint32_t);
}

void Linker::patchAddress(void *relocAddress, const uint64_t value, const Linker::RelocationInfo &relocation) {
    switch (relocation.type) {
    default:
        UNRECOVERABLE_IF(RelocationInfo::Type::address != relocation.type);
        memcpy_s(relocAddress, sizeof(uint64_t), &value, sizeof(uint64_t));
        break;
    case RelocationInfo::Type::addressLow: {
        uint32_t valueToPatch = static_cast<uint32_t>(value & 0xffffffff);
        memcpy_s(relocAddress, sizeof(uint32_t), &valueToPatch, sizeof(uint32_t));
    } break;
    case RelocationInfo::Type::addressHigh: {
        uint32_t valueToPatch = static_cast<uint32_t>((value >> 32) & 0xffffffff);
        memcpy_s(relocAddress, sizeof(uint32_t), &valueToPatch, sizeof(uint32_t));
    } break;
    case RelocationInfo::Type::address16: {
        uint16_t valueToPatch = static_cast<uint16_t>(value & 0xffff);
        memcpy_s(relocAddress, sizeof(uint16_t), &valueToPatch, sizeof(uint16_t));
    } break;
    }
}

void Linker::removeLocalSymbolsFromRelocatedSymbols() {
    auto it = relocatedSymbols.begin();
    while (it != relocatedSymbols.end()) {
        if (false == it->second.symbol.global) {
            it = relocatedSymbols.erase(it);
        } else {
            it++;
        }
    }
}

void Linker::patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals, const KernelDescriptorsT &kernelDescriptors) {
    if (false == data.getTraits().requiresPatchingOfInstructionSegments) {
        return;
    }

    auto &relocationsPerSegment = data.getRelocationsInInstructionSegments();
    UNRECOVERABLE_IF(data.getRelocationsInInstructionSegments().size() > instructionsSegments.size());
    for (size_t segId = 0U; segId < relocationsPerSegment.size(); segId++) {
        auto &segment = instructionsSegments[segId];
        for (const auto &relocation : relocationsPerSegment[segId]) {
            UNRECOVERABLE_IF(nullptr == segment.hostPointer);
            bool invalidRelocation = relocation.offset + addressSizeInBytes(relocation.type) > segment.segmentSize;
            if (invalidRelocation) {
                outUnresolvedExternals.push_back(UnresolvedExternal{relocation, static_cast<uint32_t>(segId), invalidRelocation});
                DEBUG_BREAK_IF(true);
                continue;
            }

            auto relocAddress = ptrOffset(segment.hostPointer, static_cast<uintptr_t>(relocation.offset));
            if (relocation.type == LinkerInput::RelocationInfo::Type::perThreadPayloadOffset) {
                *reinterpret_cast<uint32_t *>(relocAddress) = kernelDescriptors.at(segId)->getPerThreadDataOffset();
            } else if (relocation.symbolName == implicitArgsRelocationSymbolName) {
                pImplicitArgsRelocationAddresses[static_cast<uint32_t>(segId)].push_back(std::pair<void *, RelocationInfo::Type>(relocAddress, relocation.type));
            } else if (relocation.symbolName.empty()) {
                uint64_t patchValue = 0;
                patchAddress(relocAddress, patchValue, relocation);
            } else {
                auto symbolIt = relocatedSymbols.find(relocation.symbolName);
                if (symbolIt != relocatedSymbols.end()) {
                    uint64_t patchValue = symbolIt->second.gpuAddress + relocation.addend;
                    patchAddress(relocAddress, patchValue, relocation);
                } else {
                    outUnresolvedExternals.push_back(UnresolvedExternal{relocation, static_cast<uint32_t>(segId), invalidRelocation});
                }
            }
        }
    }
}

void Linker::patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                               SharedPoolAllocation *globalVariablesSeg, SharedPoolAllocation *globalConstantsSeg,
                               std::vector<UnresolvedExternal> &outUnresolvedExternals, Device *pDevice,
                               const void *constantsInitData, size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize) {
    std::vector<uint8_t> constantsData(globalConstantsSegInfo.segmentSize, 0u);
    memcpy_s(constantsData.data(), constantsData.size(), constantsInitData, constantsInitDataSize);
    std::vector<uint8_t> variablesData(globalVariablesSegInfo.segmentSize, 0u);
    memcpy_s(variablesData.data(), variablesData.size(), variablesInitData, variablesInitDataSize);
    bool isAnyRelocationPerformed = false;

    for (const auto &relocation : data.getDataRelocations()) {
        auto symbolIt = relocatedSymbols.find(relocation.symbolName);
        if (symbolIt == relocatedSymbols.end()) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }
        uint64_t srcGpuAddressAs64Bit = symbolIt->second.gpuAddress;

        ArrayRef<uint8_t> dst{};
        const void *initData = nullptr;
        if (SegmentType::globalConstants == relocation.relocationSegment) {
            dst = {constantsData.data(), constantsInitDataSize};
            initData = constantsInitData;
        } else if (SegmentType::globalConstantsZeroInit == relocation.relocationSegment) {
            dst = {constantsData.data() + constantsInitDataSize, constantsData.size() - constantsInitDataSize};
        } else if (SegmentType::globalVariables == relocation.relocationSegment) {
            dst = {variablesData.data(), variablesInitDataSize};
            initData = variablesInitData;
        } else if (SegmentType::globalVariablesZeroInit == relocation.relocationSegment) {
            dst = {variablesData.data() + variablesInitDataSize, variablesData.size() - variablesInitDataSize};
        } else {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }

        auto relocType = (LinkerInput::Traits::PointerSize::Ptr32bit == data.getTraits().pointerSize) ? RelocationInfo::Type::addressLow : relocation.type;
        bool invalidOffset = relocation.offset + addressSizeInBytes(relocType) > dst.size();
        DEBUG_BREAK_IF(invalidOffset);
        if (invalidOffset) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }

        uint64_t incrementValue = srcGpuAddressAs64Bit + relocation.addend;
        isAnyRelocationPerformed = true;
        switch (relocType) {
        default:
            UNRECOVERABLE_IF(RelocationInfo::Type::address != relocType);
            patchIncrement<uint64_t>(dst.begin(), static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
        case RelocationInfo::Type::addressLow:
            incrementValue = incrementValue & 0xffffffff;
            patchIncrement<uint32_t>(dst.begin(), static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
        case RelocationInfo::Type::addressHigh:
            incrementValue = (incrementValue >> 32) & 0xffffffff;
            patchIncrement<uint32_t>(dst.begin(), static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
        }
    }

    if (isAnyRelocationPerformed) {
        auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
        auto &productHelper = pDevice->getProductHelper();
        if (globalConstantsSeg) {
            bool useBlitter = productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *globalConstantsSeg->getGraphicsAllocation());
            MemoryTransferHelper::transferMemoryToAllocation(useBlitter, *pDevice, globalConstantsSeg->getGraphicsAllocation(), globalConstantsSeg->getOffset(), constantsData.data(), constantsData.size());
        }
        if (globalVariablesSeg) {
            bool useBlitter = productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *globalVariablesSeg->getGraphicsAllocation());
            MemoryTransferHelper::transferMemoryToAllocation(useBlitter, *pDevice, globalVariablesSeg->getGraphicsAllocation(), globalVariablesSeg->getOffset(), variablesData.data(), variablesData.size());
        }
    }
}

std::string constructLinkerErrorMessage(const Linker::UnresolvedExternals &unresolvedExternals, const std::vector<std::string> &instructionsSegmentsNames) {
    std::stringstream errorStream;
    if (unresolvedExternals.size() == 0) {
        errorStream << "Internal linker error";
    } else {
        for (const auto &unresExtern : unresolvedExternals) {
            if (unresExtern.internalError) {
                errorStream << "error : internal linker error while handling symbol ";
            } else {
                errorStream << "error : unresolved external symbol ";
            }

            if (unresExtern.unresolvedRelocation.relocationSegment == NEO::SegmentType::instructions) {
                errorStream << unresExtern.unresolvedRelocation.symbolName << " at offset " << unresExtern.unresolvedRelocation.offset
                            << " in instructions segment #" << unresExtern.instructionsSegmentId;
                if (instructionsSegmentsNames.size() > unresExtern.instructionsSegmentId) {
                    errorStream << " (aka " << instructionsSegmentsNames[unresExtern.instructionsSegmentId] << ")";
                }
            } else {
                errorStream << " symbol #" << unresExtern.unresolvedRelocation.symbolName << " at offset " << unresExtern.unresolvedRelocation.offset
                            << " in data segment #" << asString(unresExtern.unresolvedRelocation.relocationSegment);
            }
            errorStream << "\n";
        }
    }
    return errorStream.str();
}

std::string constructRelocationsDebugMessage(const Linker::RelocatedSymbolsMap &relocatedSymbols) {
    if (relocatedSymbols.empty()) {
        return "";
    }
    std::stringstream stream;
    stream << "Relocations debug information :\n";
    for (const auto &symbol : relocatedSymbols) {
        stream << " * \"" << symbol.first << "\" [" << symbol.second.symbol.size << " bytes]";
        stream << " " << asString(symbol.second.symbol.segment) << "_SEGMENT@" << symbol.second.symbol.offset;
        stream << " -> " << std::hex << std::showbase << symbol.second.gpuAddress << " GPUVA" << std::dec;
        stream << "\n";
    }
    return stream.str();
}

void Linker::applyDebugDataRelocations(const NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> &decodedElf, ArrayRef<uint8_t> inputOutputElf, const SegmentInfo &text, const SegmentInfo &globalData, const SegmentInfo &constData) {

    for (auto &reloc : decodedElf.getDebugInfoRelocations()) {
        auto targetSectionName = decodedElf.getSectionName(reloc.targetSectionIndex);
        auto sectionName = decodedElf.getSectionName(reloc.symbolSectionIndex);
        auto symbolAddress = decodedElf.getSymbolValue(reloc.symbolTableIndex);

        if (sectionName == Elf::SpecialSectionNames::text) {
            symbolAddress += text.gpuAddress;
        } else if (ConstStringRef(sectionName.c_str()).startsWith(Zebin::Elf::SectionNames::dataConst.data())) {
            symbolAddress += constData.gpuAddress;
        } else if (ConstStringRef(sectionName.c_str()).startsWith(Zebin::Elf::SectionNames::dataGlobal.data())) {
            symbolAddress += globalData.gpuAddress;
        } else {
            // do not offset debug sections
            if (!ConstStringRef(sectionName.c_str()).startsWith(Elf::SpecialSectionNames::debug.data())) {
                // skip other sections
                continue;
            }
        }

        symbolAddress += reloc.addend;

        auto targetSectionOffset = decodedElf.sectionHeaders[reloc.targetSectionIndex].header->offset;
        auto relocLocation = reinterpret_cast<uint64_t>(inputOutputElf.begin()) + targetSectionOffset + reloc.offset;

        if (static_cast<Elf::RelocationX8664Type>(reloc.relocType) == Elf::RelocationX8664Type::relocation64) {
            *reinterpret_cast<uint64_t *>(relocLocation) = symbolAddress;
        } else if (static_cast<Elf::RelocationX8664Type>(reloc.relocType) == Elf::RelocationX8664Type::relocation32) {
            *reinterpret_cast<uint32_t *>(relocLocation) = static_cast<uint32_t>(symbolAddress & uint32_t(-1));
        }
    }
}

bool Linker::resolveExternalFunctions(const KernelDescriptorsT &kernelDescriptors, std::vector<ExternalFunctionInfo> &externalFunctions) {
    if (externalFunctions.size() == 0U) {
        return true;
    }

    ExternalFunctionInfosT externalFunctionsPtrs;
    FunctionDependenciesT functionDependenciesPtrs;
    KernelDependenciesT kernelDependenciesPtrs;
    KernelDescriptorMapT nameToKernelDescriptor;

    auto toPtrVec = [](auto &inVec, auto &outPtrVec) {
        outPtrVec.resize(inVec.size());
        for (size_t i = 0; i < inVec.size(); i++) {
            outPtrVec[i] = &inVec[i];
        }
    };
    toPtrVec(externalFunctions, externalFunctionsPtrs);
    toPtrVec(data.getFunctionDependencies(), functionDependenciesPtrs);
    toPtrVec(data.getKernelDependencies(), kernelDependenciesPtrs);
    for (auto &kd : kernelDescriptors) {
        nameToKernelDescriptor[kd->kernelMetadata.kernelName] = kd;
    }

    auto error = NEO::resolveExternalDependencies(externalFunctionsPtrs, kernelDependenciesPtrs, functionDependenciesPtrs, nameToKernelDescriptor);
    return (error == RESOLVE_SUCCESS) ? true : false;
}

void Linker::resolveImplicitArgs(const KernelDescriptorsT &kernelDescriptors, Device *pDevice) {
    for (auto i = 0u; i < kernelDescriptors.size(); i++) {
        UNRECOVERABLE_IF(!kernelDescriptors[i]);
        KernelDescriptor &kernelDescriptor = *kernelDescriptors[i];
        auto pImplicitArgsRelocs = pImplicitArgsRelocationAddresses.find(i);
        if (pImplicitArgsRelocs != pImplicitArgsRelocationAddresses.end()) {
            for (const auto &pImplicitArgsReloc : pImplicitArgsRelocs->second) {
                UNRECOVERABLE_IF(!pDevice);
                bool addImplcictArgs = kernelDescriptor.kernelAttributes.flags.useStackCalls || (userModule && pDevice->getDebugger() != nullptr);
                kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs |= addImplcictArgs;
                if (kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs) {
                    uint64_t implicitArgsSize = 0;
                    uint8_t version = kernelDescriptor.kernelMetadata.indirectAccessBuffer;
                    if (version == 0) {
                        version = pDevice->getGfxCoreHelper().getImplicitArgsVersion();
                    }

                    if (version == 0) {
                        implicitArgsSize = ImplicitArgsV0::getAlignedSize();
                    } else if (version == 1) {
                        implicitArgsSize = ImplicitArgsV1::getAlignedSize();
                    } else if (version == 2) {
                        implicitArgsSize = ImplicitArgsV2::getAlignedSize();
                    } else {
                        UNRECOVERABLE_IF(true);
                    }
                    // Choose relocation size based on relocation type
                    auto patchSize = pImplicitArgsReloc.second == RelocationInfo::Type::address ? 8 : 4;
                    patchWithRequiredSize(pImplicitArgsReloc.first, patchSize, implicitArgsSize);
                }
            }
        }
    }
}

void Linker::resolveBuiltins(Device *pDevice, UnresolvedExternals &outUnresolvedExternals, const std::vector<PatchableSegment> &instructionsSegments, const KernelDescriptorsT &kernelDescriptors) {
    auto &productHelper = pDevice->getProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    int vecIndex = static_cast<int>(outUnresolvedExternals.size() - 1u);
    for (; vecIndex >= 0; --vecIndex) {
        if (outUnresolvedExternals[vecIndex].unresolvedRelocation.symbolName == subDeviceID) {
            if (productHelper.isResolvingSubDeviceIDNeeded(releaseHelper)) {
                RelocatedSymbol<SymbolInfo> symbol;
                symbol.gpuAddress = static_cast<uintptr_t>(pDevice->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress());
                auto relocAddress = ptrOffset(instructionsSegments[outUnresolvedExternals[vecIndex].instructionsSegmentId].hostPointer,
                                              static_cast<uintptr_t>(outUnresolvedExternals[vecIndex].unresolvedRelocation.offset));

                NEO::Linker::patchAddress(relocAddress, symbol.gpuAddress, outUnresolvedExternals[vecIndex].unresolvedRelocation);
            }
            outUnresolvedExternals[vecIndex] = outUnresolvedExternals[outUnresolvedExternals.size() - 1u];
            outUnresolvedExternals.resize(outUnresolvedExternals.size() - 1u);
        } else if (outUnresolvedExternals[vecIndex].unresolvedRelocation.symbolName == perThreadOff) {
            RelocatedSymbol<SymbolInfo> symbol;

            auto kernelName = Zebin::getKernelNameFromSectionName(ConstStringRef(outUnresolvedExternals[vecIndex].unresolvedRelocation.relocationSegmentName)).str();

            auto kernelDescriptor = std::find_if(kernelDescriptors.begin(), kernelDescriptors.end(), [&kernelName](const KernelDescriptor *obj) { return obj->kernelMetadata.kernelName == kernelName; });
            if (kernelDescriptor != std::end(kernelDescriptors)) {
                symbol.gpuAddress = (*kernelDescriptor)->getPerThreadDataOffset();
                auto relocAddress = ptrOffset(instructionsSegments[outUnresolvedExternals[vecIndex].instructionsSegmentId].hostPointer,
                                              static_cast<uintptr_t>(outUnresolvedExternals[vecIndex].unresolvedRelocation.offset));

                NEO::Linker::patchAddress(relocAddress, symbol.gpuAddress, outUnresolvedExternals[vecIndex].unresolvedRelocation);
                outUnresolvedExternals[vecIndex] = outUnresolvedExternals[outUnresolvedExternals.size() - 1u];
                outUnresolvedExternals.resize(outUnresolvedExternals.size() - 1u);
            }
        }
    }
}

template <typename PatchSizeT>
void Linker::patchIncrement(void *dstBegin, size_t relocationOffset, const void *initData, uint64_t incrementValue) {
    if (nullptr == initData) {
        *(reinterpret_cast<PatchSizeT *>(dstBegin) + relocationOffset) = static_cast<PatchSizeT>(incrementValue);
        return;
    }
    auto initValue = ptrOffset(initData, relocationOffset);
    PatchSizeT value = 0;
    memcpy_s(&value, sizeof(PatchSizeT), initValue, sizeof(PatchSizeT));
    value += static_cast<PatchSizeT>(incrementValue);

    auto destination = ptrOffset(dstBegin, relocationOffset);
    memcpy_s(destination, sizeof(PatchSizeT), &value, sizeof(PatchSizeT));
}
} // namespace NEO
