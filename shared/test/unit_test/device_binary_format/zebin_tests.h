/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/test/common/mocks/mock_elf.h"

#include "igfxfmid.h"

#include <string>
#include <vector>

extern PRODUCT_FAMILY productFamily;

inline std::string toString(NEO::Elf::ZebinKernelMetadata::Types::Version version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}

namespace ZebinTestData {

struct ValidEmptyProgram {
    static constexpr char kernelName[19] = "valid_empty_kernel";

    ValidEmptyProgram() {
        NEO::Elf::ElfEncoder<> enc;
        enc.getElfFileHeader().type = NEO::Elf::ET_ZEBIN_EXE;
        enc.getElfFileHeader().machine = productFamily;
        auto zeInfo = std::string{"---\nversion : \'" + toString(NEO::zeInfoDecoderVersion) + "\'" + "\nkernels : \n  - name : " + kernelName + "\n    execution_env : \n      simd_size  : 32\n      grf_count : 128\n...\n"};
        enc.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, zeInfo);
        enc.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "valid_empty_kernel", zeInfo);
        storage = enc.encode();
        recalcPtr(); // NOLINT(clang-analyzer-optin.cplusplus.VirtualCall)
    }

    virtual void recalcPtr() {
        elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *>(storage.data());
    }

    template <typename SectionHeaderEnumT>
    NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> &appendSection(SectionHeaderEnumT sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData) {
        std::string err, warn;
        auto decoded = NEO::Elf::decodeElf(storage, err, warn);
        NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> enc;
        enc.getElfFileHeader() = *decoded.elfFileHeader;
        int sectionIt = 0;
        auto sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
        NEO::ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
        for (const auto &section : decoded.sectionHeaders) {
            switch (section.header->type) {
            case NEO::Elf::SHN_UNDEF:
                break;
            case NEO::Elf::SHT_STRTAB:
                if (decoded.elfFileHeader->shStrNdx != sectionIt) {
                    enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
                }
                break;
            default:
                enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
                break;
            }
            ++sectionIt;
        }
        enc.appendSection(static_cast<NEO::Elf::SECTION_HEADER_TYPE>(sectionType), sectionLabel, sectionData);
        storage = enc.encode();
        recalcPtr();
        decoded = NEO::Elf::decodeElf(storage, err, warn);
        sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
        sectionHeaderNamesString = NEO::ConstStringRef(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
        for (const auto &section : decoded.sectionHeaders) {
            if ((sectionType == section.header->type) && (sectionLabel == sectionHeaderNamesString.data() + section.header->name)) {
                return const_cast<NEO::Elf::ElfSectionHeader<NEO::Elf::EI_CLASS_64> &>(*section.header);
            }
        }
        UNREACHABLE();
    }

    template <typename SectionHeaderEnumT>
    void removeSection(SectionHeaderEnumT sectionType, NEO::ConstStringRef sectionLabel) {
        std::string err, warn;
        auto decoded = NEO::Elf::decodeElf(storage, err, warn);
        NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> enc;
        enc.getElfFileHeader() = *decoded.elfFileHeader;
        int sectionIt = 0;
        auto sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
        NEO::ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
        for (const auto &section : decoded.sectionHeaders) {
            bool add = true;
            switch (section.header->type) {
            case NEO::Elf::SHN_UNDEF:
                add = false;
                break;
            case NEO::Elf::SHT_STRTAB:
                add = (decoded.elfFileHeader->shStrNdx != sectionIt);
                break;
            default:
                add = ((section.header->type != sectionType) || (sectionHeaderNamesString.data() + section.header->name != sectionLabel));
                break;
            }
            if (add) {
                enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
            }
            ++sectionIt;
        }
        if (decoded.elfFileHeader->shNum <= 3) {
            enc.appendSection(NEO::Elf::SHT_STRTAB, "", {});
        }
        storage = enc.encode();
        recalcPtr();
    }

    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
    std::vector<uint8_t> storage;
};

