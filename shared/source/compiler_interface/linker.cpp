/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/linker.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/linker.inl"
#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/program/program_info.h"

#include "RelocationInfo.h"

#include <sstream>
#include <unordered_map>

namespace NEO {

SegmentType LinkerInput::getSegmentForSection(ConstStringRef name) {
    if (name == NEO::Elf::SectionsNamesZebin::dataConst || name == NEO::Elf::SectionsNamesZebin::dataGlobalConst) {
        return NEO::SegmentType::GlobalConstants;
    } else if (name == NEO::Elf::SectionsNamesZebin::dataGlobal) {
        return NEO::SegmentType::GlobalVariables;
    } else if (name == NEO::Elf::SectionsNamesZebin::dataConstString) {
        return NEO::SegmentType::GlobalStrings;
    } else if (name.startsWith(NEO::Elf::SpecialSectionNames::text.data())) {
        return NEO::SegmentType::Instructions;
    }
    return NEO::SegmentType::Unknown;
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
        switch (symbolEntryIt->s_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.segment = SegmentType::GlobalVariables;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.segment = SegmentType::GlobalConstants;
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
        switch (symbolEntryIt->s_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::S_UNDEF:
            symbols.erase(symbolEntryIt->s_name);
            break;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.segment = SegmentType::GlobalVariables;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.segment = SegmentType::GlobalConstants;
            traits.exportsGlobalConstants = true;
            break;
        case vISA::S_FUNC:
            symbolInfo.segment = SegmentType::Instructions;
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
        relocInfo.relocationSegment = SegmentType::Instructions;
        switch (relocEntryIt->r_type) {
        default:
            DEBUG_BREAK_IF(true);
            this->valid = false;
            return false;
        case vISA::R_SYM_ADDR:
            relocInfo.type = RelocationInfo::Type::Address;
            break;
        case vISA::R_SYM_ADDR_32:
            relocInfo.type = RelocationInfo::Type::AddressLow;
            break;
        case vISA::R_SYM_ADDR_32_HI:
            relocInfo.type = RelocationInfo::Type::AddressHigh;
            break;
        case vISA::R_PER_THREAD_PAYLOAD_OFFSET_32:
            relocInfo.type = RelocationInfo::Type::PerThreadPayloadOffset;
            break;
        }
        outRelocInfo.push_back(std::move(relocInfo));
    }
    return true;
}

void LinkerInput::addDataRelocationInfo(const RelocationInfo &relocationInfo) {
    DEBUG_BREAK_IF((relocationInfo.relocationSegment != SegmentType::GlobalConstants) && (relocationInfo.relocationSegment != SegmentType::GlobalVariables));
    this->traits.requiresPatchingOfGlobalVariablesBuffer |= (relocationInfo.relocationSegment == SegmentType::GlobalVariables);
    this->traits.requiresPatchingOfGlobalConstantsBuffer |= (relocationInfo.relocationSegment == SegmentType::GlobalConstants);
    this->dataRelocations.push_back(relocationInfo);
}

void LinkerInput::addElfTextSegmentRelocation(RelocationInfo relocationInfo, uint32_t instructionsSegmentId) {
    this->traits.requiresPatchingOfInstructionSegments = true;

    if (instructionsSegmentId >= textRelocations.size()) {
        static_assert(std::is_nothrow_move_constructible<decltype(textRelocations[0])>::value, "");
        textRelocations.resize(instructionsSegmentId + 1);
    }

    auto &outRelocInfo = textRelocations[instructionsSegmentId];

    relocationInfo.relocationSegment = SegmentType::Instructions;

    outRelocInfo.push_back(std::move(relocationInfo));
}

template void LinkerInput::decodeElfSymbolTableAndRelocations<Elf::EI_CLASS_32>(Elf::Elf<Elf::EI_CLASS_32> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);
template void LinkerInput::decodeElfSymbolTableAndRelocations<Elf::EI_CLASS_64>(Elf::Elf<Elf::EI_CLASS_64> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
void LinkerInput::decodeElfSymbolTableAndRelocations(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId) {
    symbols.reserve(elf.getSymbols().size());
    for (auto &symbol : elf.getSymbols()) {
        auto bind = elf.extractSymbolBind(symbol);
        auto type = elf.extractSymbolType(symbol);

        if (bind == Elf::SYMBOL_TABLE_BIND::STB_GLOBAL) {
            SymbolInfo symbolInfo;
            symbolInfo.offset = static_cast<uint32_t>(symbol.value);
            symbolInfo.size = static_cast<uint32_t>(symbol.size);

            auto symbolSectionName = elf.getSectionName(symbol.shndx);
            auto symbolSegment = getSegmentForSection(symbolSectionName);
            if (NEO::SegmentType::Unknown == symbolSegment) {
                continue;
            }
            switch (type) {
            default:
                DEBUG_BREAK_IF(type != Elf::SYMBOL_TABLE_TYPE::STT_NOTYPE);
                continue;
            case Elf::SYMBOL_TABLE_TYPE::STT_OBJECT:
                symbolInfo.segment = symbolSegment;
                traits.exportsGlobalVariables |= symbolSegment == SegmentType::GlobalVariables;
                traits.exportsGlobalConstants |= symbolSegment == SegmentType::GlobalConstants;
                break;
            case Elf::SYMBOL_TABLE_TYPE::STT_FUNC: {
                auto kernelName = symbolSectionName.substr(static_cast<int>(NEO::Elf::SectionsNamesZebin::textPrefix.length()));
                auto segmentIdIter = nameToSegmentId.find(kernelName);
                if (segmentIdIter != nameToSegmentId.end()) {
                    symbolInfo.segment = SegmentType::Instructions;
                    traits.exportsFunctions = true;
                    int32_t instructionsSegmentId = static_cast<int32_t>(segmentIdIter->second);
                    UNRECOVERABLE_IF((this->exportedFunctionsSegmentId != -1) && (this->exportedFunctionsSegmentId != instructionsSegmentId));
                    this->exportedFunctionsSegmentId = instructionsSegmentId;
                    extFuncSymbols.push_back({elf.getSymbolName(symbol.name), symbolInfo});
                }
            } break;
            }
            symbols.insert({elf.getSymbolName(symbol.name), symbolInfo});
        } else if (type == Elf::SYMBOL_TABLE_TYPE::STT_FUNC) {
            DEBUG_BREAK_IF(Elf::SYMBOL_TABLE_BIND::STB_LOCAL != bind);
            LocalFuncSymbolInfo localSymbolInfo;
            localSymbolInfo.offset = static_cast<uint32_t>(symbol.value);
            localSymbolInfo.size = static_cast<uint32_t>(symbol.size);
            auto symbolSectionName = elf.getSectionName(symbol.shndx);
            localSymbolInfo.targetedKernelSectionName = symbolSectionName.substr(Elf::SectionsNamesZebin::textPrefix.length());
            localSymbols.insert({elf.getSymbolName(symbol.name), localSymbolInfo});
        }
    }

    for (auto &reloc : elf.getRelocations()) {
        NEO::LinkerInput::RelocationInfo relocationInfo;
        relocationInfo.offset = reloc.offset;
        relocationInfo.addend = reloc.addend;
        relocationInfo.symbolName = reloc.symbolName;

        switch (reloc.relocType) {
        case uint32_t(Elf::RELOCATION_X8664_TYPE::R_X8664_64):
            relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
            break;
        case uint32_t(Elf::RELOCATION_X8664_TYPE::R_X8664_32):
            relocationInfo.type = NEO::LinkerInput::RelocationInfo::Type::AddressLow;
            break;
        default: // Zebin relocation type
            relocationInfo.type = reloc.relocType < uint32_t(NEO::LinkerInput::RelocationInfo::Type::RelocTypeMax)
                                      ? static_cast<NEO::LinkerInput::RelocationInfo::Type>(reloc.relocType)
                                      : NEO::LinkerInput::RelocationInfo::Type::Unknown;
            break;
        }
        auto name = elf.getSectionName(reloc.targetSectionIndex);
        ConstStringRef nameRef(name);

        if (nameRef.startsWith(NEO::Elf::SectionsNamesZebin::textPrefix.data())) {
            auto kernelName = name.substr(static_cast<int>(NEO::Elf::SectionsNamesZebin::textPrefix.length()));
            auto segmentIdIter = nameToSegmentId.find(kernelName);
            if (segmentIdIter != nameToSegmentId.end()) {
                this->addElfTextSegmentRelocation(relocationInfo, segmentIdIter->second);
                parseRelocationForExtFuncUsage(relocationInfo, kernelName);
            }
        } else if (nameRef.startsWith(NEO::Elf::SpecialSectionNames::data.data())) {
            auto symbolSectionName = elf.getSectionName(reloc.symbolSectionIndex);
            auto symbolSegment = getSegmentForSection(symbolSectionName);
            auto relocationSegment = getSegmentForSection(nameRef);
            if (symbolSegment != NEO::SegmentType::Unknown &&
                (relocationSegment == NEO::SegmentType::GlobalConstants || relocationSegment == NEO::SegmentType::GlobalVariables)) {
                relocationInfo.relocationSegment = relocationSegment;
                this->addDataRelocationInfo(relocationInfo);
            }
        }
    }
}

void LinkerInput::parseRelocationForExtFuncUsage(const RelocationInfo &relocInfo, const std::string &kernelName) {
    auto extFuncSymIt = std::find_if(extFuncSymbols.begin(), extFuncSymbols.end(), [relocInfo](auto &pair) {
        return pair.first == relocInfo.symbolName;
    });
    if (extFuncSymIt != extFuncSymbols.end()) {
        if (kernelName == Elf::SectionsNamesZebin::externalFunctions.str()) {
            auto callerIt = std::find_if(extFuncSymbols.begin(), extFuncSymbols.end(), [relocInfo](auto &pair) {
                auto &symbol = pair.second;
                return relocInfo.offset >= symbol.offset && relocInfo.offset < symbol.offset + symbol.size;
            });
            if (callerIt != extFuncSymbols.end()) {
                extFunDependencies.push_back({relocInfo.symbolName, callerIt->first});
            }
        } else {
            kernelDependencies.push_back({relocInfo.symbolName, kernelName});
        }
    }
}

bool Linker::processRelocations(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions, const SegmentInfo &globalStrings,
                                const PatchableSegments &instructionsSegments) {
    relocatedSymbols.reserve(data.getSymbols().size());
    for (auto &symbol : data.getSymbols()) {
        const SegmentInfo *seg = nullptr;
        switch (symbol.second.segment) {
        default:
            DEBUG_BREAK_IF(true);
            return false;
        case SegmentType::GlobalVariables:
            seg = &globalVariables;
            break;
        case SegmentType::GlobalConstants:
            seg = &globalConstants;
            break;
        case SegmentType::GlobalStrings:
            seg = &globalStrings;
            break;
        case SegmentType::Instructions:
            seg = &exportedFunctions;
            break;
        }
        uintptr_t gpuAddress = seg->gpuAddress + symbol.second.offset;
        if (symbol.second.offset + symbol.second.size > seg->segmentSize) {
            DEBUG_BREAK_IF(true);
            return false;
        }
        relocatedSymbols[symbol.first] = {symbol.second, gpuAddress};
    }
    localRelocatedSymbols.reserve(data.getLocalSymbols().size());
    for (auto &localSymbol : data.getLocalSymbols()) {
        for (auto &s : instructionsSegments) {
            if (s.kernelName == localSymbol.second.targetedKernelSectionName) {
                uintptr_t gpuAddress = s.gpuAddress + localSymbol.second.offset;
                localRelocatedSymbols[localSymbol.first] = {localSymbol.second, gpuAddress};
            }
        }
    }
    return true;
}

uint32_t addressSizeInBytes(LinkerInput::RelocationInfo::Type relocationtype) {
    return (relocationtype == LinkerInput::RelocationInfo::Type::Address) ? sizeof(uintptr_t) : sizeof(uint32_t);
}

void Linker::patchAddress(void *relocAddress, const uint64_t value, const Linker::RelocationInfo &relocation) {
    uint64_t gpuAddressAs64bit = static_cast<uint64_t>(value);
    switch (relocation.type) {
    default:
        UNRECOVERABLE_IF(RelocationInfo::Type::Address != relocation.type);
        *reinterpret_cast<uint64_t *>(relocAddress) = gpuAddressAs64bit;
        break;
    case RelocationInfo::Type::AddressLow:
        *reinterpret_cast<uint32_t *>(relocAddress) = static_cast<uint32_t>(gpuAddressAs64bit & 0xffffffff);
        break;
    case RelocationInfo::Type::AddressHigh:
        *reinterpret_cast<uint32_t *>(relocAddress) = static_cast<uint32_t>((gpuAddressAs64bit >> 32) & 0xffffffff);
        break;
    }
}

void Linker::patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals, const KernelDescriptorsT &kernelDescriptors) {
    if (false == data.getTraits().requiresPatchingOfInstructionSegments) {
        return;
    }
    UNRECOVERABLE_IF(data.getRelocationsInInstructionSegments().size() > instructionsSegments.size());
    auto segIt = instructionsSegments.begin();
    uint32_t segId = 0u;
    for (auto relocsIt = data.getRelocationsInInstructionSegments().begin(), relocsEnd = data.getRelocationsInInstructionSegments().end();
         relocsIt != relocsEnd; ++relocsIt, ++segIt, ++segId) {
        auto &thisSegmentRelocs = *relocsIt;
        const PatchableSegment &instSeg = *segIt;
        for (const auto &relocation : thisSegmentRelocs) {
            UNRECOVERABLE_IF(nullptr == instSeg.hostPointer);
            bool invalidOffset = relocation.offset + addressSizeInBytes(relocation.type) > instSeg.segmentSize;
            DEBUG_BREAK_IF(invalidOffset);

            auto relocAddress = ptrOffset(instSeg.hostPointer, static_cast<uintptr_t>(relocation.offset));
            if (relocation.type == LinkerInput::RelocationInfo::Type::PerThreadPayloadOffset) {
                *reinterpret_cast<uint32_t *>(relocAddress) = kernelDescriptors.at(segId)->kernelAttributes.crossThreadDataSize;
                continue;
            };
            if (relocation.symbolName == implicitArgsRelocationSymbolName) {
                if (pImplicitArgsRelocationAddresses.find(segId) == pImplicitArgsRelocationAddresses.end()) {
                    pImplicitArgsRelocationAddresses.insert({segId, {}});
                }
                pImplicitArgsRelocationAddresses[segId].push_back(reinterpret_cast<uint32_t *>(relocAddress));
                continue;
            }
            auto symbolIt = relocatedSymbols.find(relocation.symbolName);
            if (symbolIt == relocatedSymbols.end()) {
                auto localSymbolIt = localRelocatedSymbols.find(relocation.symbolName);
                if (localRelocatedSymbols.end() != localSymbolIt) {
                    if (localSymbolIt->first == kernelDescriptors[segId]->kernelMetadata.kernelName) {
                        uint64_t patchValue = localSymbolIt->second.gpuAddress + relocation.addend;
                        patchAddress(relocAddress, patchValue, relocation);
                        continue;
                    }
                } else if (relocation.symbolName.empty()) {
                    uint64_t patchValue = 0;
                    patchAddress(relocAddress, patchValue, relocation);
                    continue;
                }
            }
            bool unresolvedExternal = (symbolIt == relocatedSymbols.end());
            if (invalidOffset || unresolvedExternal) {
                uint32_t segId = static_cast<uint32_t>(segIt - instructionsSegments.begin());
                outUnresolvedExternals.push_back(UnresolvedExternal{relocation, segId, invalidOffset});
                continue;
            }
            uint64_t patchValue = symbolIt->second.gpuAddress + relocation.addend;
            patchAddress(relocAddress, patchValue, relocation);
        }
    }
}

void Linker::patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                               GraphicsAllocation *globalVariablesSeg, GraphicsAllocation *globalConstantsSeg,
                               std::vector<UnresolvedExternal> &outUnresolvedExternals, Device *pDevice,
                               const void *constantsInitData, const void *variablesInitData) {
    for (const auto &relocation : data.getDataRelocations()) {
        auto symbolIt = relocatedSymbols.find(relocation.symbolName);
        if (symbolIt == relocatedSymbols.end()) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }
        uint64_t srcGpuAddressAs64Bit = symbolIt->second.gpuAddress;

        GraphicsAllocation *dst = nullptr;
        const void *initData = nullptr;
        if (SegmentType::GlobalVariables == relocation.relocationSegment) {
            dst = globalVariablesSeg;
            initData = variablesInitData;
        } else if (SegmentType::GlobalConstants == relocation.relocationSegment) {
            dst = globalConstantsSeg;
            initData = constantsInitData;
        } else {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }
        UNRECOVERABLE_IF(nullptr == dst);

        auto relocType = (LinkerInput::Traits::PointerSize::Ptr32bit == data.getTraits().pointerSize) ? RelocationInfo::Type::AddressLow : relocation.type;
        bool invalidOffset = relocation.offset + addressSizeInBytes(relocType) > dst->getUnderlyingBufferSize();
        DEBUG_BREAK_IF(invalidOffset);
        if (invalidOffset) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }

