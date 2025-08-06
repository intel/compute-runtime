/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <vector>

namespace NEO {
struct HardwareInfo;

namespace DeviceBlobConstants {
constexpr uint32_t maxSlicesSupported = 1;
constexpr uint32_t maxDualSubSlicesSupported = 2;
constexpr uint32_t maxEuPerDualSubSlice = 3;
constexpr uint32_t maxMemoryChannels = 10;
constexpr uint32_t memoryType = 11;
constexpr uint32_t numThreadsPerEu = 15;
constexpr uint32_t maxRcs = 23;
constexpr uint32_t maxCcs = 24;
constexpr uint32_t csrSizeInMb = 62;
constexpr uint32_t l3BankSizeInKb = 64;
constexpr uint32_t slmSizePerDss = 65;
constexpr uint32_t maxSubSlicesSupported = 70;
constexpr uint32_t maxEuPerSubSlice = 71;
constexpr uint32_t slmSizePerSs = 73;
constexpr uint32_t numHbmStacksPerTile = 74;
constexpr uint32_t numChannelsPerHbmStack = 75;
constexpr uint32_t numRegions = 83;
constexpr uint32_t numL3BanksPerGroup = 84;
constexpr uint32_t numL3BankGroups = 85;

enum MemoryType {
    lpddr4,
    lpddr5,
    hbm2,
    hbm2e,
    gddr6,
    hbm3
};
} // namespace DeviceBlobConstants

struct SystemInfo {

    SystemInfo(const std::vector<uint32_t> &inputData);

    ~SystemInfo() = default;

    uint32_t getMaxSlicesSupported() const { return maxSlicesSupported; }
    uint32_t getMaxDualSubSlicesSupported() const { return maxDualSubSlicesSupported; }
    uint32_t getMaxEuPerDualSubSlice() const { return maxEuPerDualSubSlice; }
    uint32_t getMaxMemoryChannels() const { return maxMemoryChannels; }
    uint32_t getMemoryType() const { return memoryType; }
    uint32_t getNumThreadsPerEu() const { return numThreadsPerEu; }
    uint32_t getMaxRCS() const { return maxRCS; }
    uint32_t getMaxCCS() const { return maxCCS; }
    uint32_t getCsrSizeInMb() const { return csrSizeInMb; }
    uint32_t getL3BankSizeInKb() const { return l3BankSizeInKb; }
    uint32_t getSlmSizePerDss() const { return slmSizePerDss; }
    uint32_t getNumHbmStacksPerTile() const { return numHbmStacksPerTile; }
    uint32_t getNumChannelsPerHbmStack() const { return numChannelsPerHbmStack; }
    uint32_t getNumRegions() const { return numRegions; }
    uint32_t getNumL3BanksPerGroup() const { return numL3BanksPerGroup; }
    uint32_t getNumL3BankGroups() const { return numL3BankGroups; }

    void checkSysInfoMismatch(HardwareInfo *hwInfo);

  protected:
    void parseDeviceBlob(const std::vector<uint32_t> &inputData);

    uint32_t maxSlicesSupported = 0;
    uint32_t maxDualSubSlicesSupported = 0;
    uint32_t maxEuPerDualSubSlice = 0;
    uint32_t memoryType = 0;
    uint32_t maxMemoryChannels = 0;
    uint32_t numThreadsPerEu = 0;
    uint32_t maxRCS = 0;
    uint32_t maxCCS = 0;
    uint32_t l3BankSizeInKb = 0;
    uint32_t slmSizePerDss = 0;
    uint32_t csrSizeInMb = 0;
    uint32_t numHbmStacksPerTile = 0;
    uint32_t numChannelsPerHbmStack = 0;
    uint32_t numRegions = 0;
    uint32_t numL3BanksPerGroup = 0;
    uint32_t numL3BankGroups = 0;
};

} // namespace NEO
