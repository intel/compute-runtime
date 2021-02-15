/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

#include "sysman/linux/pmt/pmt.h"
#include "sysman/temperature/temperature_imp.h"

namespace L0 {
namespace ult {

constexpr uint8_t tempArrForSubDevices[28] = {0x12, 0, 0, 0, 0, 0, 0, 0, 0x45, 0, 0, 0, 0x6f, 0, 0, 0, 0x34, 0, 0, 0, 0x16, 0, 0, 0, 0x1d, 0, 0, 0};
constexpr uint64_t offsetForSubDevices = 28;
constexpr uint8_t memory0MaxTempIndex = 0;
constexpr uint8_t memory1MaxTempIndex = 8;
constexpr uint8_t subDeviceMinTempIndex = 12;
constexpr uint8_t subDeviceMaxTempIndex = 16;
constexpr uint8_t gtMinTempIndex = 20;
constexpr uint8_t gtMaxTempIndex = 24;

constexpr uint8_t tempArrForNoSubDevices[19] = {0x12, 0x23, 0x43, 0xde, 0xa3, 0xce, 0x23, 0x11, 0x45, 0x32, 0x67, 0x47, 0xac, 0x21, 0x03, 0x90, 0, 0, 0};
constexpr uint64_t offsetForNoSubDevices = 0x60;
constexpr uint8_t computeIndexForNoSubDevices = 9;
constexpr uint8_t globalIndexForNoSubDevices = 3;

constexpr uint64_t mappedLength = 256;
const std::string baseTelemSysFS("/sys/class/intel_pmt");
class TemperaturePmt : public PlatformMonitoringTech {
  public:
    TemperaturePmt(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::mappedMemory;
};

template <>
struct Mock<TemperaturePmt> : public TemperaturePmt {
    Mock<TemperaturePmt>(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : TemperaturePmt(pFsAccess, onSubdevice, subdeviceId) {}
    ~Mock() override {
        if (mappedMemory != nullptr) {
            delete mappedMemory;
            mappedMemory = nullptr;
        }
        rootDeviceTelemNodeIndex = 0;
    }

    void mockedInit(FsAccess *pFsAccess) {
        mappedMemory = new char[mappedLength];
        std::string rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, rootPciPathOfGpuDevice)) {
            return;
        }
        // Fill mappedMemory to validate cases when there are subdevices
        for (uint64_t i = 0; i < sizeof(tempArrForSubDevices) / sizeof(uint8_t); i++) {
            mappedMemory[offsetForSubDevices + i] = tempArrForSubDevices[i];
        }
        // Fill mappedMemory to validate cases when there are no subdevices
        for (uint64_t i = 0; i < sizeof(tempArrForNoSubDevices) / sizeof(uint8_t); i++) {
            mappedMemory[offsetForNoSubDevices + i] = tempArrForNoSubDevices[i];
        }
    }

    void mockedInitWithoutMappedMemory(FsAccess *pFsAccess) {
        std::string rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, rootPciPathOfGpuDevice)) {
            return;
        }
    }
};

class TemperatureFsAccess : public FsAccess {};

template <>
struct Mock<TemperatureFsAccess> : public TemperatureFsAccess {
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
    Mock<TemperatureFsAccess>() = default;
};

class PublicLinuxTemperatureImp : public L0::LinuxTemperatureImp {
  public:
    PublicLinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxTemperatureImp(pOsSysman, onSubdevice, subdeviceId) {}
    using LinuxTemperatureImp::pPmt;
    using LinuxTemperatureImp::type;
};
} // namespace ult
} // namespace L0