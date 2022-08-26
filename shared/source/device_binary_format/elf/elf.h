/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include <inttypes.h>
#include <stddef.h>

namespace NEO {

namespace Elf {

// Elf identifier class
enum ELF_IDENTIFIER_CLASS : uint8_t {
    EI_CLASS_NONE = 0, // undefined
    EI_CLASS_32 = 1,   // 32-bit elf file
    EI_CLASS_64 = 2,   // 64-bit elf file
};

// Elf identifier data
enum ELF_IDENTIFIER_DATA : uint8_t {
    EI_DATA_NONE = 0,          // undefined
    EI_DATA_LITTLE_ENDIAN = 1, // little-endian
    EI_DATA_BIG_ENDIAN = 2,    // big-endian
};

// Target machine
enum ELF_MACHINE : uint16_t {
    EM_NONE = 0, // No specific instruction set
    EM_INTELGT = 205,
};

// Elf version
enum ELF_VERSION_ : uint8_t {
    EV_INVALID = 0, // undefined
    EV_CURRENT = 1, // current
};

// Elf type
enum ELF_TYPE : uint16_t {
    ET_NONE = 0,                       // undefined
    ET_REL = 1,                        // relocatable
    ET_EXEC = 2,                       // executable
    ET_DYN = 3,                        // shared object
    ET_CORE = 4,                       // core file
    ET_LOPROC = 0xff00,                // start of processor-specific type
    ET_OPENCL_RESERVED_START = 0xff01, // start of Intel OCL ELF_TYPES
    ET_OPENCL_RESERVED_END = 0xff05,   // end of Intel OCL ELF_TYPES
    ET_HIPROC = 0xffff                 // end of processor-specific types
};

// Section header type
enum SECTION_HEADER_TYPE : uint32_t {
    SHT_NULL = 0,                           // inactive section header
    SHT_PROGBITS = 1,                       // program data
    SHT_SYMTAB = 2,                         // symbol table
    SHT_STRTAB = 3,                         // string table
    SHT_RELA = 4,                           // relocation entries with add
    SHT_HASH = 5,                           // symbol hash table
    SHT_DYNAMIC = 6,                        // dynamic linking info
    SHT_NOTE = 7,                           // notes
    SHT_NOBITS = 8,                         // program "no data" space (bss)
    SHT_REL = 9,                            // relocation entries (without add)
    SHT_SHLIB = 10,                         // reserved
    SHT_DYNSYM = 11,                        // dynamic linker symbol table
    SHT_INIT_ARRAY = 14,                    // array of constructors
    SHT_FINI_ARRAY = 15,                    // array of destructors
    SHT_PREINIT_ARRAY = 16,                 // aaray of pre-constructors
    SHT_GROUP = 17,                         // section group
    SHT_SYMTAB_SHNDX = 18,                  // extended section indices
    SHT_NUM = 19,                           // number of defined types
    SHT_LOOS = 0x60000000,                  // start of os-specifc
    SHT_OPENCL_RESERVED_START = 0xff000000, // start of Intel OCL SHT_TYPES
    SHT_OPENCL_RESERVED_END = 0xff00000c    // end of Intel OCL SHT_TYPES
};

enum SPECIAL_SECTION_HEADER_NUMBER : uint16_t {
    SHN_UNDEF = 0U, // undef section
};

enum SECTION_HEADER_FLAGS : uint32_t {
    SHF_NONE = 0x0,            // no flags
    SHF_WRITE = 0x1,           // writeable data
    SHF_ALLOC = 0x2,           // occupies memory during execution
    SHF_EXECINSTR = 0x4,       // executable machine instructions
    SHF_MERGE = 0x10,          // data of section can be merged
    SHF_STRINGS = 0x20,        // data of section is null-terminated strings
    SHF_INFO_LINK = 0x40,      // section's sh_info is valid index
    SHF_LINK_ORDER = 0x80,     // has ordering requirements
    SHF_OS_NONCONFORM = 0x100, // requires os-specific processing
    SHF_GROUP = 0x200,         // section is part of section group
    SHF_TLS = 0x400,           // thread-local storage
    SHF_MASKOS = 0x0ff00000,   // operating-system-specific flags
    SHF_MASKPROC = 0xf0000000, // processor-specific flags
};

enum PROGRAM_HEADER_TYPE {
    PT_NULL = 0x0,          // unused segment
    PT_LOAD = 0x1,          // loadable segment
    PT_DYNAMIC = 0x2,       // dynamic linking information
    PT_INTERP = 0x3,        // path name to invoke as an interpreter
    PT_NOTE = 0x4,          // auxiliary information
    PT_SHLIB = 0x5,         // reserved
    PT_PHDR = 0x6,          // location and of programe header table
    PT_TLS = 0x7,           // thread-local storage template
    PT_LOOS = 0x60000000,   // start os-specifc segments
    PT_HIOS = 0x6FFFFFFF,   // end of os-specific segments
    PT_LOPROC = 0x70000000, // start processor-specific segments
    PT_HIPROC = 0x7FFFFFFF  // end processor-specific segments
};

enum PROGRAM_HEADER_FLAGS : uint32_t {
    PF_NONE = 0x0,           // all access denied
    PF_X = 0x1,              // execute
    PF_W = 0x2,              // write
    PF_R = 0x4,              // read
    PF_MASKOS = 0x0ff00000,  // operating-system-specific flags
    PF_MASKPROC = 0xf0000000 // processor-specific flags

};

enum SYMBOL_TABLE_TYPE : uint32_t {
    STT_NOTYPE = 0,
    STT_OBJECT = 1,
    STT_FUNC = 2,
    STT_SECTION = 3
};

enum SYMBOL_TABLE_BIND : uint32_t {
    STB_LOCAL = 0,
    STB_GLOBAL = 1
};

constexpr const char elfMagic[4] = {0x7f, 'E', 'L', 'F'};

struct ElfFileHeaderIdentity {
    ElfFileHeaderIdentity(ELF_IDENTIFIER_CLASS classBits)
        : eClass(classBits) {
    }
    char magic[4] = {elfMagic[0], elfMagic[1], elfMagic[2], elfMagic[3]}; // should match elfMagic
    uint8_t eClass = EI_CLASS_NONE;                                       // 32- or 64-bit format
    uint8_t data = EI_DATA_LITTLE_ENDIAN;                                 // endianness
    uint8_t version = EV_CURRENT;                                         // elf file version
    uint8_t osAbi = 0U;                                                   // target system
    uint8_t abiVersion = 0U;                                              // abi
    char padding[7] = {};                                                 // pad to 16 bytes
};
static_assert(sizeof(ElfFileHeaderIdentity) == 16, "");

template <int NumBits>
struct ElfProgramHeaderTypes;

template <>
struct ElfProgramHeaderTypes<EI_CLASS_32> {
    using Type = uint32_t;
    using Flags = uint32_t;
    using Offset = uint32_t;
    using VAddr = uint32_t;
    using PAddr = uint32_t;
    using FileSz = uint32_t;
    using MemSz = uint32_t;
    using Align = uint32_t;
};

template <>
struct ElfProgramHeaderTypes<EI_CLASS_64> {
    using Type = uint32_t;
    using Flags = uint32_t;
    using Offset = uint64_t;
    using VAddr = uint64_t;
    using PAddr = uint64_t;
    using FileSz = uint64_t;
    using MemSz = uint64_t;
    using Align = uint64_t;
};

template <int NumBits>
struct ElfProgramHeader;

template <>
struct ElfProgramHeader<EI_CLASS_32> {
    ElfProgramHeaderTypes<EI_CLASS_32>::Type type = PT_NULL;   // type of segment
    ElfProgramHeaderTypes<EI_CLASS_32>::Offset offset = 0U;    // absolute offset of segment data in file
    ElfProgramHeaderTypes<EI_CLASS_32>::VAddr vAddr = 0U;      // VA of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_32>::PAddr pAddr = 0U;      // PA of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_32>::FileSz fileSz = 0U;    // size of segment in file
    ElfProgramHeaderTypes<EI_CLASS_32>::MemSz memSz = 0U;      // size of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_32>::Flags flags = PF_NONE; // segment-dependent flags
    ElfProgramHeaderTypes<EI_CLASS_32>::Align align = 1U;      // alignment
};

template <>
struct ElfProgramHeader<EI_CLASS_64> {
    ElfProgramHeaderTypes<EI_CLASS_64>::Type type = PT_NULL;   // type of segment
    ElfProgramHeaderTypes<EI_CLASS_64>::Flags flags = PF_NONE; // segment-dependent flags
    ElfProgramHeaderTypes<EI_CLASS_64>::Offset offset = 0U;    // absolute offset of segment data in file
    ElfProgramHeaderTypes<EI_CLASS_64>::VAddr vAddr = 0U;      // VA of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_64>::PAddr pAddr = 0U;      // PA of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_64>::FileSz fileSz = 0U;    // size of segment in file
    ElfProgramHeaderTypes<EI_CLASS_64>::MemSz memSz = 0U;      // size of segment in memory
    ElfProgramHeaderTypes<EI_CLASS_64>::Align align = 1U;      // alignment
};

static_assert(sizeof(ElfProgramHeader<EI_CLASS_32>) == 0x20, "");
static_assert(sizeof(ElfProgramHeader<EI_CLASS_64>) == 0x38, "");

template <int NumBits>
struct ElfSectionHeaderTypes;

template <>
struct ElfSectionHeaderTypes<EI_CLASS_32> {
    using Name = uint32_t;
    using Type = uint32_t;
    using Flags = uint32_t;
    using Addr = uint32_t;
    using Offset = uint32_t;
    using Size = uint32_t;
    using Link = uint32_t;
    using Info = uint32_t;
    using AddrAlign = uint32_t;
    using EntSize = uint32_t;
};

template <>
struct ElfSectionHeaderTypes<EI_CLASS_64> {
    using Name = uint32_t;
    using Type = uint32_t;
    using Flags = uint64_t;
    using Addr = uint64_t;
    using Offset = uint64_t;
    using Size = uint64_t;
    using Link = uint32_t;
    using Info = uint32_t;
    using AddrAlign = uint64_t;
    using EntSize = uint64_t;
};

template <int NumBits>
struct ElfSectionHeader {
    typename ElfSectionHeaderTypes<NumBits>::Name name = 0U;           // offset to string in string section names
    typename ElfSectionHeaderTypes<NumBits>::Type type = SHT_NULL;     // section type
    typename ElfSectionHeaderTypes<NumBits>::Flags flags = SHF_NONE;   // section flags
    typename ElfSectionHeaderTypes<NumBits>::Addr addr = 0U;           // VA of section in memory
    typename ElfSectionHeaderTypes<NumBits>::Offset offset = 0U;       // absolute offset of section data in file
    typename ElfSectionHeaderTypes<NumBits>::Size size = 0U;           // size of section's data
    typename ElfSectionHeaderTypes<NumBits>::Link link = SHN_UNDEF;    // index of associated section
    typename ElfSectionHeaderTypes<NumBits>::Info info = 0U;           // extra information
    typename ElfSectionHeaderTypes<NumBits>::AddrAlign addralign = 0U; // section alignment
    typename ElfSectionHeaderTypes<NumBits>::EntSize entsize = 0U;     // section's entries size
};

static_assert(sizeof(ElfSectionHeader<EI_CLASS_32>) == 0x28, "");
static_assert(sizeof(ElfSectionHeader<EI_CLASS_64>) == 0x40, "");

template <ELF_IDENTIFIER_CLASS NumBits>
struct ElfFileHeaderTypes;

template <>
struct ElfFileHeaderTypes<EI_CLASS_32> {
    using Type = uint16_t;
    using Machine = uint16_t;
    using Version = uint32_t;
    using Entry = uint32_t;
    using PhOff = uint32_t;
    using ShOff = uint32_t;
    using Flags = uint32_t;
    using EhSize = uint16_t;
    using PhEntSize = uint16_t;
    using PhNum = uint16_t;
    using ShEntSize = uint16_t;
    using ShNum = uint16_t;
    using ShStrNdx = uint16_t;
};

template <>
struct ElfFileHeaderTypes<EI_CLASS_64> {
    using Type = uint16_t;
    using Machine = uint16_t;
    using Version = uint32_t;
    using Entry = uint64_t;
    using PhOff = uint64_t;
    using ShOff = uint64_t;
    using Flags = uint32_t;
    using EhSize = uint16_t;
    using PhEntSize = uint16_t;
    using PhNum = uint16_t;
    using ShEntSize = uint16_t;
    using ShNum = uint16_t;
    using ShStrNdx = uint16_t;
};

template <ELF_IDENTIFIER_CLASS NumBits>
struct ElfFileHeader {
    ElfFileHeaderIdentity identity = ElfFileHeaderIdentity(NumBits);                               // elf file identity
    typename ElfFileHeaderTypes<NumBits>::Type type = ET_NONE;                                     // elf file type
    typename ElfFileHeaderTypes<NumBits>::Machine machine = EM_NONE;                               // target machine
    typename ElfFileHeaderTypes<NumBits>::Version version = 1U;                                    // elf file version
    typename ElfFileHeaderTypes<NumBits>::Entry entry = 0U;                                        // entry point (start address)
    typename ElfFileHeaderTypes<NumBits>::PhOff phOff = 0U;                                        // absolute offset to program header table in file
    typename ElfFileHeaderTypes<NumBits>::ShOff shOff = 0U;                                        // absolute offset to section header table in file
    typename ElfFileHeaderTypes<NumBits>::Flags flags = 0U;                                        // target-dependent flags
    typename ElfFileHeaderTypes<NumBits>::EhSize ehSize = sizeof(ElfFileHeader<NumBits>);          // header size
    typename ElfFileHeaderTypes<NumBits>::PhEntSize phEntSize = sizeof(ElfProgramHeader<NumBits>); // size of entries in program header table
    typename ElfFileHeaderTypes<NumBits>::PhNum phNum = 0U;                                        // number of entries in pogram header table
    typename ElfFileHeaderTypes<NumBits>::ShEntSize shEntSize = sizeof(ElfSectionHeader<NumBits>); // size of entries section header table
    typename ElfFileHeaderTypes<NumBits>::ShNum shNum = 0U;                                        // number of entries in section header table
    typename ElfFileHeaderTypes<NumBits>::ShStrNdx shStrNdx = SHN_UNDEF;                           // index of section header table with section names
};

static_assert(sizeof(ElfFileHeader<EI_CLASS_32>) == 0x34, "");
static_assert(sizeof(ElfFileHeader<EI_CLASS_64>) == 0x40, "");

struct ElfNoteSection {
    uint32_t nameSize;
    uint32_t descSize;
    uint32_t type;
};
static_assert(sizeof(ElfNoteSection) == 0xC, "");

template <int NumBits>
struct ElfSymbolEntryTypes;

template <>
struct ElfSymbolEntryTypes<EI_CLASS_32> {
    using Name = uint32_t;
    using Info = uint8_t;
    using Other = uint8_t;
    using Shndx = uint16_t;
    using Value = uint32_t;
    using Size = uint32_t;
};

template <>
struct ElfSymbolEntryTypes<EI_CLASS_64> {
    using Name = uint32_t;
    using Info = uint8_t;
    using Other = uint8_t;
    using Shndx = uint16_t;
    using Value = uint64_t;
    using Size = uint64_t;
};

template <ELF_IDENTIFIER_CLASS NumBits>
struct ElfSymbolEntry {
    using Name = typename ElfSymbolEntryTypes<NumBits>::Name;
    using Value = typename ElfSymbolEntryTypes<NumBits>::Value;
    using Size = typename ElfSymbolEntryTypes<NumBits>::Size;
    using Info = typename ElfSymbolEntryTypes<NumBits>::Info;
    using Other = typename ElfSymbolEntryTypes<NumBits>::Other;
    using Shndx = typename ElfSymbolEntryTypes<NumBits>::Shndx;
    Name name = 0U;
    Info info = 0U;
    Other other = 0U;
    Shndx shndx = SHN_UNDEF;
    Value value = 0U;
    Size size = 0U;

