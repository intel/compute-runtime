/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/debug_zebin.h"

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/shared_pool_allocation.h"

namespace NEO::Zebin::Debug {
using namespace NEO::Zebin::Elf;

Segments::Segments() {}

Segments::Segments(const SharedPoolAllocation *globalVarAlloc, const SharedPoolAllocation *globalConstAlloc, ArrayRef<const uint8_t> &globalStrings, std::vector<KernelNameIsaTupleT> &kernels) {
    if (globalVarAlloc) {
        varData = {static_cast<uintptr_t>(globalVarAlloc->getGpuAddress()), globalVarAlloc->getSize()};
    }
    if (globalConstAlloc) {
        constData = {static_cast<uintptr_t>(globalConstAlloc->getGpuAddress()), globalConstAlloc->getSize()};
    }
    if (false == globalStrings.empty()) {
        stringData = {reinterpret_cast<uintptr_t>(globalStrings.begin()), globalStrings.size()};
    }
    for (auto &[kernelName, isaSegment] : kernels) {
        nameToSegMap.insert(std::pair(kernelName, isaSegment));
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
    ElfEncoder<EI_CLASS_64> elfEncoder(false, false, 8);
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
            if (!isCpuSegment(sectionName)) {
                elfEncoder.appendProgramHeaderLoad(i, segment->address, segment->size);
            }
            sectionHeader.addr = segment->address;
        }
    }
    debugZebin = elfEncoder.encode();
}

#pragma pack(push, 1)
template <typename T>
struct SafeType {
    T value;
};
#pragma pack(pop)

template void patchWithValue<uint32_t>(uintptr_t addr, uint32_t value);
template void patchWithValue<uint64_t>(uintptr_t addr, uint64_t value);
template <typename T>
void patchWithValue(uintptr_t addr, T value) {
    if (isAligned<sizeof(T)>(addr)) {
        *reinterpret_cast<T *>(addr) = value;
    } else {
        reinterpret_cast<SafeType<T> *>(addr)->value = value;
    }
}

void DebugZebinCreator::applyRelocation(uintptr_t addr, uint64_t value, RelocTypeZebin type) {
    switch (type) {
    default:
        UNRECOVERABLE_IF(type != R_ZE_SYM_ADDR)
        return patchWithValue<uint64_t>(addr, value);
    case R_ZE_SYM_ADDR_32:
        return patchWithValue<uint32_t>(addr, static_cast<uint32_t>(value & uint32_t(-1)));
    case R_ZE_SYM_ADDR_32_HI:
        return patchWithValue<uint32_t>(addr, static_cast<uint32_t>((value >> 32) & uint32_t(-1)));
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
        } else if (ConstStringRef(symbolSectionName).startsWith(SectionNames::debugPrefix.data()) &&
                   ConstStringRef(symbolName).startsWith(SectionNames::textPrefix.data())) {
            symbol.value += getTextSegmentByName(symbolName)->address;
        }
    }

    for (const auto *relocations : {&elf.getDebugInfoRelocations(), &elf.getRelocations()}) {
        for (const auto &reloc : *relocations) {
            auto relocType = static_cast<RelocTypeZebin>(reloc.relocType);
            if (isRelocTypeSupported(relocType) == false) {
                continue;
            }

            auto relocAddr = reinterpret_cast<uintptr_t>(debugZebin.data() + elf.getSectionOffset(reloc.targetSectionIndex) + reloc.offset);
            uint64_t relocVal = symbols[reloc.symbolTableIndex].value + reloc.addend;
            applyRelocation(relocAddr, relocVal, relocType);
        }
    }
}

bool DebugZebinCreator::isRelocTypeSupported(RelocTypeZebin type) {
    return type == RelocTypeZebin::R_ZE_SYM_ADDR ||
           type == RelocTypeZebin::R_ZE_SYM_ADDR_32 ||
           type == RelocTypeZebin::R_ZE_SYM_ADDR_32_HI;
}

const Segments::Segment *DebugZebinCreator::getSegmentByName(ConstStringRef sectionName) {
    if (sectionName.startsWith(SectionNames::textPrefix.data())) {
        return getTextSegmentByName(sectionName);
    } else if (sectionName == SectionNames::dataConst) {
        return &segments.constData;
    } else if (sectionName == SectionNames::dataGlobal) {
        return &segments.varData;
    } else if (sectionName == SectionNames::dataConstString) {
        return &segments.stringData;
    }
    return nullptr;
}

const Segments::Segment *DebugZebinCreator::getTextSegmentByName(ConstStringRef sectionName) {
    auto kernelName = sectionName.substr(SectionNames::textPrefix.length());
    auto kernelSegmentIt = segments.nameToSegMap.find(kernelName.str());
    UNRECOVERABLE_IF(kernelSegmentIt == segments.nameToSegMap.end());
    return &kernelSegmentIt->second;
}

bool DebugZebinCreator::isCpuSegment(ConstStringRef sectionName) {
    return (sectionName == SectionNames::dataConstString);
}

} // namespace NEO::Zebin::Debug
