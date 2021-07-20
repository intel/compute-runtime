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
struct GPUSegments {
    struct Segment {
        uintptr_t gpuAddress = std::numeric_limits<uintptr_t>::max();
        ArrayRef<const uint8_t> data;
    };
    using KernelNameToSectionIdMap = std::unordered_map<std::string, size_t>;
    Segment varData;
    Segment constData;
    StackVec<Segment, 8> kernels;
    KernelNameToSectionIdMap nameToSectIdMap;
};
void patch(uint64_t addr, uint64_t value, NEO::Elf::RELOC_TYPE_ZEBIN type);
void patchDebugZebin(std::vector<uint8_t> &debugZebin, const GPUSegments &segmentData);
std::vector<uint8_t> createDebugZebin(ArrayRef<const uint8_t> zebin, const GPUSegments &segmentData);

std::vector<uint8_t> getDebugZebin(ArrayRef<const uint8_t> zebin, const GPUSegments &segmentData);
} // namespace Debug
} // namespace NEO
