/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NEO {

namespace Elf {

template <ElfIdentifierClass NumBits = NEO::Elf::EI_CLASS_64>
struct MutableSectionHeader {
    MutableSectionHeader(const std::string &name, const NEO::Elf::ElfSectionHeader<NumBits> &header, const std::vector<uint8_t> &data)
        : name(name), header(header), data(data) {
    }
    std::string name;
    NEO::Elf::ElfSectionHeader<NumBits> header{};
    std::vector<uint8_t> data;
};

template <ElfIdentifierClass NumBits = NEO::Elf::EI_CLASS_64>
struct MutableProgramHeader {
    MutableProgramHeader(const NEO::Elf::ElfProgramHeader<NumBits> &header, const std::vector<uint8_t> &data)
        : header(header), data(data) {
    }
    NEO::Elf::ElfProgramHeader<NumBits> header = {};
    std::vector<uint8_t> data;
    MutableSectionHeader<NumBits> *referencedSectionData = nullptr;
};

template <ElfIdentifierClass NumBits = NEO::Elf::EI_CLASS_64>
struct ElfRewriter {
    using SectionId = uint32_t;

    ElfRewriter(NEO::Elf::Elf<NumBits> &src) {
        elfFileHeader = *src.elfFileHeader;
        for (const auto &sh : src.sectionHeaders) {
            this->sectionHeaders.push_back(std::make_unique<MutableSectionHeader<NumBits>>(src.getName(sh.header->name), *sh.header, std::vector<uint8_t>{sh.data.begin(), sh.data.end()}));
        }
        for (const auto &ph : src.programHeaders) {
            this->programHeaders.push_back(std::make_unique<MutableProgramHeader<NumBits>>(*ph.header, std::vector<uint8_t>{ph.data.begin(), ph.data.end()}));
            for (const auto &sh : this->sectionHeaders) {
                if ((sh->header.offset == ph.header->offset) && (sh->header.size == ph.header->fileSz)) {
                    (*this->programHeaders.rbegin())->referencedSectionData = sh.get();
                }
            }
        }
    }

    std::vector<uint8_t> encode() const {
        NEO::Elf::ElfEncoder<NumBits> encoder;
        encoder.getElfFileHeader() = elfFileHeader;
        std::unordered_map<MutableSectionHeader<NumBits> *, decltype(NEO::Elf::ElfSectionHeader<NumBits>::offset)> encodedSectionsOffsets;
        for (const auto &sh : this->sectionHeaders) {
            if ((sh->header.type == SHT_NULL) || (sh->header.type == SHT_STRTAB)) {
                continue;
            }
            auto nameIdx = encoder.appendSectionName(sh->name);
            NEO::Elf::ElfSectionHeader<NumBits> header = sh->header;
            header.name = nameIdx;
            encodedSectionsOffsets[sh.get()] = encoder.appendSection(header, sh->data).offset;
        }
        for (const auto &ph : this->programHeaders) {
            if (ph->referencedSectionData) {
                auto &header = ph->header;
                header.offset = encodedSectionsOffsets[ph->referencedSectionData];
                encoder.appendSegment(ph->header, {});
            } else {
                encoder.appendSegment(ph->header, ph->data);
            }
        }
        return encoder.encode();
    }

    StackVec<SectionId, 16> findSections(typename ElfSectionHeaderTypes<NumBits>::Type type, ConstStringRef name) {
        StackVec<SectionId, 16> ret;
        for (size_t i = 0; i < this->sectionHeaders.size(); i++) {
            auto &section = this->sectionHeaders[i];
            if ((type == section->header.type) && (name == section->name)) {
                ret.push_back(static_cast<SectionId>(i));
            }
        }
        return ret;
    }

    MutableSectionHeader<NumBits> &getSection(SectionId idx) {
        return *sectionHeaders[idx];
    }

    void removeSection(SectionId idx) {
        auto sectionHeaderToRemove = std::move(sectionHeaders[idx]);
        for (auto it = idx + 1; it < sectionHeaders.size(); ++it) { // preserve order
            sectionHeaders[it - 1] = std::move(sectionHeaders[it]);
        }
        sectionHeaders.pop_back();
        for (auto &programHeader : programHeaders) {
            if (sectionHeaderToRemove.get() == programHeader->referencedSectionData) {
                programHeader->referencedSectionData = nullptr;
            }
        }
    }

    ElfFileHeader<NumBits> elfFileHeader = {};

  protected:
    StackVec<std::unique_ptr<MutableSectionHeader<NumBits>>, 32> sectionHeaders;
    StackVec<std::unique_ptr<MutableProgramHeader<NumBits>>, 32> programHeaders;
};

} // namespace Elf

} // namespace NEO
