/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace NEO {
struct ExternalFunctionInfo;
struct ExternalFunctionUsageExtFunc;
struct ExternalFunctionUsageKernel;

class Device;
class GraphicsAllocation;
struct KernelDescriptor;
struct ProgramInfo;

enum class SegmentType : uint32_t {
    Unknown,
    GlobalConstants,
    GlobalStrings,
    GlobalVariables,
    Instructions,
};

enum class LinkingStatus : uint32_t {
    Error,
    LinkedFully,
    LinkedPartially
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

struct LocalFuncSymbolInfo {
    uint32_t offset = std::numeric_limits<uint32_t>::max();
    uint32_t size = std::numeric_limits<uint32_t>::max();
    std::string targetedKernelSectionName;
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
            AddressLow,
            AddressHigh,
            PerThreadPayloadOffset,
            RelocTypeMax
        };

        std::string symbolName;
        uint64_t offset = std::numeric_limits<uint64_t>::max();
        Type type = Type::Unknown;
        SegmentType relocationSegment = SegmentType::Unknown;
        int64_t addend = 0U;
    };

    using SectionNameToSegmentIdMap = std::unordered_map<std::string, uint32_t>;
    using Relocations = std::vector<RelocationInfo>;
    using SymbolMap = std::unordered_map<std::string, SymbolInfo>;
    using LocalSymbolMap = std::unordered_map<std::string, LocalFuncSymbolInfo>;
    using RelocationsPerInstSegment = std::vector<Relocations>;

    virtual ~LinkerInput() = default;

    static SegmentType getSegmentForSection(ConstStringRef name);

    MOCKABLE_VIRTUAL bool decodeGlobalVariablesSymbolTable(const void *data, uint32_t numEntries);
    MOCKABLE_VIRTUAL bool decodeExportedFunctionsSymbolTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    MOCKABLE_VIRTUAL bool decodeRelocationTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    void addDataRelocationInfo(const RelocationInfo &relocationInfo);

    void addElfTextSegmentRelocation(RelocationInfo relocationInfo, uint32_t instructionsSegmentId);

    template <Elf::ELF_IDENTIFIER_CLASS numBits>
    void decodeElfSymbolTableAndRelocations(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);

    const Traits &getTraits() const {
        return traits;
    }

    int32_t getExportedFunctionsSegmentId() const {
        return exportedFunctionsSegmentId;
    }

    void setExportedFunctionsSegmentId(int32_t functionsSegmentId) {
        exportedFunctionsSegmentId = functionsSegmentId;
    }

    const SymbolMap &getSymbols() const {
        return symbols;
    }

    const LocalSymbolMap &getLocalSymbols() const {
        return localSymbols;
    }

    void addSymbol(const std::string &symbolName, const SymbolInfo &symbolInfo) {
        symbols.emplace(std::make_pair(symbolName, symbolInfo));
    }

    const RelocationsPerInstSegment &getRelocationsInInstructionSegments() const {
        return textRelocations;
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

    const std::vector<ExternalFunctionUsageKernel> &getKernelDependencies() const {
        return kernelDependencies;
    }

    const std::vector<ExternalFunctionUsageExtFunc> &getFunctionDependencies() const {
        return extFunDependencies;
    }

  protected:
    void parseRelocationForExtFuncUsage(const RelocationInfo &relocInfo, const std::string &kernelName);

    Traits traits;
    SymbolMap symbols;
    LocalSymbolMap localSymbols;
    RelocationsPerInstSegment textRelocations;
    Relocations dataRelocations;
    std::vector<std::pair<std::string, SymbolInfo>> extFuncSymbols;
    int32_t exportedFunctionsSegmentId = -1;
    std::vector<ExternalFunctionUsageKernel> kernelDependencies;
    std::vector<ExternalFunctionUsageExtFunc> extFunDependencies;
    bool valid = true;
};

struct Linker {
    inline static const std::string subDeviceID = "__SubDeviceID";

    using RelocationInfo = LinkerInput::RelocationInfo;

    struct SegmentInfo {
        uintptr_t gpuAddress = std::numeric_limits<uintptr_t>::max();
        size_t segmentSize = std::numeric_limits<size_t>::max();
    };

    struct PatchableSegment {
        void *hostPointer = nullptr;
        uintptr_t gpuAddress = 0;
        size_t segmentSize = std::numeric_limits<size_t>::max();
        std::string kernelName;
    };

    struct UnresolvedExternal {
        RelocationInfo unresolvedRelocation;
        uint32_t instructionsSegmentId = std::numeric_limits<uint32_t>::max();
        bool internalError = false;
    };

    template <typename T>
    struct RelocatedSymbol {
        T symbol;
        uintptr_t gpuAddress = std::numeric_limits<uintptr_t>::max();
    };

    using RelocatedSymbolsMap = std::unordered_map<std::string, RelocatedSymbol<SymbolInfo>>;
    using LocalsRelocatedSymbolsMap = std::unordered_map<std::string, RelocatedSymbol<LocalFuncSymbolInfo>>;
    using PatchableSegments = std::vector<PatchableSegment>;
    using UnresolvedExternals = std::vector<UnresolvedExternal>;
    using KernelDescriptorsT = std::vector<KernelDescriptor *>;
    using ExternalFunctionsT = std::vector<ExternalFunctionInfo>;

    Linker(const LinkerInput &data)
        : data(data) {
    }

    LinkingStatus link(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo, const SegmentInfo &exportedFunctionsSegInfo,
                       const SegmentInfo &globalStringsSegInfo, GraphicsAllocation *globalVariablesSeg, GraphicsAllocation *globalConstantsSeg,
                       const PatchableSegments &instructionsSegments, UnresolvedExternals &outUnresolvedExternals, Device *pDevice, const void *constantsInitData,
                       size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize, const KernelDescriptorsT &kernelDescriptors,
                       ExternalFunctionsT &externalFunctions);

    static void patchAddress(void *relocAddress, const uint64_t value, const RelocationInfo &relocation);
    RelocatedSymbolsMap extractRelocatedSymbols() {
        return RelocatedSymbolsMap(std::move(relocatedSymbols));
    }

    static void applyDebugDataRelocations(const NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> &decodedElf, ArrayRef<uint8_t> inputOutputElf,
                                          const SegmentInfo &text,
                                          const SegmentInfo &globalData,
                                          const SegmentInfo &constData);

  protected:
    const LinkerInput &data;
    RelocatedSymbolsMap relocatedSymbols;
    LocalsRelocatedSymbolsMap localRelocatedSymbols;

    bool processRelocations(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions, const SegmentInfo &globalStrings, const PatchableSegments &instructionsSegments);

    void patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals, const KernelDescriptorsT &kernelDescriptors);

    void patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                           GraphicsAllocation *globalVariablesSeg, GraphicsAllocation *globalConstantsSeg,
                           std::vector<UnresolvedExternal> &outUnresolvedExternals, Device *pDevice,
                           const void *constantsInitData, size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize);

    bool resolveExternalFunctions(const KernelDescriptorsT &kernelDescriptors, std::vector<ExternalFunctionInfo> &externalFunctions);
    void resolveImplicitArgs(const KernelDescriptorsT &kernelDescriptors, Device *pDevice);
    void resolveBuiltins(Device *pDevice, UnresolvedExternals &outUnresolvedExternals, const std::vector<PatchableSegment> &instructionsSegments);

    template <typename PatchSizeT>
    void patchIncrement(void *dstAllocation, size_t relocationOffset, const void *initData, uint64_t incrementValue);

    std::unordered_map<uint32_t /*ISA segment id*/, StackVec<uint32_t *, 2> /*implicit args relocation address to patch*/> pImplicitArgsRelocationAddresses;
};

std::string constructLinkerErrorMessage(const Linker::UnresolvedExternals &unresolvedExternals, const std::vector<std::string> &instructionsSegmentsNames);
std::string constructRelocationsDebugMessage(const Linker::RelocatedSymbolsMap &relocatedSymbols);
inline bool isDataSegment(const SegmentType &segment) {
    return segment == SegmentType::GlobalConstants || segment == SegmentType::GlobalVariables;
}

} // namespace NEO
