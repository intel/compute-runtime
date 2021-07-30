/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <chrono>
#include <string>
const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("unknown");
const std::string intelPciId("0x8086");
constexpr uint32_t MbpsToBytesPerSecond = 125000;
constexpr double milliVoltsFactor = 1000.0;

namespace L0 {
struct steadyClock {
    typedef std::chrono::duration<uint64_t, std::milli> duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::time_point<steadyClock> time_point;
    static constexpr bool is_steady = true;
    static time_point now() noexcept {
        static auto epoch = std::chrono::steady_clock::now();
        return time_point(std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - epoch));
    }
};
} // namespace L0

namespace PciLinkSpeeds {
constexpr double Pci2_5GigatransfersPerSecond = 2.5;
constexpr double Pci5_0GigatransfersPerSecond = 5.0;
constexpr double Pci8_0GigatransfersPerSecond = 8.0;
constexpr double Pci16_0GigatransfersPerSecond = 16.0;
constexpr double Pci32_0GigatransfersPerSecond = 32.0;

} // namespace PciLinkSpeeds
enum PciGenerations {
    PciGen1 = 1,
    PciGen2,
    PciGen3,
    PciGen4,
    PciGen5,
};

constexpr uint8_t maxPciBars = 6;
// Linux kernel would report 255 link width, as an indication of unknown.
constexpr uint32_t unknownPcieLinkWidth = 255u;

constexpr uint32_t microSecondsToNanoSeconds = 1000u;

constexpr uint64_t convertJouleToMicroJoule = 1000000u;
constexpr uint64_t minTimeoutModeHeartbeat = 5000u;
constexpr uint64_t minTimeoutInMicroSeconds = 1000u;
constexpr uint16_t milliSecsToMicroSecs = 1000;
constexpr uint32_t milliFactor = 1000u;
constexpr uint32_t microFacor = milliFactor * milliFactor;
constexpr uint64_t gigaUnitTransferToUnitTransfer = 1000 * 1000 * 1000;

constexpr int32_t memoryBusWidth = 128; // bus width in bits
constexpr int32_t numMemoryChannels = 8;
#define BITS(x, at, width) (((x) >> (at)) & ((1 << (width)) - 1))