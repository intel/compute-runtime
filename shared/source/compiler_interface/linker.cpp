/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "linker.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/utilities/compiler_support.h"

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
        relocInfo.symbolSegment = SegmentType::Unknown;
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
        }
        outRelocInfo.push_back(std::move(relocInfo));
    }
    return true;
}

void LinkerInput::addDataRelocationInfo(const RelocationInfo &relocationInfo) {
    DEBUG_BREAK_IF((relocationInfo.relocationSegment != SegmentType::GlobalConstants) && (relocationInfo.relocationSegment != SegmentType::GlobalVariables));
    DEBUG_BREAK_IF((relocationInfo.symbolSegment != SegmentType::GlobalConstants) && (relocationInfo.symbolSegment != SegmentType::GlobalVariables));
    DEBUG_BREAK_IF(relocationInfo.type == LinkerInput::RelocationInfo::Type::AddressHigh);
    this->traits.requiresPatchingOfGlobalVariablesBuffer |= (relocationInfo.relocationSegment == SegmentType::GlobalVariables);
    this->traits.requiresPatchingOfGlobalConstantsBuffer |= (relocationInfo.relocationSegment == SegmentType::GlobalConstants);
    this->dataRelocations.push_back(relocationInfo);
}

bool Linker::processRelocations(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions) {
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
    return true;
}

uint32_t addressSizeInBytes(LinkerInput::RelocationInfo::Type relocationtype) {
    return (relocationtype == LinkerInput::RelocationInfo::Type::Address) ? sizeof(uintptr_t) : sizeof(uint32_t);
}

bool Linker::patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals) {
    if (false == data.getTraits().requiresPatchingOfInstructionSegments) {
        return true;
    }
    UNRECOVERABLE_IF(data.getRelocationsInInstructionSegments().size() > instructionsSegments.size());
    auto unresolvedExternalsPrev = outUnresolvedExternals.size();
    auto segIt = instructionsSegments.begin();
    for (auto relocsIt = data.getRelocationsInInstructionSegments().begin(), relocsEnd = data.getRelocationsInInstructionSegments().end();
         relocsIt != relocsEnd; ++relocsIt, ++segIt) {
        auto &thisSegmentRelocs = *relocsIt;
        const PatchableSegment &instSeg = *segIt;
        for (const auto &relocation : thisSegmentRelocs) {
            UNRECOVERABLE_IF(nullptr == instSeg.hostPointer);
            auto relocAddress = ptrOffset(instSeg.hostPointer, static_cast<uintptr_t>(relocation.offset));
            auto symbolIt = relocatedSymbols.find(relocation.symbolName);

            bool invalidOffset = relocation.offset + addressSizeInBytes(relocation.type) > instSeg.segmentSize;
            bool unresolvedExternal = (symbolIt == relocatedSymbols.end());

            DEBUG_BREAK_IF(invalidOffset);
            if (invalidOffset || unresolvedExternal) {
                uint32_t segId = static_cast<uint32_t>(segIt - instructionsSegments.begin());
                outUnresolvedExternals.push_back(UnresolvedExternal{relocation, segId, invalidOffset});
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
    return outUnresolvedExternals.size() == unresolvedExternalsPrev;
}

bool Linker::patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                               PatchableSegment &globalVariablesSeg, PatchableSegment &globalConstantsSeg,
                               std::vector<UnresolvedExternal> &outUnresolvedExternals) {
    if (false == (data.getTraits().requiresPatchingOfGlobalConstantsBuffer || data.getTraits().requiresPatchingOfGlobalVariablesBuffer)) {
        return true;
    }

    auto unresolvedExternalsPrev = outUnresolvedExternals.size();
    for (const auto &relocation : data.getDataRelocations()) {
        const SegmentInfo *src = nullptr;
        const PatchableSegment *dst = nullptr;
        switch (relocation.symbolSegment) {
        default:
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        case SegmentType::GlobalVariables:
            src = &globalVariablesSegInfo;
            break;
        case SegmentType::GlobalConstants:
            src = &globalConstantsSegInfo;
            break;
        }
        switch (relocation.relocationSegment) {
        default:
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        case SegmentType::GlobalVariables:
            dst = &globalVariablesSeg;
            break;
        case SegmentType::GlobalConstants:
            dst = &globalConstantsSeg;
            break;
        }

        UNRECOVERABLE_IF(nullptr == dst->hostPointer);

        if (RelocationInfo::Type::AddressHigh == relocation.type) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }

        auto relocType = (LinkerInput::Traits::PointerSize::Ptr32bit == data.getTraits().pointerSize) ? RelocationInfo::Type::AddressLow : relocation.type;
        bool invalidOffset = relocation.offset + addressSizeInBytes(relocType) > dst->segmentSize;
        DEBUG_BREAK_IF(invalidOffset);
        if (invalidOffset) {
            outUnresolvedExternals.push_back(UnresolvedExternal{relocation});
            continue;
        }

        uint64_t gpuAddressAs64bit = src->gpuAddress;
        auto relocAddress = ptrOffset(dst->hostPointer, static_cast<uintptr_t>(relocation.offset));
        switch (relocType) {
        default:
            UNRECOVERABLE_IF(RelocationInfo::Type::Address != relocType);
            patchIncrement(relocAddress, sizeof(uintptr_t), gpuAddressAs64bit);
            break;
        case RelocationInfo::Type::AddressLow:
            patchIncrement(relocAddress, 4, static_cast<uint32_t>(gpuAddressAs64bit & 0xffffffff));
            break;
        }
    }
    return outUnresolvedExternals.size() == unresolvedExternalsPrev;
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
                errorStream << " address of segment #" << asString(unresExtern.unresolvedRelocation.symbolSegment) << " at offset " << unresExtern.unresolvedRelocation.offset
                            << " in data segment #" << asString(unresExtern.unresolvedRelocation.relocationSegment);
            }
            errorStream << "\n";
        }
    }
    return errorStream.str();
}

} // namespace NEO
