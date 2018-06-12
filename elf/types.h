/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// Abstract: Defines the types used for ELF headers/sections.
#pragma once

#include <inttypes.h>
#include <stddef.h>

namespace CLElfLib {
/******************************************************************************\
 ELF Enumerates
\******************************************************************************/

// E_ID_IDX - Defines a file as being ELF
enum E_ID_IDX {
    ID_IDX_MAGIC0 = 0,
    ID_IDX_MAGIC1 = 1,
    ID_IDX_MAGIC2 = 2,
    ID_IDX_MAGIC3 = 3,
    ID_IDX_CLASS = 4,
    ID_IDX_VERSION = 5,
    ID_IDX_OSABI = 6,
    ID_IDX_ABI_VERSION = 7,
    ID_IDX_PADDING = 8,
    ID_IDX_NUM_BYTES = 16,
};

// E_EHT_CLASS - Describes what data types the ELF structures will use.
enum E_EH_CLASS {
    EH_CLASS_NONE = 0,
    EH_CLASS_32 = 1, // Use Elf32 data types
    EH_CLASS_64 = 2, // Use Elf64 data types
};

// E_EHT_TYPE - List of pre-defined types header types.
//    OS-specific codes start at 0xfe00 and run to 0xfeff.
//    Processor-specific codes start at 0xff00 and end at 0xffff.
enum E_EH_TYPE {
    EH_TYPE_NONE = 0,
    EH_TYPE_RELOCATABLE = 1,
    EH_TYPE_EXECUTABLE = 2,
    EH_TYPE_DYNAMIC = 3,
    EH_TYPE_CORE = 4,
    EH_TYPE_OPENCL_SOURCE = 0xff01,     // format used to pass CL text sections to FE
    EH_TYPE_OPENCL_OBJECTS = 0xff02,    // format used to pass LLVM objects / store LLVM binary output
    EH_TYPE_OPENCL_LIBRARY = 0xff03,    // format used to store LLVM archive output
    EH_TYPE_OPENCL_EXECUTABLE = 0xff04, // format used to store executable output
    EH_TYPE_OPENCL_DEBUG = 0xff05,      // format used to store debug output
};

// E_EH_MACHINE - List of pre-defined machine types.
//    For OpenCL, currently, we do not need this information, so this is not
//    fully defined.
enum E_EH_MACHINE {
    EH_MACHINE_NONE = 0,
    //EHT_MACHINE_LO_RSVD    = 1,   // Beginning of range of reserved types.
    //EHT_MACHINE_HI_RSVD    = 200, // End of range of reserved types.
};

// E_EHT_VERSION - ELF header version options.
enum E_EHT_VERSION {
    EH_VERSION_INVALID = 0,
    EH_VERSION_CURRENT = 1,
};

// E_SH_TYPE - List of pre-defined section header types.
//    Processor-specific codes start at 0xff00 and end at 0xffff.
enum E_SH_TYPE {
    SH_TYPE_NULL = 0,
    SH_TYPE_PROG_BITS = 1,
    SH_TYPE_SYM_TBL = 2,
    SH_TYPE_STR_TBL = 3,
    SH_TYPE_RELO_ADDS = 4,
    SH_TYPE_HASH = 5,
    SH_TYPE_DYN = 6,
    SH_TYPE_NOTE = 7,
    SH_TYPE_NOBITS = 8,
    SH_TYPE_RELO_NO_ADDS = 9,
    SH_TYPE_SHLIB = 10,
    SH_TYPE_DYN_SYM_TBL = 11,
    SH_TYPE_INIT = 14,
    SH_TYPE_FINI = 15,
    SH_TYPE_PRE_INIT = 16,
    SH_TYPE_GROUP = 17,
    SH_TYPE_SYMTBL_SHNDX = 18,
    SH_TYPE_OPENCL_SOURCE = 0xff000000,           // CL source to link into LLVM binary
    SH_TYPE_OPENCL_HEADER = 0xff000001,           // CL header to link into LLVM binary
    SH_TYPE_OPENCL_LLVM_TEXT = 0xff000002,        // LLVM text
    SH_TYPE_OPENCL_LLVM_BINARY = 0xff000003,      // LLVM byte code
    SH_TYPE_OPENCL_LLVM_ARCHIVE = 0xff000004,     // LLVM archives(s)
    SH_TYPE_OPENCL_DEV_BINARY = 0xff000005,       // Device binary (coherent by default)
    SH_TYPE_OPENCL_OPTIONS = 0xff000006,          // CL Options
    SH_TYPE_OPENCL_PCH = 0xff000007,              // PCH (pre-compiled headers)
    SH_TYPE_OPENCL_DEV_DEBUG = 0xff000008,        // Device debug
    SH_TYPE_SPIRV = 0xff000009,                   // SPIRV
    SH_TYPE_NON_COHERENT_DEV_BINARY = 0xff00000a, // Non-coherent Device binary
};

// E_SH_FLAG - List of section header flags.
enum E_SH_FLAG {
    SH_FLAG_WRITE = 0x1,
    SH_FLAG_ALLOC = 0x2,
    SH_FLAG_EXEC_INSTR = 0x4,
    SH_FLAG_MERGE = 0x8,
    SH_FLAG_STRINGS = 0x10,
    SH_FLAG_INFO_LINK = 0x20,
    SH_FLAG_LINK_ORDER = 0x40,
    SH_FLAG_OS_NONCONFORM = 0x100,
    SH_FLAG_GROUP = 0x200,
    SH_FLAG_TLS = 0x400,
    SH_FLAG_MASK_OS = 0x0ff00000,
    SH_FLAG_MASK_PROC = 0xf0000000,
};

/******************************************************************************\
 ELF-64 Data Types
\******************************************************************************/
#if defined(_MSC_VER) // && (_MSC_VER < 1700)
typedef unsigned __int64 Elf64_Addr;
typedef unsigned __int64 Elf64_Off;
typedef unsigned __int16 Elf64_Short; // Renaming Elf64_Half to Elf64_Short to avoid a conflict with Android
typedef unsigned __int32 Elf64_Word;
typedef __int32 Elf64_Sword;
typedef unsigned __int64 Elf64_Xword;
#else
#if !defined(_UAPI_LINUX_ELF_H)
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
#endif
typedef uint16_t Elf64_Short; // Renaming Elf64_Half to Elf64_Short to avoid a conflict with Android
#endif

/******************************************************************************\
 ELF Constants
\******************************************************************************/
static const unsigned char ELF_MAG0 = 0x7f;     // ELFHeader.Identity[ELF_ID_MAGIC0]
static const unsigned char ELF_MAG1 = 'E';      // ELFHeader.Identity[ELF_ID_MAGIC1]
static const unsigned char ELF_MAG2 = 'L';      // ELFHeader.Identity[ELF_ID_MAGIC2]
static const unsigned char ELF_MAG3 = 'F';      // ELFHeader.Identity[ELF_ID_MAGIC3]
static const unsigned int ELF_ALIGN_BYTES = 16; // Alignment set to 16-bytes

/******************************************************************************\
 ELF-64 Header
\******************************************************************************/
struct SElf64Header {
    unsigned char Identity[ID_IDX_NUM_BYTES];
    Elf64_Short Type;
    Elf64_Short Machine;
    Elf64_Word Version;
    Elf64_Addr EntryAddress;
    Elf64_Off ProgramHeadersOffset;
    Elf64_Off SectionHeadersOffset;
    Elf64_Word Flags;
    Elf64_Short ElfHeaderSize;
    Elf64_Short ProgramHeaderEntrySize;
    Elf64_Short NumProgramHeaderEntries;
    Elf64_Short SectionHeaderEntrySize;
    Elf64_Short NumSectionHeaderEntries;
    Elf64_Short SectionNameTableIndex;
};

/******************************************************************************\
 ELF-64 Section Header
\******************************************************************************/
struct SElf64SectionHeader {
    Elf64_Word Name;
    Elf64_Word Type;
    Elf64_Xword Flags;
    Elf64_Addr Address;
    Elf64_Off DataOffset;
    Elf64_Xword DataSize;
    Elf64_Word Link;
    Elf64_Word Info;
    Elf64_Xword Alignment;
    Elf64_Xword EntrySize;
};

} // namespace CLElfLib
