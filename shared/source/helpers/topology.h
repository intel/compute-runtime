/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/topology_map.h"

#include <cstdint>
#include <span>

namespace NEO {

struct HardwareInfo;

struct TopologyBitmap {
    std::span<const uint8_t> dssGeometry;
    std::span<const uint8_t> dssCompute;
    std::span<const uint8_t> l3Banks;
    std::span<const uint8_t> eu; // shared by all subslices
};

struct TopologyInfo {
    int sliceCount;
    int subSliceCount;
    int euCount;
    int l3BankCount;
};

struct TopologyLimits {
    int maxSlices;
    int maxSubSlicesPerSlice;
    int maxEusPerSubSlice;
};

TopologyInfo getTopologyInfo(HardwareInfo &hwInfo, const TopologyBitmap &topologyBitmap, const TopologyLimits &topologyLimits, TopologyMapping &topologyMapping, bool scanFullBitmap);
TopologyInfo getTopologyInfoMultiTile(HardwareInfo &hwInfo, std::span<const TopologyBitmap> topologyBitmap, const TopologyLimits &topologyLimits, TopologyMap &topologyMap, bool scanFullBitmap);
} // namespace NEO
