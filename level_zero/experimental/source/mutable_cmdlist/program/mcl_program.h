/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/utilities/const_stringref.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace L0::MCL::Program {
using ElfSymbol = NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64>;
using ElfRela = NEO::Elf::ElfRela<NEO::Elf::EI_CLASS_64>;
namespace Sections {

enum class SectionType : uint32_t {
    shtUndef = 0,
    shtSymtab = 2,
    shtStrtab = 3,
    shtRela = 4,

    shtMclStart = 0xFEED0000,
    shtCs = 0xFEED0001,
    shtIh = 0xFEED0002,
    shtIoh = 0xFEED0003,
    shtSsh = 0xFEED0004,
    shtDataConst = 0xFEED0005,
    shtDataVar = 0xFEED0006,
    shtDataStrings = 0xFEED0007,
    shtDataTmp = 0xFEED0008,
    shtMclEnd = 0xFEED000F,
};

namespace SectionNames {
using ConstStringRef = NEO::ConstStringRef;
static ConstStringRef csSectionName = ".text";
static ConstStringRef ihSectionName = ".text.eu";
static ConstStringRef iohSectionName = ".heap.ioh";
static ConstStringRef dshSectionName = ".heap.dsh";
static ConstStringRef sshSectionName = ".heap.ssh";
static ConstStringRef tmpSectionName = ".data.tmp";
static ConstStringRef relaPrefix = ".rela";
static ConstStringRef symtabName = ".symtab";
static ConstStringRef strtabName = ".strtab";
// -------ZEBIN--------------
static ConstStringRef dataConstSectionName = ".data.const";
static ConstStringRef dataVarSectionName = ".data.var";
static ConstStringRef dataStringSectionName = ".data.string";
// ---------------------------
}; // namespace SectionNames

} // namespace Sections

namespace Symbols {
enum class SymbolType : uint8_t {
    notype = 0,
    object = 1,
    funct = 2,
    section = 3,
    file = 4,
    common = 5,
    base = 7,
    var = 8,
    dispatch = 11,
    kernel = 12,
    ioh = 13,
    heap = 14,
    function = 15,
    symbolTypeMax = 0xF
};

struct KernelSymbol {
    static constexpr uint8_t indirectOffsetBitShift = 3;
    KernelSymbol() = default;
    KernelSymbol(uint64_t symbolValue) : data(symbolValue) {}
    KernelSymbol(uint32_t kernelDataId, uint16_t skipPerThreadDataLoad, uint8_t simdSize, uint8_t passInlineData, uint8_t indirectOffset)
        : kernelDataId(kernelDataId), skipPerThreadDataLoad(skipPerThreadDataLoad), simdSize(simdSize),
          passInlineData(passInlineData), indirectOffset(indirectOffset) {
        reserve = 0;
    };

    union {
        struct {
            uint32_t kernelDataId;
            uint16_t skipPerThreadDataLoad;
            uint8_t simdSize;
            union {
                struct {
                    uint8_t passInlineData : 1;
                    uint8_t indirectOffset : 4;
                    uint8_t reserve : 3;
                };
                uint8_t flags;
            };
        };
        const uint64_t data = 0U;
    };
};
static_assert(sizeof(KernelSymbol) == sizeof(uint64_t));

struct DispatchSymbolValue {
    DispatchSymbolValue() = default;
    DispatchSymbolValue(uint64_t symbolValue) : data(symbolValue) {}

    union {
        struct {
            uint16_t dispatchId;
            uint16_t kernelDataId;
            uint16_t groupSizeVarId;
            uint16_t groupCountVarId;
        };
        const uint64_t data = 0U;
    };
};
static_assert(sizeof(DispatchSymbolValue) == sizeof(uint64_t));

struct VariableSymbolValue {
    VariableSymbolValue() = default;
    VariableSymbolValue(uint64_t symbolValue) : data(symbolValue) {}

    union {
        struct {
            uint32_t id;
            uint16_t reserved;
            uint8_t variableType;
            union {
                struct {
                    bool isTmp : 1;
                    bool isScalable : 1; // false - const / true - scalable
                } flags;
                uint8_t flagsAsUint8;
            };
        };
        const uint64_t data = 0U;
    };
};
static_assert(sizeof(VariableSymbolValue) == sizeof(uint64_t));

namespace SymbolNames {
using ConstStringRef = NEO::ConstStringRef;
static ConstStringRef gpuBaseSymbol = ".gpu.base";
static ConstStringRef ihBaseSymbol = ".text.eu.base";
static ConstStringRef iohBaseSymbol = ".heap.ioh.base";
static ConstStringRef iohPrefix = ".heap.ioh.";
static ConstStringRef sshPrefix = ".heap.ssh.";
static ConstStringRef bindingTablePrefix = ".binding.table.";
static ConstStringRef dshBaseSymbol = ".heap.dsh.base";
static ConstStringRef sshBaseSymbol = ".heap.ssh.base";
static ConstStringRef varPrefix = "var.";
static ConstStringRef dispatchPrefix = "dispatch.";
static ConstStringRef tmpPrefix = "tmp.";
static ConstStringRef kernelPrefix = ".text.eu.";
static ConstStringRef kernelDataPrefix = ".kernel.";
static ConstStringRef functionPrefix = ".text.eu.func.";
} // namespace SymbolNames

struct Symbol {
    Symbol() = default;
    Symbol(const std::string &name, SymbolType type, Sections::SectionType section, uint64_t value, size_t size)
        : name(name), type(type), section(section), value(value), size(size){};
    std::string name;
    SymbolType type;
    Sections::SectionType section;
    uint64_t value;
    size_t size;
};
}; // namespace Symbols

namespace Relocations {

enum RelocType : uint32_t {
    address = 1,
    address32 = 2,
    address32Hi = 3,

    sba = 19U,

    // ---KERNEL DISPATCH-----
    crossThreadDataStart,
    perThreadDataStart,
    surfaceStateHeapStart,
    walkerCommandStart,
    kernelStart,
    bindingTable,
    // -----------------------

    // ---DISPATCH TRAITS-----
    walkerCommand,
    localWorkSize,
    localWorkSize2,
    enqLocalWorkSize,
    globalWorkSize,
    numWorkGroups,
    workDimensions,
    numThreadsPerTg,
    // -----------------------

    // ---BUFFER VAR----------
    bufferStateless,
    bufferOffset,
    bufferBindful,
    bufferBindfulWithoutOffset,
    bufferBindless,
    bufferBindlessWithoutOffset,
    bufferAddress,
    // -----------------------

    // ---VALUE VAR-----------
    valueStateless,
    valueCmdBuffer,
    valueStatelessSize,
    valueCmdBufferSize
    // -----------------------
};
static_assert(sizeof(RelocType) == sizeof(uint32_t));

struct Relocation {
    Relocation() = delete;
    Relocation(Sections::SectionType section, size_t symbolId, RelocType type, uint64_t offset, int64_t addend = 0)
        : symbolId(symbolId), offset(offset), addend(addend), type(type), section(section){};

    size_t symbolId;
    uint64_t offset;
    int64_t addend;
    RelocType type;
    Sections::SectionType section;
};
} // namespace Relocations

struct MclProgram {
    using HeapT = std::vector<uint8_t>;
    using SymMapT = std::unordered_map<std::string, size_t>;

    using Symbols = std::vector<Symbols::Symbol>;
    using Relocations = std::unordered_map<Sections::SectionType, std::vector<Relocations::Relocation>>;

    HeapT cs;
    HeapT ih;
    HeapT ioh;
    HeapT ssh;

    // -------ZEBIN--------------
    HeapT consts;
    HeapT vars;
    HeapT strings;
    // ---------------------------

    Symbols symbols;
    Relocations relocs;
};
} // namespace L0::MCL::Program
