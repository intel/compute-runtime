/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/debug_zebin.h"

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"

namespace NEO {
namespace Debug {
using namespace Elf;

std::vector<uint8_t> createDebugZebin(NEO::Elf::Elf<EI_CLASS_64> &zebin, const GPUSegments &gpuSegments) {
    ElfEncoder<EI_CLASS_64> elfEncoder(false, false);
    auto &header = elfEncoder.getElfFileHeader();
    header.machine = zebin.elfFileHeader->machine;
    header.flags = zebin.elfFileHeader->flags;
    header.type = zebin.elfFileHeader->type;
    header.version = zebin.elfFileHeader->version;
    header.shStrNdx = zebin.elfFileHeader->shStrNdx;

    for (uint32_t i = 0; i < zebin.sectionHeaders.size(); i++) {
        const auto &section = zebin.sectionHeaders[i];
        auto sectionName = zebin.getSectionName(i);
        auto refSectionName = ConstStringRef(sectionName);

        uint64_t segGpuAddr = 0U;
        ArrayRef<const uint8_t> data;

        if (refSectionName.startsWith(SectionsNamesZebin::textPrefix.data())) {
            auto kernelName = sectionName.substr(SectionsNamesZebin::textPrefix.length());
            auto segmentIdIter = gpuSegments.nameToSectIdMap.find(kernelName);
            UNRECOVERABLE_IF(segmentIdIter == gpuSegments.nameToSectIdMap.end());
            const auto &kernel = gpuSegments.kernels[segmentIdIter->second];
            segGpuAddr = kernel.gpuAddress;
            data = kernel.data;
        } else if (refSectionName == SectionsNamesZebin::dataConst) {
            segGpuAddr = gpuSegments.constData.gpuAddress;
            data = gpuSegments.constData.data;
        } else if (refSectionName == SectionsNamesZebin::dataGlobal) {
            segGpuAddr = gpuSegments.varData.gpuAddress;
            data = gpuSegments.varData.data;
        } else {
            data = section.data;
        }

        if (segGpuAddr != 0U) {
            elfEncoder.appendProgramHeaderLoad(i, segGpuAddr, data.size());
        }

        auto &sectionHeader = elfEncoder.appendSection(section.header->type, refSectionName, data);
        sectionHeader.link = section.header->link;
        sectionHeader.info = section.header->info;
        sectionHeader.name = section.header->name;
    }
    return elfEncoder.encode();
}

void patch(uint64_t addr, uint64_t value, RELOC_TYPE_ZEBIN type) {
    switch (type) {
    default:
        UNRECOVERABLE_IF(type != R_ZE_SYM_ADDR)
        *reinterpret_cast<uint64_t *>(addr) = value;
        break;
    case R_ZE_SYM_ADDR_32:
        *reinterpret_cast<uint32_t *>(addr) = static_cast<uint32_t>(value & uint32_t(-1));
        break;
    case R_ZE_SYM_ADDR_32_HI:
        *reinterpret_cast<uint32_t *>(addr) = static_cast<uint32_t>((value >> 32) & uint32_t(-1));
        break;
    }
}

void patchDebugZebin(std::vector<uint8_t> &debugZebin, const GPUSegments &gpuSegments) {
    std::string errors, warnings;
    auto elf = decodeElf(debugZebin, errors, warnings);

    for (const auto &reloc : elf.getDebugInfoRelocations()) {
        auto sectionName = elf.getSectionName(reloc.symbolSectionIndex);
        auto refSectionName = ConstStringRef(sectionName);
        uint64_t sectionAddress = 0U;
        if (refSectionName.startsWith(SectionsNamesZebin::textPrefix.data())) {
            auto kernelName = sectionName.substr(SectionsNamesZebin::textPrefix.length());
            auto segmentIdIter = gpuSegments.nameToSectIdMap.find(kernelName);
            UNRECOVERABLE_IF(segmentIdIter == gpuSegments.nameToSectIdMap.end());
            sectionAddress = gpuSegments.kernels[segmentIdIter->second].gpuAddress;
        } else if (refSectionName.startsWith(SectionsNamesZebin::dataConst.data())) {
            sectionAddress = gpuSegments.constData.gpuAddress;
        } else if (refSectionName.startsWith(SectionsNamesZebin::dataGlobal.data())) {
            sectionAddress = gpuSegments.varData.gpuAddress;
        } else if (refSectionName.startsWith(SectionsNamesZebin::debugPrefix.data())) {
            // do not offset debug symbols
        } else {
            DEBUG_BREAK_IF(true);
            continue;
        }

        auto patchValue = sectionAddress + elf.getSymbolValue(reloc.symbolTableIndex) + reloc.addend;
        auto patchLocation = reinterpret_cast<uint64_t>(debugZebin.data()) + elf.getSectionOffset(reloc.targetSectionIndex) + reloc.offset;

        patch(patchLocation, patchValue, static_cast<RELOC_TYPE_ZEBIN>(reloc.relocType));
    }
}

std::vector<uint8_t> getDebugZebin(ArrayRef<const uint8_t> zebinBin, const GPUSegments &gpuSegments) {
    std::string errors, warnings;
    auto zebin = decodeElf(zebinBin, errors, warnings);
    if (false == errors.empty()) {
        return {};
    }

    auto debugZebin = createDebugZebin(zebin, gpuSegments);
    patchDebugZebin(debugZebin, gpuSegments);
    return debugZebin;
}

} // namespace Debug
} // namespace NEO
