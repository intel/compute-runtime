/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/utilities/arrayref.h"

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
namespace Elf {
enum RELOC_TYPE_ZEBIN : uint32_t;
}

class GraphicsAllocation;
namespace Debug {
struct Segments {
    struct Segment {
        uintptr_t address = std::numeric_limits<uintptr_t>::max();
        size_t size = 0;
    };
    using CPUSegment = Segment;
    using GPUSegment = Segment;
    using KernelNameIsaPairT = std::pair<std::string_view, GraphicsAllocation *>;
    using KernelNameToSegmentMap = std::unordered_map<std::string, GPUSegment>;

    GPUSegment varData;
    GPUSegment constData;
    CPUSegment stringData;
    KernelNameToSegmentMap nameToSegMap;
    Segments();
    Segments(const GraphicsAllocation *globalVarAlloc, const GraphicsAllocation *globalConstAlloc, ArrayRef<const uint8_t> &globalStrings, std::vector<KernelNameIsaPairT> &kernels);
};

class DebugZebinCreator {
  public:
    using Elf = NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>;
    DebugZebinCreator() = delete;
    DebugZebinCreator(Elf &zebin, const Segments &segments) : segments(segments), zebin(zebin) {}

    void applyRelocations();
    void createDebugZebin();
    inline std::vector<uint8_t> getDebugZebin() { return debugZebin; }

  protected:
    void applyRelocation(uintptr_t addr, uint64_t value, NEO::Elf::RELOC_TYPE_ZEBIN type);
    bool isRelocTypeSupported(NEO::Elf::RELOC_TYPE_ZEBIN type);
    const Segments::Segment *getSegmentByName(ConstStringRef sectionName);
    const Segments::Segment *getTextSegmentByName(ConstStringRef textSegmentName);
    bool isCpuSegment(ConstStringRef sectionName);

    const Segments &segments;
    const Elf &zebin;
    uint32_t symTabShndx = std::numeric_limits<uint32_t>::max();
    std::vector<uint8_t> debugZebin;
};

template <typename T>
void patchWithValue(uintptr_t addr, T value);

std::vector<uint8_t> createDebugZebin(ArrayRef<const uint8_t> zebin, const Segments &segmentData);
} // namespace Debug
} // namespace NEO
