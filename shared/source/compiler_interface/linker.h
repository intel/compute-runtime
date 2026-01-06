/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstdint>
#include <limits>
#include <optional>
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
class SharedPoolAllocation;

enum class SegmentType : uint32_t {
    unknown,
    globalConstants,
    globalConstantsZeroInit,
    globalStrings,
    globalVariables,
    globalVariablesZeroInit,
    instructions,
};

enum class LinkingStatus : uint32_t {
    error,
    linkedFully,
    linkedPartially
};

inline const char *asString(SegmentType segment) {
    switch (segment) {
    default:
        return "UNKNOWN";
    case SegmentType::globalConstants:
        return "GLOBAL_CONSTANTS";
    case SegmentType::globalVariables:
        return "GLOBAL_VARIABLES";
    case SegmentType::instructions:
        return "INSTRUCTIONS";
    }
}

struct SymbolInfo {
    uint64_t offset = std::numeric_limits<uint64_t>::max();
    uint64_t size = std::numeric_limits<uint64_t>::max();
    SegmentType segment = SegmentType::unknown;
    uint32_t instructionSegmentId = std::numeric_limits<uint32_t>::max(); // set if segment type is instructions
    bool global = false;                                                  // Binding
};

struct LinkerInput : NEO::NonCopyableAndNonMovableClass {
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
            unknown,
            address,
            addressLow,
            addressHigh,
            perThreadPayloadOffset,
            address16 = 7,
            relocTypeMax
        };

        std::string symbolName;
        uint64_t offset = std::numeric_limits<uint64_t>::max();
        Type type = Type::unknown;
        SegmentType relocationSegment = SegmentType::unknown;
        std::string relocationSegmentName;
        int64_t addend = 0U;
    };

    using SectionNameToSegmentIdMap = std::unordered_map<std::string, uint32_t>;
    using Relocations = std::vector<RelocationInfo>;
    using SymbolMap = std::unordered_map<std::string, SymbolInfo>;
    using RelocationsPerInstSegment = std::vector<Relocations>;

    LinkerInput();
    virtual ~LinkerInput();

    static SegmentType getSegmentForSection(ConstStringRef name);

    MOCKABLE_VIRTUAL bool decodeGlobalVariablesSymbolTable(const void *data, uint32_t numEntries);
    MOCKABLE_VIRTUAL bool decodeExportedFunctionsSymbolTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    MOCKABLE_VIRTUAL bool decodeRelocationTable(const void *data, uint32_t numEntries, uint32_t instructionsSegmentId);
    void addDataRelocationInfo(const RelocationInfo &relocationInfo);

    void addElfTextSegmentRelocation(RelocationInfo relocationInfo, uint32_t instructionsSegmentId);

    template <Elf::ElfIdentifierClass numBits>
    void decodeElfSymbolTableAndRelocations(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId);

    template <Elf::ElfIdentifierClass numBits>
    bool addSymbol(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, size_t symId);

    template <Elf::ElfIdentifierClass numBits>
    bool addRelocation(Elf::Elf<numBits> &elf, const SectionNameToSegmentIdMap &nameToSegmentId, const typename Elf::Elf<numBits>::RelocationInfo &relocation);

    std::optional<uint32_t> getInstructionSegmentId(const SectionNameToSegmentIdMap &kernelNameToSegId, const std::string &kernelName);

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
    std::vector<std::string> externalSymbols;
    std::vector<std::pair<std::string, SymbolInfo>> extFuncSymbols;
    Relocations dataRelocations;
    RelocationsPerInstSegment textRelocations;
    std::vector<ExternalFunctionUsageKernel> kernelDependencies;
    std::vector<ExternalFunctionUsageExtFunc> extFunDependencies;
    int32_t exportedFunctionsSegmentId = -1;
    bool valid = true;
};

struct Linker {
    inline static constexpr std::string_view subDeviceID = "__SubDeviceID";
    inline static constexpr std::string_view perThreadOff = "__INTEL_PER_THREAD_OFF";

    using RelocationInfo = LinkerInput::RelocationInfo;

    struct SegmentInfo {
        uint64_t gpuAddress = std::numeric_limits<uint64_t>::max();
        size_t segmentSize = 0u;
    };

    struct PatchableSegment {
        void *hostPointer = nullptr;
        uint64_t gpuAddress = 0;
        size_t segmentSize = std::numeric_limits<size_t>::max();
    };

