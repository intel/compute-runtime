/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/power/linux/dg1/os_power_imp.h"

#include "sysman/linux/fs_access.h"

#include <string>

namespace L0 {
namespace ult {

const std::string hwmonDir("device/hwmon");
const std::string i915HwmonDir("device/hwmon/hwmon4");
const std::string nonI915HwmonDir("device/hwmon/hwmon1");
const std::vector<std::string> listOfMockedHwmonDirs = {"hwmon1", "hwmon2", "hwmon3", "hwmon4"};
const std::string sustainedPowerLimitEnabled("power1_max_enable");
const std::string sustainedPowerLimit("power1_max");
const std::string sustainedPowerLimitInterval("power1_max_interval");
const std::string burstPowerLimitEnabled("power1_cap_enable");
const std::string burstPowerLimit("power1_cap");
const std::string energyCounterNode("energy1_input");
constexpr uint64_t expectedEnergyCounter = 123456785u;
class PowerSysfsAccess : public SysfsAccess {};

template <>
struct Mock<PowerSysfsAccess> : public PowerSysfsAccess {

    ze_result_t getValString(const std::string file, std::string &val) {
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        if (file.compare(i915HwmonDir + "/" + "name") == 0) {
            val = "i915";
            result = ZE_RESULT_SUCCESS;
        } else if (file.compare(nonI915HwmonDir + "/" + "name") == 0) {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else {
            val = "garbageI915";
            result = ZE_RESULT_SUCCESS;
        }
        return result;
    }

    uint64_t sustainedPowerLimitEnabledVal = 1u;
    uint64_t sustainedPowerLimitVal = 0;
    uint64_t sustainedPowerLimitIntervalVal = 0;
    uint64_t burstPowerLimitEnabledVal = 0;
    uint64_t burstPowerLimitVal = 0;
    uint64_t energyCounterNodeVal = expectedEnergyCounter;

    ze_result_t getValUnsignedLongReturnErrorForEnergyCounter(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + energyCounterNode) == 0) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForBurstPowerLimit(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            val = burstPowerLimitEnabledVal;
        }
        if (file.compare(i915HwmonDir + "/" + burstPowerLimit) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForBurstPowerLimitEnabled(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForSustainedPowerLimitEnabled(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnsPowerLimitEnabledAsDisabled(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            val = 0;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            val = 0;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForSustainedPower(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            val = 1;
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForSustainedPowerInterval(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            val = 1;
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValUnsignedLongReturnErrorForBurstPowerLimit(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + burstPowerLimit) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValUnsignedLongReturnErrorForBurstPowerLimitEnabled(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValReturnErrorForSustainedPower(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValReturnErrorForSustainedPowerInterval(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            val = sustainedPowerLimitEnabledVal;
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            val = sustainedPowerLimitVal;
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0) {
            val = sustainedPowerLimitIntervalVal;
        } else if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            val = burstPowerLimitEnabledVal;
        } else if (file.compare(i915HwmonDir + "/" + burstPowerLimit) == 0) {
            val = burstPowerLimitVal;
        } else if (file.compare(i915HwmonDir + "/" + energyCounterNode) == 0) {
            val = energyCounterNodeVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }
    ze_result_t setVal(const std::string file, const int val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            sustainedPowerLimitVal = static_cast<uint64_t>(val);
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitInterval) == 0) {
            sustainedPowerLimitIntervalVal = static_cast<uint64_t>(val);
        } else if (file.compare(i915HwmonDir + "/" + burstPowerLimitEnabled) == 0) {
            burstPowerLimitEnabledVal = static_cast<uint64_t>(val);
        } else if (file.compare(i915HwmonDir + "/" + burstPowerLimit) == 0) {
            burstPowerLimitVal = static_cast<uint64_t>(val);
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }
    ze_result_t getscanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) {
        if (file.compare(hwmonDir) == 0) {
            listOfEntries = listOfMockedHwmonDirs;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getscanDirEntriesStatusReturnError(const std::string file, std::vector<std::string> &listOfEntries) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock<PowerSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const int val), (override));
    MOCK_METHOD(ze_result_t, scanDirEntries, (const std::string file, std::vector<std::string> &listOfEntries), (override));
};

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    using LinuxPowerImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
