/*
 * Copyright (C) 2017-2019 Intel Corporation
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

struct SymbolInfo {
    enum Type : uint32_t {
        Unknown,
        GlobalConstant,
        GlobalVariable,
        Function
    };
    uint32_t offset = std::numeric_limits<uint32_t>::max();
    uint32_t size = std::numeric_limits<uint32_t>::max();
    Type type = Unknown;
};

struct LinkerInput {
    union Traits {
        Traits() : packed(0) {
        }
        struct {
            bool exportsGlobalVariables : 1;
            bool exportsGlobalConstants : 1;
            bool exportsFunctions : 1;
            bool requiresPatchingOfInstructionSegments : 1;
        };
        uint32_t packed;
    };
    static_assert(sizeof(Traits) == sizeof(Traits::packed), "");

    struct RelocationInfo {
        std::string symbolName;
        uint32_t offset = std::numeric_limits<uint32_t>::max();
    };

    using Relocations = std::vector<RelocationInfo>;
    using SymbolMap = std::unordered_map<std::string, SymbolInfo>;
    using RelocationsPerInstSegment = std::vector<Relocations>;

    bool decodeGlobalVariablesSymbolTable(const void *data, uint32_t numEntries);
    bool decodeExportedFunctionsSymbolTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    bool decodeRelocationTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);

    const Traits &getTraits() const {
        return traits;
    }

    int32_t getExportedFunctionsSegmentId() const {
        return exportedFunctionsSegmentId;
    }

    const SymbolMap &getSymbols() const {
        return symbols;
    }

    const RelocationsPerInstSegment &getRelocations() const {
        return relocations;
    }

  protected:
    Traits traits;
    SymbolMap symbols;
    RelocationsPerInstSegment relocations;
    int32_t exportedFunctionsSegmentId = -1;
};

struct Linker {
    using RelocationInfo = LinkerInput::RelocationInfo;

    struct Segment {
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

    bool link(const Segment &globalVariables, const Segment &globalConstants, const Segment &exportedFunctions,
              const PatchableSegments &instructionsSegments,
              UnresolvedExternals &outUnresolvedExternals) {
        return processRelocations(globalVariables, globalConstants, exportedFunctions) && patchInstructionsSegments(instructionsSegments, outUnresolvedExternals);
    }

    RelocatedSymbolsMap extractRelocatedSymbols() {
        return RelocatedSymbolsMap(std::move(relocatedSymbols));
    }

  protected:
    const LinkerInput &data;
    RelocatedSymbolsMap relocatedSymbols;

    bool processRelocations(const Segment &globalVariables, const Segment &globalConstants, const Segment &exportedFunctions);

    bool patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals);
};

std::string constructLinkerErrorMessage(const Linker::UnresolvedExternals &unresolvedExternals, const std::vector<std::string> &instructionsSegmentsNames);

} // namespace NEO
