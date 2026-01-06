/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

namespace NEO::Zebin::Elf {
using namespace NEO::Elf;
enum ElfTypeZebin : uint16_t {
    ET_ZEBIN_REL = 0xff11, // A relocatable ZE binary file
    ET_ZEBIN_EXE = 0xff12, // An executable ZE binary file
    ET_ZEBIN_DYN = 0xff13, // A shared object ZE binary file
};

enum SectionHeaderTypeZebin : uint32_t {
    SHT_ZEBIN_SPIRV = 0xff000009,      // .spv.kernel section, value the same as SHT_OPENCL_SPIRV
    SHT_ZEBIN_ZEINFO = 0xff000011,     // .ze_info section
    SHT_ZEBIN_GTPIN_INFO = 0xff000012, // .gtpin_info section
    SHT_ZEBIN_VISA_ASM = 0xff000013,   // .visaasm sections
    SHT_ZEBIN_MISC = 0xff000014        // .misc section
};

enum RelocTypeZebin : uint32_t {
    R_ZE_NONE,
    R_ZE_SYM_ADDR,
    R_ZE_SYM_ADDR_32,
    R_ZE_SYM_ADDR_32_HI,
    R_PER_THREAD_PAYLOAD_OFFSET
};

namespace SectionNames {
inline constexpr ConstStringRef text = ".text";
inline constexpr ConstStringRef textPrefix = ".text.";
inline constexpr ConstStringRef functions = ".text.Intel_Symbol_Table_Void_Program";
inline constexpr ConstStringRef dataConst = ".data.const";
inline constexpr ConstStringRef dataConstZeroInit = ".bss.const";
inline constexpr ConstStringRef dataGlobalConst = ".data.global_const";
inline constexpr ConstStringRef dataGlobal = ".data.global";
inline constexpr ConstStringRef dataGlobalZeroInit = ".bss.global";
inline constexpr ConstStringRef dataConstString = ".data.const.string";
inline constexpr ConstStringRef symtab = ".symtab";
inline constexpr ConstStringRef relTablePrefix = ".rel.";
inline constexpr ConstStringRef relaTablePrefix = ".rela.";
inline constexpr ConstStringRef spv = ".spv";
inline constexpr ConstStringRef debugPrefix = ".debug_";
inline constexpr ConstStringRef debugInfo = ".debug_info";
inline constexpr ConstStringRef debugAbbrev = ".debug_abbrev";
inline constexpr ConstStringRef zeInfo = ".ze_info";
inline constexpr ConstStringRef gtpinInfo = ".gtpin_info.";
inline constexpr ConstStringRef noteIntelGT = ".note.intelgt.compat";
inline constexpr ConstStringRef buildOptions = ".misc.buildOptions";
inline constexpr ConstStringRef vIsaAsmPrefix = ".visaasm.";
inline constexpr ConstStringRef externalFunctions = "Intel_Symbol_Table_Void_Program";
} // namespace SectionNames

inline constexpr ConstStringRef intelGTNoteOwnerName = "IntelGT";
enum IntelGTSectionType : uint32_t {
    productFamily = 1,
    gfxCore = 2,
    targetMetadata = 3,
    zebinVersion = 4,
    vISAAbiVersion = 5, // for debugger only
    productConfig = 6,
    indirectAccessDetectionVersion = 7,
    indirectAccessBufferMajorVersion = 8,
    lastSupported = indirectAccessBufferMajorVersion
};
struct IntelGTNote {
    IntelGTSectionType type;
    ArrayRef<const uint8_t> data;
};
struct ZebinTargetFlags {
    union {
        struct {
            // bit[7:0]: dedicated for specific generator (meaning based on generatorId)
            uint8_t generatorSpecificFlags : 8;

            // bit[12:8]: values [0-31], min compatible device revision Id (stepping)
            uint8_t minHwRevisionId : 5;

            // bit[13:13]:
            // 0 - full validation during decoding (safer decoding)
            // 1 - no validation (faster decoding - recommended for known generators)
            bool validateRevisionId : 1;

            // bit[14:14]:
            // 0 - ignore minHwRevisionId and maxHwRevisionId
            // 1 - underlying device must match specified revisionId info
            bool disableExtendedValidation : 1;

            // bit[15:15]:
            // 0 - elfFileHeader::machine is PRODUCT_FAMILY
            // 1 - elfFileHeader::machine is GFXCORE_FAMILY
            bool machineEntryUsesGfxCoreInsteadOfProductFamily : 1;

            // bit[20:16]:  max compatible device revision Id (stepping)
            uint8_t maxHwRevisionId : 5;

            // bit[23:21]: generator of this device binary
            // 0 - Unregistered
            // 1 - IGC
            uint8_t generatorId : 3;

            // bit[31:24]: MBZ, reserved for future use
        };
        uint32_t packed = 0U;
    };
};
static_assert(sizeof(ZebinTargetFlags) == sizeof(uint32_t), "");

} // namespace NEO::Zebin::Elf
