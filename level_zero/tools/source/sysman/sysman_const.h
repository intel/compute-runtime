/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("Unknown");
const std::string intelPciId("0x8086");
constexpr uint32_t MbpsToBytesPerSecond = 125000;
constexpr double milliVoltsFactor = 1000.0;

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
constexpr uint64_t numSocTemperatureEntries = 7;
constexpr uint32_t numCoreTemperatureEntries = 4;
constexpr uint32_t milliFactor = 1000u;