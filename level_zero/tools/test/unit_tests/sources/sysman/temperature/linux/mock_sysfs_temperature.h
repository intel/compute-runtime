/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "sysman/linux/pmt.h"
#include "sysman/temperature/temperature_imp.h"

namespace L0 {
namespace ult {
constexpr uint8_t computeIndex = 9;
constexpr uint8_t globalIndex = 3;
constexpr uint8_t tempArr[16] = {0x12, 0x23, 0x43, 0xde, 0xa3, 0xce, 0x23, 0x11, 0x45, 0x32, 0x67, 0x47, 0xac, 0x21, 0x03, 0x90};
constexpr uint64_t offsetCompute = 0x60;
constexpr uint64_t mappedLength = 256;
class TemperaturePmt : public PlatformMonitoringTech {
  public:
    using PlatformMonitoringTech::mappedMemory;
};

template <>
struct Mock<TemperaturePmt> : public TemperaturePmt {
    Mock() = default;
    ~Mock() override {
        if (mappedMemory != nullptr) {
            delete mappedMemory;
            mappedMemory = nullptr;
        }
    }

    void setVal(bool val) {
        isPmtSupported = val;
    }

    void init(const std::string &deviceName, FsAccess *pFsAccess) override {
        mappedMemory = new char[mappedLength];
        for (uint64_t i = 0; i < sizeof(tempArr) / sizeof(uint8_t); i++) {
            mappedMemory[offsetCompute + i] = tempArr[i];
        }
        pmtSupported = isPmtSupported;
    }

  private:
    bool isPmtSupported = false;
};

class PublicLinuxTemperatureImp : public L0::LinuxTemperatureImp {
  public:
    using LinuxTemperatureImp::pPmt;
    using LinuxTemperatureImp::type;
};
} // namespace ult
} // namespace L0