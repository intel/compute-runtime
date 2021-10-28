/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/stackvec.h"

#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
namespace Debug {
struct Segments {
    struct Segment {
        uintptr_t address = std::numeric_limits<uintptr_t>::max();
        ArrayRef<const uint8_t> data;
    };
    using CPUSegment = Segment;
    using GPUSegment = Segment;
    using KernelNameToSegmentMap = std::unordered_map<std::string, GPUSegment>;

    GPUSegment varData;
    GPUSegment constData;
    CPUSegment stringData;
    KernelNameToSegmentMap nameToSegMap;
};

class DebugZebinCreator {
  public:
    using Elf = NEO::Elf::Elf<NEO::Elf::EI_CLASS_64>;
    DebugZebinCreator() = delete;
    DebugZebinCreator(Elf &zebin, const Segments &segments) : segments(segments), zebin(zebin) {}

    void applyDebugRelocations();
    void createDebugZebin();
    inline std::vector<uint8_t> getDebugZebin() { return debugZebin; }

  protected:
    void applyRelocation(uint64_t addr, uint64_t value, NEO::Elf::RELOC_TYPE_ZEBIN type);
    const Segments::Segment *getSegmentByName(ConstStringRef sectionName);

    std::vector<uint8_t> debugZebin;
    const Segments &segments;
    Elf &zebin;
};

std::vector<uint8_t> createDebugZebin(ArrayRef<const uint8_t> zebin, const Segments &segmentData);
} // namespace Debug
} // namespace NEO
