/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"

#include "igfxfmid.h"

#include <vector>

extern PRODUCT_FAMILY productFamily;

inline std::string toString(NEO::Elf::ZebinKernelMetadata::Types::Version version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor);
}

namespace ZebinTestData {

struct ValidEmptyProgram {
    ValidEmptyProgram() {
        NEO::Elf::ElfEncoder<> enc;
        enc.getElfFileHeader().type = NEO::Elf::ET_ZEBIN_EXE;
        enc.getElfFileHeader().machine = productFamily;
        auto zeInfo = std::string{"---\nversion : \'" + toString(NEO::zeInfoDecoderVersion) + "\'" + "\nkernels : \n...\n"};
        enc.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, zeInfo);
        storage = enc.encode();
        recalcPtr();
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

} // namespace ZebinTestData