struct ZebinWithExternalFunctionsInfo {
    ZebinWithExternalFunctionsInfo() {
        std::string zeInfo = std::string("version :\'") + toString(NEO::zeInfoDecoderVersion) + R"===('
kernels:
    - name: kernel
      execution_env:
        simd_size: 8
    - name: Intel_Symbol_Table_Void_Program
      execution_env:
        simd_size: 8
functions:
    - name: fun0
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 0
    - name: fun1
      execution_env:
        grf_count: 128
        simd_size: 8
        barrier_count: 2
)===";
        MockElfEncoder<> elfEncoder;
        auto &elfHeader = elfEncoder.getElfFileHeader();
        elfHeader.type = NEO::Elf::ET_ZEBIN_EXE;
        elfHeader.flags = 0U;

        const uint8_t kData[32] = {0U};
        elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "kernel", kData);
        auto kernelSectionIdx = elfEncoder.getLastSectionHeaderIndex();

        const uint8_t funData[32] = {0U};
        elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + NEO::Elf::SectionsNamesZebin::externalFunctions.str(), funData);
        auto externalFunctionsIdx = elfEncoder.getLastSectionHeaderIndex();

        elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, zeInfo);

        NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbols[2];
        symbols[0].name = decltype(symbols[0].name)(elfEncoder.appendSectionName(fun0Name));
        symbols[0].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
        symbols[0].shndx = decltype(symbols[0].shndx)(externalFunctionsIdx);
        symbols[0].size = 16;
        symbols[0].value = 0;

        symbols[1].name = decltype(symbols[1].name)(elfEncoder.appendSectionName(fun1Name));
        symbols[1].info = NEO::Elf::SYMBOL_TABLE_TYPE::STT_FUNC | NEO::Elf::SYMBOL_TABLE_BIND::STB_GLOBAL << 4;
        symbols[1].shndx = decltype(symbols[1].shndx)(externalFunctionsIdx);
        symbols[1].size = 16;
        symbols[1].value = 16;
        elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Elf::SectionsNamesZebin::symtab,
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(symbols), sizeof(symbols)));

        NEO::Elf::ElfRel<NEO::Elf::EI_CLASS_64> extFuncSegReloc = {}; // fun0 calls fun1
        extFuncSegReloc.offset = 0x8;
        extFuncSegReloc.info = (uint64_t(1) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;
        auto &extFuncRelSection = elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + NEO::Elf::SectionsNamesZebin::textPrefix.str() + NEO::Elf::SectionsNamesZebin::externalFunctions.str(),
                                                           {reinterpret_cast<uint8_t *>(&extFuncSegReloc), sizeof(extFuncSegReloc)});
        extFuncRelSection.info = externalFunctionsIdx;

        NEO::Elf::ElfRel<NEO::Elf::EI_CLASS_64>
            kernelSegReloc = {}; // kernel calls fun0
        kernelSegReloc.offset = 0x8;
        kernelSegReloc.info = (uint64_t(0) << 32) | NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR;
        auto &kernelRelSection = elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + NEO::Elf::SectionsNamesZebin::textPrefix.str() + "kernel",
                                                          {reinterpret_cast<uint8_t *>(&kernelSegReloc), sizeof(kernelSegReloc)});
        kernelRelSection.info = kernelSectionIdx;

        storage = elfEncoder.encode();
        recalcPtr();

        nameToSegId["kernel"] = 0;
        nameToSegId["Intel_Symbol_Table_Void_Program"] = 1;
    }

    void recalcPtr() {
        elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *>(storage.data());
    }

    void setProductFamily(uint16_t productFamily) {
        elfHeader->machine = productFamily;
    }

    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> getElf() {
        std::string errors, warnings;
        auto elf = NEO::Elf::decodeElf({storage.data(), storage.size()}, errors, warnings);
        return elf;
    }
    std::unordered_map<std::string, uint32_t> nameToSegId;
    NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *elfHeader;
    NEO::ConstStringRef fun0Name = "fun0";
    NEO::ConstStringRef fun1Name = "fun1";
    const uint8_t barrierCount = 2;
    std::vector<uint8_t> storage;
};
} // namespace ZebinTestData
