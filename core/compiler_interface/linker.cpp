/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "linker.h"

#include "common/compiler_support.h"
#include "core/helpers/debug_helpers.h"
#include "core/helpers/ptr_math.h"

#include "RelocationInfo.h"

#include <sstream>

namespace NEO {

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
            return false;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.type = SymbolInfo::GlobalVariable;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.type = SymbolInfo::GlobalConstant;
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
            return false;
        case vISA::S_GLOBAL_VAR:
            symbolInfo.type = SymbolInfo::GlobalVariable;
            traits.exportsGlobalVariables = true;
            break;
        case vISA::S_GLOBAL_VAR_CONST:
            symbolInfo.type = SymbolInfo::GlobalConstant;
            traits.exportsGlobalConstants = true;
            break;
        case vISA::S_FUNC:
            symbolInfo.type = SymbolInfo::Function;
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
    if (instructionsSegmentId >= relocations.size()) {
        static_assert(std::is_nothrow_move_constructible<decltype(relocations[0])>::value, "");
        relocations.resize(instructionsSegmentId + 1);
    }

    auto &outRelocInfo = relocations[instructionsSegmentId];
    outRelocInfo.reserve(numEntries);
    for (; relocEntryIt != relocEntryEnd; ++relocEntryIt) {
        RelocationInfo relocInfo{};
        relocInfo.offset = relocEntryIt->r_offset;
        relocInfo.symbolName = relocEntryIt->r_symbol;
        switch (relocEntryIt->r_type) {
        default:
            DEBUG_BREAK_IF(true);
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
        }
        outRelocInfo.push_back(std::move(relocInfo));
    }
    return true;
}

bool Linker::processRelocations(const Segment &globalVariables, const Segment &globalConstants, const Segment &exportedFunctions) {
    relocatedSymbols.reserve(data.getSymbols().size());
    for (auto &symbol : data.getSymbols()) {
        const Segment *seg = nullptr;
        switch (symbol.second.type) {
        default:
            DEBUG_BREAK_IF(true);
            return false;
        case SymbolInfo::GlobalVariable:
            seg = &globalVariables;
            break;
        case SymbolInfo::GlobalConstant:
            seg = &globalConstants;
            break;
        case SymbolInfo::Function:
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
    return true;
}

bool Linker::patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals) {
    if (false == data.getTraits().requiresPatchingOfInstructionSegments) {
        return true;
    }
    UNRECOVERABLE_IF(data.getRelocations().size() > instructionsSegments.size());
    std::vector<UnresolvedExternal> unresolvedExternals;
    auto segIt = instructionsSegments.begin();
    for (auto relocsIt = data.getRelocations().begin(), relocsEnd = data.getRelocations().end();
         relocsIt != relocsEnd; ++relocsIt, ++segIt) {
        auto &thisSegmentRelocs = *relocsIt;
        const PatchableSegment &instSeg = *segIt;
        for (const auto &relocation : thisSegmentRelocs) {
            auto relocAddress = ptrOffset(instSeg.hostPointer, relocation.offset);
            auto symbolIt = relocatedSymbols.find(relocation.symbolName);

            bool invalidOffset = relocation.offset + sizeof(uintptr_t) > instSeg.segmentSize;
            bool unresolvedExternal = (symbolIt == relocatedSymbols.end());

            DEBUG_BREAK_IF(invalidOffset);
            if (invalidOffset || unresolvedExternal) {
                uint32_t segId = static_cast<uint32_t>(segIt - instructionsSegments.begin());
                unresolvedExternals.push_back(UnresolvedExternal{relocation, segId, invalidOffset});
                continue;
            }

            uint64_t gpuAddressAs64bit = static_cast<uint64_t>(symbolIt->second.gpuAddress);
            switch (relocation.type) {
            default:
                UNRECOVERABLE_IF(RelocationInfo::Type::Address != relocation.type);
                *reinterpret_cast<uintptr_t *>(relocAddress) = symbolIt->second.gpuAddress;
                break;
            case RelocationInfo::Type::AddressLow:
                *reinterpret_cast<uint32_t *>(relocAddress) = static_cast<uint32_t>(gpuAddressAs64bit & 0xffffffff);
                break;
            case RelocationInfo::Type::AddressHigh:
                *reinterpret_cast<uint32_t *>(relocAddress) = static_cast<uint32_t>((gpuAddressAs64bit >> 32) & 0xffffffff);
                break;
            }
        }
    }
    outUnresolvedExternals.swap(unresolvedExternals);
    return outUnresolvedExternals.size() == 0;
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
            errorStream << unresExtern.unresolvedRelocation.symbolName << " at offset " << unresExtern.unresolvedRelocation.offset
                        << " in instructions segment #" << unresExtern.instructionsSegmentId;
            if (instructionsSegmentsNames.size() > unresExtern.instructionsSegmentId) {
                errorStream << " (aka " << instructionsSegmentsNames[unresExtern.instructionsSegmentId] << ")";
            }
            errorStream << "\n";
        }
    }
    return errorStream.str();
}

} // namespace NEO
