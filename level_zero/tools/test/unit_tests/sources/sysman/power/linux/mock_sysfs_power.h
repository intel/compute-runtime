/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

#include "sysman/linux/pmt.h"
#include "sysman/power/power_imp.h"
#include "sysman/sysman_imp.h"

namespace L0 {
namespace ult {
constexpr uint64_t setEnergyCounter = 83456;
constexpr uint64_t offset = 0x400;
constexpr uint64_t mappedLength = 2048;
const std::string deviceName("device");
FsAccess *pFsAccess = nullptr;
class PowerPmt : public PlatformMonitoringTech {};

template <>
struct Mock<PowerPmt> : public PlatformMonitoringTech {
    Mock() = default;
    ~Mock() override {
        delete mappedMemory;
        mappedMemory = nullptr;
    }

    void init(const std::string &deviceName, FsAccess *pFsAccess) override {
        mappedMemory = new char[mappedLength];
        // fill memmory with 8 bytes of data using setEnergyCoutner at offset = 0x400
        for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
            mappedMemory[offset + i] = static_cast<char>((setEnergyCounter >> 8 * i) & 0xff);
        }
        pmtSupported = true;
    }
};

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    using LinuxPowerImp::pPmt;
};
} // namespace ult
} // namespace L0