    Info getBinding() const {
        return info >> 4;
    }

    Info getType() const {
        return info & 0xF;
    }

    Other getVisibility() const {
        return other & 0x3;
    }

    void setBinding(Info binding) {
        info = (info & 0xF) | (binding << 4);
    }

    void setType(Info type) {
        info = (info & (~0xF)) | (type & 0xF);
    }

    void setVisibility(Other visibility) {
        other = (other & (~0x3)) | (visibility & 0x3);
    }
};

static_assert(sizeof(ElfSymbolEntry<EI_CLASS_32>) == 0x10, "");
static_assert(sizeof(ElfSymbolEntry<EI_CLASS_64>) == 0x18, "");

template <int NumBits>
struct ElfRelocationEntryTypes;

template <>
struct ElfRelocationEntryTypes<EI_CLASS_32> {
    using Offset = uint32_t;
    using Info = uint32_t;
    using Addend = int32_t;
};

template <>
struct ElfRelocationEntryTypes<EI_CLASS_64> {
    using Offset = uint64_t;
    using Info = uint64_t;
    using Addend = int64_t;
};

namespace RelocationFuncs {
template <typename T>
constexpr T getSymbolTableIndex(T info);

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_32>::Info getSymbolTableIndex(ElfRelocationEntryTypes<EI_CLASS_32>::Info info) {
    return info >> 8;
}

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_64>::Info getSymbolTableIndex(ElfRelocationEntryTypes<EI_CLASS_64>::Info info) {
    return info >> 32;
}

template <typename T>
constexpr T getRelocationType(T info);

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_32>::Info getRelocationType(ElfRelocationEntryTypes<EI_CLASS_32>::Info info) {
    return static_cast<uint8_t>(info);
}

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_64>::Info getRelocationType(ElfRelocationEntryTypes<EI_CLASS_64>::Info info) {
    return static_cast<uint32_t>(info);
}

template <typename T>
constexpr T setSymbolTableIndex(T info, T index);

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_32>::Info setSymbolTableIndex(ElfRelocationEntryTypes<EI_CLASS_32>::Info info,
                                                                         ElfRelocationEntryTypes<EI_CLASS_32>::Info index) {
    return (info & 0x000000FF) | (index << 8);
}

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_64>::Info setSymbolTableIndex(ElfRelocationEntryTypes<EI_CLASS_64>::Info info,
                                                                         ElfRelocationEntryTypes<EI_CLASS_64>::Info index) {
    return (info & 0x00000000FFFFFFFF) | (index << 32);
}

template <typename T>
constexpr T setRelocationType(T info, T type);

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_32>::Info setRelocationType(ElfRelocationEntryTypes<EI_CLASS_32>::Info info,
                                                                       ElfRelocationEntryTypes<EI_CLASS_32>::Info type) {
    return (info & 0xFFFFFF00) | static_cast<uint8_t>(type);
}

template <>
constexpr ElfRelocationEntryTypes<EI_CLASS_64>::Info setRelocationType(ElfRelocationEntryTypes<EI_CLASS_64>::Info info,
                                                                       ElfRelocationEntryTypes<EI_CLASS_64>::Info type) {
    return (info & 0xFFFFFFFF00000000) | static_cast<uint32_t>(type);
}
} // namespace RelocationFuncs

template <ELF_IDENTIFIER_CLASS NumBits>
struct ElfRel {
    using Offset = typename ElfRelocationEntryTypes<NumBits>::Offset;
    using Info = typename ElfRelocationEntryTypes<NumBits>::Info;
    Offset offset = 0U;
    Info info = 0U;

