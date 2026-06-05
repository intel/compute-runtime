/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <chrono>
#include <cstdint>

namespace NEO {

enum class FabricVertexType : uint16_t {
    device = 1,
    subdevice = 2,
    switchType = 3,
};

struct FabricVertexInfo {
    uint16_t fabricId;
    FabricVertexType type;
    uint8_t uuid[16];
    uint32_t pciBdf;
};

enum class FabricBandwidthUnit : uint8_t {
    unknown = 0,
    bytesPerSec = 1,
    gbps = 2,
};

enum class FabricLatencyUnit : uint8_t {
    unknown = 0,
    nanosec = 1,
    hop = 2,
};

enum class FabricDuplexMode : uint8_t {
    unknown = 0,
    halfDuplex = 1,
    fullDuplex = 2,
};

struct FabricEdgeInfo {
    uint16_t srcFabricId;
    uint16_t dstFabricId;
    uint8_t srcPort;
    uint8_t dstPort;
    uint8_t hopCount;
    uint8_t edgeFlags;
    uint64_t bandwidth;
    uint32_t latency;
    FabricBandwidthUnit bandwidthUnit;
    FabricLatencyUnit latencyUnit;
    FabricDuplexMode duplexMode;
};

constexpr uint64_t bitsPerByte = 8ULL;

inline uint32_t convertBandwidthToBytesPerNanosec(uint64_t bandwidth, FabricBandwidthUnit unit) {
    switch (unit) {
    case FabricBandwidthUnit::bytesPerSec:
        return static_cast<uint32_t>(bandwidth / std::chrono::nanoseconds::period::den);
    case FabricBandwidthUnit::gbps:
        return static_cast<uint32_t>(bandwidth / bitsPerByte);
    default:
        return 0;
    }
}

} // namespace NEO