    struct UnresolvedExternal {
        RelocationInfo unresolvedRelocation;
        uint32_t instructionsSegmentId = std::numeric_limits<uint32_t>::max();
        bool internalError = false;
    };

    template <typename T>
    struct RelocatedSymbol {
        T symbol;
        uint64_t gpuAddress = std::numeric_limits<uint64_t>::max();
    };

    using RelocatedSymbolsMap = std::unordered_map<std::string, RelocatedSymbol<SymbolInfo>>;
    using PatchableSegments = std::vector<PatchableSegment>;
    using UnresolvedExternals = std::vector<UnresolvedExternal>;
    using KernelDescriptorsT = std::vector<KernelDescriptor *>;
    using ExternalFunctionsT = std::vector<ExternalFunctionInfo>;

    Linker(const LinkerInput &data)
        : Linker(data, true) {
    }

    Linker(const LinkerInput &data, bool userModule)
        : data(data), userModule(userModule) {
    }

    LinkingStatus link(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo, const SegmentInfo &exportedFunctionsSegInfo,
                       const SegmentInfo &globalStringsSegInfo, SharedPoolAllocation *globalVariablesSeg, SharedPoolAllocation *globalConstantsSeg,
                       const PatchableSegments &instructionsSegments, UnresolvedExternals &outUnresolvedExternals, Device *pDevice, const void *constantsInitData,
                       size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize, const KernelDescriptorsT &kernelDescriptors,
                       ExternalFunctionsT &externalFunctions);

    static void patchAddress(void *relocAddress, const uint64_t value, const RelocationInfo &relocation);
    void removeLocalSymbolsFromRelocatedSymbols();
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

    bool relocateSymbols(const SegmentInfo &globalVariables, const SegmentInfo &globalConstants, const SegmentInfo &exportedFunctions, const SegmentInfo &globalStrings, const PatchableSegments &instructionsSegments, size_t globalConstantsInitDataSize, size_t globalVariablesInitDataSize);

    void patchInstructionsSegments(const std::vector<PatchableSegment> &instructionsSegments, std::vector<UnresolvedExternal> &outUnresolvedExternals, const KernelDescriptorsT &kernelDescriptors);

    void patchDataSegments(const SegmentInfo &globalVariablesSegInfo, const SegmentInfo &globalConstantsSegInfo,
                           SharedPoolAllocation *globalVariablesSeg, SharedPoolAllocation *globalConstantsSeg,
                           std::vector<UnresolvedExternal> &outUnresolvedExternals, Device *pDevice,
                           const void *constantsInitData, size_t constantsInitDataSize, const void *variablesInitData, size_t variablesInitDataSize);

    bool resolveExternalFunctions(const KernelDescriptorsT &kernelDescriptors, std::vector<ExternalFunctionInfo> &externalFunctions);
    void resolveImplicitArgs(const KernelDescriptorsT &kernelDescriptors, Device *pDevice);
    void resolveBuiltins(Device *pDevice, UnresolvedExternals &outUnresolvedExternals, const std::vector<PatchableSegment> &instructionsSegments, const KernelDescriptorsT &kernelDescriptors);

    template <typename PatchSizeT>
    void patchIncrement(void *dstAllocation, size_t relocationOffset, const void *initData, uint64_t incrementValue);

    /* <ISA segment id> to <implicit args relocation address to patch, relocation type> */
    std::unordered_map<uint32_t, StackVec<std::pair<void *, RelocationInfo::Type>, 2>> pImplicitArgsRelocationAddresses;
    bool userModule = true;
};

static_assert(NEO::NonCopyableAndNonMovable<LinkerInput>);

std::string constructLinkerErrorMessage(const Linker::UnresolvedExternals &unresolvedExternals, const std::vector<std::string> &instructionsSegmentsNames);
std::string constructRelocationsDebugMessage(const Linker::RelocatedSymbolsMap &relocatedSymbols);

inline bool isVarDataSegment(const SegmentType &segment) {
    return segment == NEO::SegmentType::globalVariables || segment == NEO::SegmentType::globalVariablesZeroInit;
}
inline bool isConstDataSegment(const SegmentType &segment) {
    return segment == NEO::SegmentType::globalConstants || segment == NEO::SegmentType::globalConstantsZeroInit;
}
inline bool isDataSegment(const SegmentType &segment) {
    return isConstDataSegment(segment) || isVarDataSegment(segment);
}

} // namespace NEO