    constexpr Info getSymbolTableIndex() const {
        return RelocationFuncs::getSymbolTableIndex(info);
    }

    constexpr Info getRelocationType() const {
        return RelocationFuncs::getRelocationType(info);
    }

    constexpr void setSymbolTableIndex(Info index) {
        info = RelocationFuncs::setSymbolTableIndex(info, index);
    }

    constexpr void setRelocationType(Info type) {
        info = RelocationFuncs::setRelocationType(info, type);
    }
};

static_assert(sizeof(ElfRel<EI_CLASS_32>) == 0x8, "");
static_assert(sizeof(ElfRel<EI_CLASS_64>) == 0x10, "");

template <int NumBits>
struct ElfRela {
    using Offset = typename ElfRelocationEntryTypes<NumBits>::Offset;
    using Info = typename ElfRelocationEntryTypes<NumBits>::Info;
    using Addend = typename ElfRelocationEntryTypes<NumBits>::Addend;
    Offset offset = 0U;
    Info info = 0U;
    Addend addend = 0U;

    constexpr Info getSymbolTableIndex() const {
        return RelocationFuncs::getSymbolTableIndex(info);
    }

    constexpr Info getRelocationType() const {
        return RelocationFuncs::getRelocationType(info);
    }

    constexpr void setSymbolTableIndex(Info index) {
        info = RelocationFuncs::setSymbolTableIndex(info, index);
    }

