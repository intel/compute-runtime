/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/debug_zebin.h"

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
namespace Debug {
using namespace Elf;

Segments::Segments() {}

Segments::Segments(const GraphicsAllocation *globalVarAlloc, const GraphicsAllocation *globalConstAlloc, ArrayRef<const uint8_t> &globalStrings, std::vector<KernelNameIsaPairT> &kernels) {
    if (globalVarAlloc) {
        varData = {static_cast<uintptr_t>(globalVarAlloc->getGpuAddress()), globalVarAlloc->getUnderlyingBufferSize()};
    }
    if (globalConstAlloc) {
        constData = {static_cast<uintptr_t>(globalConstAlloc->getGpuAddress()), globalConstAlloc->getUnderlyingBufferSize()};
    }
    if (false == globalStrings.empty()) {
        stringData = {reinterpret_cast<uintptr_t>(globalStrings.begin()), globalStrings.size()};
    }
    for (auto &[kernelName, isa] : kernels) {
        Debug::Segments::Segment kernelSegment = {static_cast<uintptr_t>(isa->getGpuAddress()), isa->getUnderlyingBufferSize()};
        nameToSegMap.insert(std::pair(kernelName, kernelSegment));
    }
}

std::vector<uint8_t> createDebugZebin(ArrayRef<const uint8_t> zebinBin, const Segments &gpuSegments) {
    std::string errors, warnings;
    auto zebin = decodeElf(zebinBin, errors, warnings);
    if (false == errors.empty()) {
        return {};
    }

    auto dzc = DebugZebinCreator(zebin, gpuSegments);
    dzc.createDebugZebin();
    dzc.applyRelocations();
    return dzc.getDebugZebin();
}

void DebugZebinCreator::createDebugZebin() {
    ElfEncoder<EI_CLASS_64> elfEncoder(false, false, 4);
    auto &header = elfEncoder.getElfFileHeader();
    header.machine = zebin.elfFileHeader->machine;
    header.flags = zebin.elfFileHeader->flags;
    header.type = NEO::Elf::ET_EXEC;
    header.version = zebin.elfFileHeader->version;
    header.shStrNdx = zebin.elfFileHeader->shStrNdx;

    for (uint32_t i = 0; i < zebin.sectionHeaders.size(); i++) {
        const auto &section = zebin.sectionHeaders[i];
        auto sectionName = zebin.getSectionName(i);

        ArrayRef<const uint8_t> sectionData = section.data;
        if (section.header->type == SHT_SYMTAB) {
            symTabShndx = i;
        }

        auto &sectionHeader = elfEncoder.appendSection(section.header->type, sectionName, sectionData);
        sectionHeader.link = section.header->link;
        sectionHeader.info = section.header->info;
        sectionHeader.name = section.header->name;
        sectionHeader.flags = section.header->flags;

        if (auto segment = getSegmentByName(sectionName)) {
            elfEncoder.appendProgramHeaderLoad(i, segment->address, segment->size);
            sectionHeader.addr = segment->address;
        }
    }
    debugZebin = elfEncoder.encode();
}

void DebugZebinCreator::applyRelocation(uint64_t addr, uint64_t value, RELOC_TYPE_ZEBIN type) {
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

void DebugZebinCreator::applyRelocations() {
    if (symTabShndx == std::numeric_limits<uint32_t>::max()) {
        return;
    }

    using ElfSymbolT = ElfSymbolEntry<EI_CLASS_64>;
    std::string errors, warnings;
    auto elf = decodeElf(debugZebin, errors, warnings);

    auto symTabSecHdr = elf.sectionHeaders[symTabShndx].header;
    size_t symbolsCount = static_cast<size_t>(symTabSecHdr->size) / static_cast<size_t>(symTabSecHdr->entsize);
    ArrayRef<ElfSymbolT> symbols = {reinterpret_cast<ElfSymbolT *>(debugZebin.data() + symTabSecHdr->offset), symbolsCount};
    for (auto &symbol : symbols) {
        auto symbolSectionName = elf.getSectionName(symbol.shndx);
        auto symbolName = elf.getSymbolName(symbol.name);

        auto segment = getSegmentByName(symbolSectionName);
        if (segment != nullptr) {
            symbol.value += segment->address;
        } else if (ConstStringRef(symbolSectionName).startsWith(SectionsNamesZebin::debugPrefix.data()) &&
                   ConstStringRef(symbolName).startsWith(SectionsNamesZebin::textPrefix.data())) {
            symbol.value += getTextSegmentByName(symbolName)->address;
        }
    }

    for (const auto &relocations : {elf.getDebugInfoRelocations(), elf.getRelocations()}) {
        for (const auto &reloc : relocations) {
            auto relocType = static_cast<RELOC_TYPE_ZEBIN>(reloc.relocType);
            if (isRelocTypeSupported(relocType) == false) {
                continue;
            }

            auto relocAddr = reinterpret_cast<uint64_t>(debugZebin.data() + elf.getSectionOffset(reloc.targetSectionIndex) + reloc.offset);
            uint64_t relocVal = symbols[reloc.symbolTableIndex].value + reloc.addend;
            applyRelocation(relocAddr, relocVal, relocType);
        }
    }
}

bool DebugZebinCreator::isRelocTypeSupported(NEO::Elf::RELOC_TYPE_ZEBIN type) {
    return type == NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR ||
           type == NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32 ||
           type == NEO::Elf::RELOC_TYPE_ZEBIN::R_ZE_SYM_ADDR_32_HI;
}

const Segments::Segment *DebugZebinCreator::getSegmentByName(ConstStringRef sectionName) {
    if (sectionName.startsWith(SectionsNamesZebin::textPrefix.data())) {
        return getTextSegmentByName(sectionName);
    } else if (sectionName == SectionsNamesZebin::dataConst) {
        return &segments.constData;
    } else if (sectionName == SectionsNamesZebin::dataGlobal) {
        return &segments.varData;
    } else if (sectionName == SectionsNamesZebin::dataConstString) {
        return &segments.stringData;
    }
    return nullptr;
}

const Segments::Segment *DebugZebinCreator::getTextSegmentByName(ConstStringRef sectionName) {
    auto kernelName = sectionName.substr(SectionsNamesZebin::textPrefix.length());
    auto kernelSegmentIt = segments.nameToSegMap.find(kernelName.str());
    UNRECOVERABLE_IF(kernelSegmentIt == segments.nameToSegMap.end());
    return &kernelSegmentIt->second;
}

} // namespace Debug
} // namespace NEO
