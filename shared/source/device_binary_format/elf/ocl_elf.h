/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Abstract: Defines the types used for ELF headers/sections.
#pragma once

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/utilities/const_stringref.h"

#include <inttypes.h>
#include <stddef.h>

namespace NEO {

namespace Elf {

enum ELF_TYPE_OPENCL : uint16_t {
    ET_OPENCL_SOURCE = 0xff01,     // format used to pass CL text sections to FE
    ET_OPENCL_OBJECTS = 0xff02,    // format used to pass LLVM objects / store LLVM binary output
    ET_OPENCL_LIBRARY = 0xff03,    // format used to store LLVM archive output
    ET_OPENCL_EXECUTABLE = 0xff04, // format used to store executable output
    ET_OPENCL_DEBUG = 0xff05,      // format used to store debug output
};
static_assert(sizeof(ELF_TYPE_OPENCL) == sizeof(ELF_TYPE), "");
static_assert(static_cast<uint16_t>(ET_OPENCL_SOURCE) == static_cast<uint16_t>(ET_OPENCL_RESERVED_START), "");
static_assert(static_cast<uint16_t>(ET_OPENCL_DEBUG) == static_cast<uint16_t>(ET_OPENCL_RESERVED_END), "");

enum SHT_OPENCL : uint32_t {
    SHT_OPENCL_SOURCE = 0xff000000,           // CL source to link into LLVM binary
    SHT_OPENCL_HEADER = 0xff000001,           // CL header to link into LLVM binary
    SHT_OPENCL_LLVM_TEXT = 0xff000002,        // LLVM text
    SHT_OPENCL_LLVM_BINARY = 0xff000003,      // LLVM byte code
    SHT_OPENCL_LLVM_ARCHIVE = 0xff000004,     // LLVM archives(s)
    SHT_OPENCL_DEV_BINARY = 0xff000005,       // Device binary (coherent by default)
    SHT_OPENCL_OPTIONS = 0xff000006,          // CL Options
    SHT_OPENCL_PCH = 0xff000007,              // PCH (pre-compiled headers)
    SHT_OPENCL_DEV_DEBUG = 0xff000008,        // Device debug
    SHT_OPENCL_SPIRV = 0xff000009,            // SPIRV
    SHT_NON_COHERENT_DEV_BINARY = 0xff00000a, // Non-coherent Device binary
};
static_assert(sizeof(SHT_OPENCL) == sizeof(SECTION_HEADER_TYPE), "");
static_assert(static_cast<uint32_t>(SHT_OPENCL_SOURCE) == static_cast<uint32_t>(SHT_OPENCL_RESERVED_START), "");
static_assert(static_cast<uint32_t>(SHT_NON_COHERENT_DEV_BINARY) == static_cast<uint32_t>(SHT_OPENCL_RESERVED_END), "");

namespace SectionNamesOpenCl {
static constexpr ConstStringRef buildOptions = "BuildOptions";
static constexpr ConstStringRef spirvObject = "SPIRV Object";
static constexpr ConstStringRef llvmObject = "Intel(R) OpenCL LLVM Object";
static constexpr ConstStringRef deviceDebug = "Intel(R) OpenCL Device Debug";
static constexpr ConstStringRef deviceBinary = "Intel(R) OpenCL Device Binary";
} // namespace SectionNamesOpenCl

} // namespace Elf

} // namespace NEO