    constexpr void setRelocationType(Info type) {
        info = RelocationFuncs::setRelocationType(info, type);
    }
};

static_assert(sizeof(ElfRela<EI_CLASS_32>) == 0xC, "");
static_assert(sizeof(ElfRela<EI_CLASS_64>) == 0x18, "");

namespace SpecialSectionNames {
constexpr ConstStringRef bss = ".bss";                    // uninitialized memory
constexpr ConstStringRef comment = ".comment";            // version control information
constexpr ConstStringRef data = ".data";                  // initialized memory
constexpr ConstStringRef data1 = ".data1";                // initialized memory
constexpr ConstStringRef debug = ".debug";                // debug symbols
constexpr ConstStringRef debugInfo = ".debug_info";       // debug info
constexpr ConstStringRef dynamic = ".dynamic";            // dynamic linking information
constexpr ConstStringRef dynstr = ".dynstr";              // strings for dynamic linking
constexpr ConstStringRef dynsym = ".dynsym";              // dynamic linking symbol table
constexpr ConstStringRef fini = ".fini";                  // executable instructions of program termination
constexpr ConstStringRef finiArray = ".fini_array";       // function pointers of termination array
constexpr ConstStringRef got = ".got";                    // global offset table
constexpr ConstStringRef hash = ".hash";                  // symnol hash table
constexpr ConstStringRef init = ".init";                  // executable instructions of program initializaion
constexpr ConstStringRef initArray = ".init_array";       // function pointers of initialization array
constexpr ConstStringRef interp = ".interp";              // path name of program interpreter
constexpr ConstStringRef line = ".line";                  // line number info for symbolic debugging
constexpr ConstStringRef note = ".note";                  // note section
constexpr ConstStringRef plt = ".plt";                    // procedure linkage table
constexpr ConstStringRef preinitArray = ".preinit_array"; // function pointers of pre-initialization array
constexpr ConstStringRef relPrefix = ".rel";              // prefix of .relNAME - relocations for NAME section
constexpr ConstStringRef relaPrefix = ".rela";            // prefix of .relaNAME - rela relocations for NAME section
constexpr ConstStringRef rodata = ".rodata";              // read-only data
constexpr ConstStringRef rodata1 = ".rodata1";            // read-only data
constexpr ConstStringRef shStrTab = ".shstrtab";          // section names (strings)
constexpr ConstStringRef strtab = ".strtab";              // strings
constexpr ConstStringRef symtab = ".symtab";              // symbol table
constexpr ConstStringRef symtabShndx = ".symtab_shndx";   // special symbol table section index array
constexpr ConstStringRef tbss = ".tbss";                  // uninitialized thread-local data
constexpr ConstStringRef tadata = ".tdata";               // initialided thread-local data
constexpr ConstStringRef tdata1 = ".tdata1";              // initialided thread-local data
constexpr ConstStringRef text = ".text";                  // executable instructions
} // namespace SpecialSectionNames

} // namespace Elf

} // namespace NEO
