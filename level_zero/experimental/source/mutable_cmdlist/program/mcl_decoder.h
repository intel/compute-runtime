/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_imp.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_kernel_dispatch.h"
#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_program.h"

#include <memory>

namespace NEO {
class Device;
class CommandContainer;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;

namespace MCL {
struct MclAllocations;
struct KernelData;
struct KernelDispatchOffsets;
struct Variable;
enum VariableType : uint8_t;
struct BufferUsages;
struct ValueUsages;
} // namespace MCL

} // namespace L0

namespace L0::MCL::Program::Decoder {

struct MclDecoderArgs {
    ArrayRef<const uint8_t> binary;
    std::vector<std::unique_ptr<Variable>> *variableStorage;
    std::unordered_map<std::string, Variable *> *variableMap;
    std::vector<Variable *> *tempVariables;
    std::vector<std::unique_ptr<KernelData>> *kernelDatas;
    std::vector<std::unique_ptr<KernelDispatch>> *kernelDispatchs;
    std::vector<StateBaseAddressOffsets> *sbaVec;
    std::vector<std::unique_ptr<MutableComputeWalker>> *mutableWalkerCmds;
    L0::Device *device;
    NEO::CommandContainer *cmdContainer;
    MclAllocations *allocs;
    MutableCommandList *mcl;
    NEO::EngineGroupType cmdListEngine;
    uint32_t partitionCount = 0;
    bool heapless = false;
    bool localDispatch = false;
};

struct VarInfo {
    std::string name;
    size_t size;
    bool tmp : 1;
    bool scalable : 1;
    VariableType type;
    BufferUsages bufferUsages;
    ValueUsages valueUsages;
};

struct KernelDispatchInfo {
    size_t kernelDataId = undefined<size_t>;
    size_t groupSizeVarId = undefined<size_t>;
    size_t groupCountVarId = undefined<size_t>;

    MutableIndirectData::Offsets indirectDataOffsets;
    KernelDispatchOffsets dispatchOffsets;
    size_t iohSize;
    size_t sshSize;
};

class MclDecoder {
    struct Segment {
        ArrayRef<const uint8_t> initData;
        GpuAddress address;
        CpuAddress cpuPtr;
    };
    struct Program {
        using ElfRelocsT = std::vector<std::pair<Sections::SectionType, ArrayRef<const ElfRela>>>;
        using ElfSectionHeader = NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64>;
        struct Segments {
            Segment cs;
            Segment ih;
            Segment ioh;
            Segment ssh;
            Segment consts;
            Segment vars;
            Segment strings;
        } segments;
        ArrayRef<const char> elfStrTabData;
        ArrayRef<const ElfSymbol> elfSymbolData;
        ArrayRef<const ElfSectionHeader> elfSh;
        std::vector<Symbols::Symbol> symbols;
        ElfRelocsT elfRelocs;
    };

  public:
    static void decode(const MclDecoderArgs &args);
    MclDecoder() = default;
    bool decodeElf(ArrayRef<const uint8_t> mclElfBinary);

    using AllocPtrT = std::unique_ptr<NEO::GraphicsAllocation>;
    void createRequiredAllocations(NEO::Device *device, NEO::CommandContainer &cmdCtr,
                                   AllocPtrT &outIHAlloc, AllocPtrT &outConstsAlloc, AllocPtrT &outVarsAlloc,
                                   std::unique_ptr<char[]> &outStringsAlloc, bool heapless);
    void parseSymbols();
    void parseRelocations();

    inline std::vector<VarInfo> &getVarInfo() { return varInfos; }
    inline std::vector<KernelData> &getKernelData() { return kernelDataVec; }
    inline std::vector<KernelDispatchInfo> &getKernelDispatchInfos() { return dispatchInfos; }
    inline StateBaseAddressOffsets getSbaOffsets() { return sba; }
    const Program::Segments &getSegments() { return program.segments; }

  protected:
    Program program;
    std::vector<KernelData> kernelDataVec;
    std::vector<KernelDispatchInfo> dispatchInfos;
    std::vector<VarInfo> varInfos;
    struct {
        GpuAddress generalState;
        GpuAddress instruction;
        GpuAddress indirectObject;
        GpuAddress surfaceState;
    } baseAddresses;
    StateBaseAddressOffsets sba;

    Segment *getSegmentBySecType(Sections::SectionType type);

    inline NEO::ConstStringRef getSymName(uint64_t offset) { return NEO::ConstStringRef(program.elfStrTabData.begin() + offset); }
    void relocateSymbol(Symbols::Symbol &symbol);
    void parseBaseSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol);
    void parseVarSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol);
    void parseDispatchSymbol(NEO::ConstStringRef symbolName, Symbols::Symbol &symbol);

    void parseBaseRelocation(const Symbols::Symbol &symbol, const ElfRela &reloc);
    void parseDefaultRelocation(Sections::SectionType targetSecType, uint64_t value, const ElfRela &reloc);
    void parseDispatchRelocation(Sections::SectionType targetSecType, KernelDispatchInfo &di, const ElfRela &reloc);
    void parseVarRelocation(Sections::SectionType targetSecType, VarInfo &varInfo, const ElfRela &reloc);
};
} // namespace L0::MCL::Program::Decoder
