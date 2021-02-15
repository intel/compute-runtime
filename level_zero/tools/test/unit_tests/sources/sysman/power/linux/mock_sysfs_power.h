/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

#include "sysman/linux/pmt/pmt.h"
#include "sysman/power/power_imp.h"
#include "sysman/sysman_imp.h"

namespace L0 {
namespace ult {
constexpr uint64_t setEnergyCounter = 83456;
constexpr uint64_t offset = 0x400;
constexpr uint64_t mappedLength = 2048;
const std::string deviceName("device");
const std::string baseTelemSysFS("/sys/class/intel_pmt");

class PowerPmt : public PlatformMonitoringTech {
  public:
    PowerPmt(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    using PlatformMonitoringTech::keyOffsetMap;
};

template <>
struct Mock<PowerPmt> : public PowerPmt {
    ~Mock() override {
        if (mappedMemory != nullptr) {
            delete mappedMemory;
            mappedMemory = nullptr;
        }
        rootDeviceTelemNodeIndex = 0;
    }

    Mock<PowerPmt>(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PowerPmt(pFsAccess, onSubdevice, subdeviceId) {}

    void mockedInit(FsAccess *pFsAccess) {
        mappedMemory = new char[mappedLength];
        std::string rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, rootPciPathOfGpuDevice)) {
            return;
        }

        // fill memmory with 8 bytes of data using setEnergyCoutner at offset = 0x400
        for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
            mappedMemory[offset + i] = static_cast<char>((setEnergyCounter >> 8 * i) & 0xff);
        }
    }
};

class PowerFsAccess : public FsAccess {};

template <>
struct Mock<PowerFsAccess> : public PowerFsAccess {
    ze_result_t listDirectorySuccess(const std::string directory, std::vector<std::string> &listOfTelemNodes) {
        if (directory.compare(baseTelemSysFS) == 0) {
            listOfTelemNodes.push_back("telem1");
            listOfTelemNodes.push_back("telem2");
            listOfTelemNodes.push_back("telem3");
            listOfTelemNodes.push_back("telem4");
            listOfTelemNodes.push_back("telem5");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t listDirectoryFailure(const std::string directory, std::vector<std::string> &events) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getRealPathSuccess(const std::string path, std::string &buf) {
        if (path.compare("/sys/class/intel_pmt/telem1") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:86:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
        } else if (path.compare("/sys/class/intel_pmt/telem2") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:86:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
        } else if (path.compare("/sys/class/intel_pmt/telem3") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem3";
        } else if (path.compare("/sys/class/intel_pmt/telem4") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem4";
        } else if (path.compare("/sys/class/intel_pmt/telem5") == 0) {
            buf = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem5";
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getRealPathFailure(const std::string path, std::string &buf) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MOCK_METHOD(ze_result_t, listDirectory, (const std::string path, std::vector<std::string> &list), (override));
    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &buf), (override));
    Mock<PowerFsAccess>() = default;
};

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(OsSysman *pOsSysman) : LinuxPowerImp(pOsSysman) {}
    using LinuxPowerImp::pPmt;
};
} // namespace ult
} // namespace L0
