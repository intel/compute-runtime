/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/temperature/linux/sysman_os_temperature_imp.h"
#include "level_zero/sysman/source/api/temperature/sysman_temperature_imp.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t offsetComputeTemperatures = 104;
constexpr uint32_t offsetCoreTemperatures = 108;
constexpr uint32_t offsetSocTemperatures1 = 96;
constexpr uint32_t offsetSocTemperatures2 = 56;

constexpr uint32_t offsetTileMaxTemperature = 44;
constexpr uint32_t offsetGtMaxTemperature = 52;
constexpr uint32_t offsetHbm0MaxDeviceTemperature = 28;
constexpr uint32_t offsetHbm1MaxDeviceTemperature = 36;
constexpr uint32_t offsetHbm2MaxDeviceTemperature = 300;
constexpr uint32_t offsetHbm3MaxDeviceTemperature = 308;

constexpr uint8_t memory0MaxTemperature = 0x12;
constexpr uint8_t memory1MaxTemperature = 0x45;
constexpr uint8_t memory2MaxTemperature = 0x32;
constexpr uint8_t memory3MaxTemperature = 0x36;
constexpr uint32_t gtMaxTemperature = 0x1d;
constexpr uint32_t tileMaxTemperature = 0x34;

constexpr uint8_t computeTempIndex = 8;
constexpr uint8_t coreTempIndex = 12;
constexpr uint8_t socTempIndex = 0;
constexpr uint8_t tempArrForNoSubDevices[19] = {0x09, 0x23, 0x43, 0xde, 0xa3, 0xce, 0x23, 0x11, 0x45, 0x32, 0x67, 0x47, 0xac, 0x21, 0x03, 0x90, 0, 0, 0};
constexpr uint8_t computeIndexForNoSubDevices = 9;
constexpr uint8_t gtTempIndexForNoSubDevices = 0;
const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string realPathTelem1 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
const std::string realPathTelem2 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
const std::string realPathTelem3 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem3";
const std::string realPathTelem4 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem4";
const std::string realPathTelem5 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem5";
const std::string realPathTelem6 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem6";
const std::string sysfsPathTelem1 = "/sys/class/intel_pmt/telem1";
const std::string sysfsPathTelem2 = "/sys/class/intel_pmt/telem2";
const std::string sysfsPathTelem3 = "/sys/class/intel_pmt/telem3";
const std::string sysfsPathTelem4 = "/sys/class/intel_pmt/telem4";
const std::string sysfsPathTelem5 = "/sys/class/intel_pmt/telem5";
const std::string sysfsPathTelem6 = "/sys/class/intel_pmt/telem6";
const std::string telem1OffsetFileName("/sys/class/intel_pmt/telem1/offset");
const std::string telem1GuidFileName("/sys/class/intel_pmt/telem1/guid");
const std::string telem1TelemFileName("/sys/class/intel_pmt/telem1/telem");
const std::string telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");
const std::string telem3OffsetFileName("/sys/class/intel_pmt/telem3/offset");
const std::string telem3GuidFileName("/sys/class/intel_pmt/telem3/guid");
const std::string telem3TelemFileName("/sys/class/intel_pmt/telem3/telem");
const std::string telem4OffsetFileName("/sys/class/intel_pmt/telem4/offset");
const std::string telem4GuidFileName("/sys/class/intel_pmt/telem4/guid");
const std::string telem4TelemFileName("/sys/class/intel_pmt/telem4/telem");
const std::string telem5OffsetFileName("/sys/class/intel_pmt/telem5/offset");
const std::string telem5GuidFileName("/sys/class/intel_pmt/telem5/guid");
const std::string telem5TelemFileName("/sys/class/intel_pmt/telem5/telem");
const std::string telem6OffsetFileName("/sys/class/intel_pmt/telem6/offset");
const std::string telem6GuidFileName("/sys/class/intel_pmt/telem6/guid");
const std::string telem6TelemFileName("/sys/class/intel_pmt/telem6/telem");

class PublicLinuxTemperatureImp : public L0::Sysman::LinuxTemperatureImp {
  public:
    PublicLinuxTemperatureImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxTemperatureImp(pOsSysman, onSubdevice, subdeviceId) {}
};

} // namespace ult
} // namespace Sysman
} // namespace L0
