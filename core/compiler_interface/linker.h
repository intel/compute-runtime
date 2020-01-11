/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace NEO {

enum class SegmentType : uint32_t {
    Unknown,
    GlobalConstants,
    GlobalVariables,
    Instructions,
};

inline const char *asString(SegmentType segment) {
    switch (segment) {
    default:
        return "UNKOWN";
    case SegmentType::GlobalConstants:
        return "GLOBAL_CONSTANTS";
    case SegmentType::GlobalVariables:
        return "GLOBAL_VARIABLES";
    case SegmentType::Instructions:
        return "INSTRUCTIONS";
    }
}

struct SymbolInfo {
    uint32_t offset = std::numeric_limits<uint32_t>::max();
    uint32_t size = std::numeric_limits<uint32_t>::max();
    SegmentType segment = SegmentType::Unknown;
};

struct LinkerInput {
    union Traits {
        enum PointerSize : uint8_t {
            Ptr32bit = 0,
            Ptr64bit = 1
        };
        Traits() : packed(0) {
            pointerSize = (sizeof(void *) == 4) ? PointerSize::Ptr32bit : PointerSize::Ptr64bit;
        }
        struct {
            bool exportsGlobalVariables : 1;
            bool exportsGlobalConstants : 1;
            bool exportsFunctions : 1;
            bool requiresPatchingOfInstructionSegments : 1;
            bool requiresPatchingOfGlobalVariablesBuffer : 1;
            bool requiresPatchingOfGlobalConstantsBuffer : 1;
            uint8_t pointerSize : 1;
        };
        uint32_t packed;
    };
    static_assert(sizeof(Traits) == sizeof(Traits::packed), "");

    struct RelocationInfo {
        enum class Type : uint32_t {
            Unknown,
            Address,
            AddressHigh,
            AddressLow
        };

        std::string symbolName;
        uint64_t offset = std::numeric_limits<uint64_t>::max();
        Type type = Type::Unknown;
        SegmentType relocationSegment = SegmentType::Unknown;
        SegmentType symbolSegment = SegmentType::Unknown;
    };

    using Relocations = std::vector<RelocationInfo>;
    using SymbolMap = std::unordered_map<std::string, SymbolInfo>;
    using RelocationsPerInstSegment = std::vector<Relocations>;

    virtual ~LinkerInput() = default;

    MOCKABLE_VIRTUAL bool decodeGlobalVariablesSymbolTable(const void *data, uint32_t numEntries);
    MOCKABLE_VIRTUAL bool decodeExportedFunctionsSymbolTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    MOCKABLE_VIRTUAL bool decodeRelocationTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    void addDataRelocationInfo(const RelocationInfo &relocationInfo);

    const Traits &getTraits() const {
        return traits;
    }

    int32_t getExportedFunctionsSegmentId() const {
        return exportedFunctionsSegmentId;
    }

    const SymbolMap &getSymbols() const {
        return symbols;
    }

    const RelocationsPerInstSegment &getRelocationsInInstructionSegments() const {
        return relocations;
    }

    const Relocations &getDataRelocations() const {
        return dataRelocations;
    }

    void setPointerSize(Traits::PointerSize pointerSize) {
        traits.pointerSize = pointerSize;
    }

    bool isValid() const {
        return valid;
    }

  protected:
    Traits traits;
    SymbolMap symbols;
    RelocationsPerInstSegment relocations;
    Relocations dataRelocations;
    int32_t exportedFunctionsSegmentId = -1;
    bool valid = true;
};

struct Linker {
    using RelocationInfo = LinkerInput::RelocationInfo;

    struct SegmentInfo {
        uintptr_t gpuAddress = std::numeric_limits<uintptr_t>::max();
        size_t segmentSize = std::numeric_limits<size_t>::max();
    };

    struct PatchableSegment {
        void *hostPointer = nullptr;
        size_t segmentSize = std::numeric_limits<size_t>::max();
    };

    struct UnresolvedExternal {
        RelocationInfo unresolvedRelocation;
        uint32_t instructionsSegmentId = std::numeric_limits<uint32_t>::max();
        bool internalError = false;
    };

    struct RelocatedSymbol {
        SymbolInfo symbol;
        uintptr_t gpuAddress = std::numeric_limits<uintptr_t>::max();
    };

    using RelocatedSymbolsMap = std::unordered_map<std::string, RelocatedSymbol>;
    using PatchableSegments = std::vector<PatchableSegment>;
    using UnresolvedExternals = std::vector<UnresolvedExternal>;

    Linker(const LinkerInput &data)
        : data(data) {
    }

    bool link(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo, const SegmentInfo &exportedFunctionsSegInfo,
              PatchableSegment &globalVariablesSeg, PatchableSegment &globalConstantsSeg, const PatchableSegments &instructionsSegments,
              UnresolvedExternals &outUnresolvedExternals) {
        bool success = data.isValid();
        success = success && processRelocations(globalVariablesSegInfo, globalConstantsSegInfo, exportedFunctionsSegInfo);
        success = success && patchInstructionsSegments(instructionsSegments, outUnresolvedExternals);
        success = success && patchDataSegments(globalVariablesSegInfo, globalConstantsSegInfo, globalVariablesSeg, globalConstantsSeg, outUnresolvedExternals);

        return success;
    }

    RelocatedSymbolsMap extractRelocatedSymbols() {
        return RelocatedSymbolsMap(std::move(relocatedSymbols));
    }

  protected:
    const LinkerInput &data;
    RelocatedSymbolsMap relocatedSymbols;

    bool processRelocations(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions);

    bool patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals);

    bool patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                           PatchableSegment &globalVariablesSeg, PatchableSegment &globalConstantsSeg,
                           std::vector<UnresolvedExternal> &outUnresolvedExternals);
};

std::string constructLinkerErrorMessage(const Linker::UnresolvedExternals &unresolvedExternals, const std::vector<std::string> &instructionsSegmentsNames);

} // namespace NEO