        uint64_t incrementValue = srcGpuAddressAs64Bit + relocation.addend;
        switch (relocType) {
        default:
            UNRECOVERABLE_IF(RelocationInfo::Type::Address != relocType);
            patchIncrement<uint64_t>(pDevice, dst, static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
        case RelocationInfo::Type::AddressLow:
            incrementValue = incrementValue & 0xffffffff;
            patchIncrement<uint32_t>(pDevice, dst, static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
        case RelocationInfo::Type::AddressHigh:
            incrementValue = (incrementValue >> 32) & 0xffffffff;
            patchIncrement<uint32_t>(pDevice, dst, static_cast<size_t>(relocation.offset), initData, incrementValue);
            break;
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

            if (unresExtern.unresolvedRelocation.relocationSegment == NEO::SegmentType::Instructions) {
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
    stream << "Relocations debug informations :\n";
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
        } else if (ConstStringRef(sectionName.c_str()).startsWith(Elf::SectionsNamesZebin::dataConst.data())) {
            symbolAddress += constData.gpuAddress;
        } else if (ConstStringRef(sectionName.c_str()).startsWith(Elf::SectionsNamesZebin::dataGlobal.data())) {
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

        if (static_cast<Elf::RELOCATION_X8664_TYPE>(reloc.relocType) == Elf::RELOCATION_X8664_TYPE::R_X8664_64) {
            *reinterpret_cast<uint64_t *>(relocLocation) = symbolAddress;
        } else if (static_cast<Elf::RELOCATION_X8664_TYPE>(reloc.relocType) == Elf::RELOCATION_X8664_TYPE::R_X8664_32) {
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

    auto error = NEO::resolveBarrierCount(externalFunctionsPtrs, kernelDependenciesPtrs, functionDependenciesPtrs, nameToKernelDescriptor);
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
                kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = kernelDescriptor.kernelAttributes.flags.useStackCalls || pDevice->getDebugger() != nullptr;
                if (kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs) {
                    *pImplicitArgsReloc = sizeof(ImplicitArgs);
                }
            }
        }
    }
}

void Linker::resolveBuiltins(Device *pDevice, UnresolvedExternals &outUnresolvedExternals, const std::vector<PatchableSegment> &instructionsSegments) {
    int vecIndex = static_cast<int>(outUnresolvedExternals.size() - 1u);
    for (; vecIndex >= 0; --vecIndex) {
        if (outUnresolvedExternals[vecIndex].unresolvedRelocation.symbolName == subDeviceID) {
            RelocatedSymbol<SymbolInfo> symbol;
            symbol.gpuAddress = static_cast<uintptr_t>(pDevice->getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress());
            auto relocAddress = ptrOffset(instructionsSegments[outUnresolvedExternals[vecIndex].instructionsSegmentId].hostPointer,
                                          static_cast<uintptr_t>(outUnresolvedExternals[vecIndex].unresolvedRelocation.offset));

            NEO::Linker::patchAddress(relocAddress, symbol.gpuAddress, outUnresolvedExternals[vecIndex].unresolvedRelocation);

            outUnresolvedExternals[vecIndex] = outUnresolvedExternals[outUnresolvedExternals.size() - 1u];
            outUnresolvedExternals.resize(outUnresolvedExternals.size() - 1u);
        }
    }
}
} // namespace NEO
