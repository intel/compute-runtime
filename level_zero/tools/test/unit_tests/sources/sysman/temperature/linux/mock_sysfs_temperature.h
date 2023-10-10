/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"
#include "level_zero/tools/source/sysman/temperature/temperature_imp.h"

namespace L0 {
namespace ult {

constexpr uint8_t memory0MaxTemperature = 0x12;
constexpr uint8_t memory1MaxTemperature = 0x45;
constexpr uint8_t memory2MaxTemperature = 0x32;
constexpr uint8_t memory3MaxTemperature = 0x36;
constexpr uint32_t gtMaxTemperature = 0x1d;
constexpr uint32_t tileMaxTemperature = 0x34;

constexpr uint8_t computeTempIndex = 8;
constexpr uint8_t coreTempIndex = 12;
constexpr uint8_t socTempIndex = 0;
constexpr uint8_t tempArrForNoSubDevices[19] = {0x12, 0x23, 0x43, 0xde, 0xa3, 0xce, 0x23, 0x11, 0x45, 0x32, 0x67, 0x47, 0xac, 0x21, 0x03, 0x90, 0, 0, 0};
constexpr uint8_t computeIndexForNoSubDevices = 9;
constexpr uint8_t gtTempIndexForNoSubDevices = 0;
const std::string baseTelemSysFS("/sys/class/intel_pmt");
std::string gpuUpstreamPortPathInTemperature = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
const std::string realPathTelem1 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
const std::string realPathTelem2 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
const std::string realPathTelem3 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem3";
const std::string realPathTelem4 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem4";
const std::string realPathTelem5 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem5";
const std::string sysfsPahTelem1 = "/sys/class/intel_pmt/telem1";
const std::string sysfsPahTelem2 = "/sys/class/intel_pmt/telem2";
const std::string sysfsPahTelem3 = "/sys/class/intel_pmt/telem3";
const std::string sysfsPahTelem4 = "/sys/class/intel_pmt/telem4";
const std::string sysfsPahTelem5 = "/sys/class/intel_pmt/telem5";

struct MockTemperaturePmt : public PlatformMonitoringTech {
    MockTemperaturePmt(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::preadFunction;
    using PlatformMonitoringTech::telemetryDeviceEntry;

    ze_result_t mockReadValueResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadCoreTempResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadComputeTempResult = ZE_RESULT_SUCCESS;

    ~MockTemperaturePmt() override {
        rootDeviceTelemNodeIndex = 0;
    }

    void mockedInit(FsAccess *pFsAccess) {
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, gpuUpstreamPortPathInTemperature)) {
            return;
        }
        telemetryDeviceEntry = "/sys/class/intel_pmt/telem2/telem";
    }

    ze_result_t readValue(const std::string key, uint32_t &val) override {

        if (mockReadValueResult != ZE_RESULT_SUCCESS) {
            return mockReadValueResult;
        }

        ze_result_t result = ZE_RESULT_SUCCESS;
        if (key.compare("HBM0MaxDeviceTemperature") == 0) {
            val = memory0MaxTemperature;
        } else if (key.compare("HBM1MaxDeviceTemperature") == 0) {
            val = memory1MaxTemperature;
        } else if (key.compare("HBM2MaxDeviceTemperature") == 0) {
            val = memory2MaxTemperature;
        } else if (key.compare("HBM3MaxDeviceTemperature") == 0) {
            val = memory3MaxTemperature;
        } else if (key.compare("GTMaxTemperature") == 0) {
            val = gtMaxTemperature;
        } else if (key.compare("TileMaxTemperature") == 0) {
            val = tileMaxTemperature;
        } else if (key.compare("COMPUTE_TEMPERATURES") == 0) {
            if (mockReadComputeTempResult != ZE_RESULT_SUCCESS) {
                return mockReadComputeTempResult;
            }
            val = 0;
            for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                val |= (uint32_t)tempArrForNoSubDevices[(computeTempIndex) + i] << (i * 8);
            }
        } else if (key.compare("CORE_TEMPERATURES") == 0) {
            if (mockReadCoreTempResult != ZE_RESULT_SUCCESS) {
                return mockReadCoreTempResult;
            }
            val = 0;
            for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                val |= (uint32_t)tempArrForNoSubDevices[(coreTempIndex) + i] << (i * 8);
            }
        } else {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        return result;
    }

    ze_result_t readValue(const std::string key, uint64_t &val) override {

        if (mockReadValueResult != ZE_RESULT_SUCCESS) {
            return mockReadValueResult;
        }

        if (key.compare("SOC_TEMPERATURES") == 0) {
            val = 0;
            for (uint8_t i = 0; i < sizeof(uint64_t); i++) {
                val |= (uint64_t)tempArrForNoSubDevices[(socTempIndex) + i] << (i * 8);
            }
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }
};

struct MockTemperatureFsAccess : public FsAccess {
    ze_result_t mockErrorListDirectory = ZE_RESULT_SUCCESS;
    ze_result_t mockErrorGetRealPath = ZE_RESULT_SUCCESS;
    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &listOfTelemNodes) override {
        if (mockErrorListDirectory != ZE_RESULT_SUCCESS) {
            return mockErrorListDirectory;
        }
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

    ze_result_t getRealPath(const std::string path, std::string &buf) override {
        if (mockErrorGetRealPath != ZE_RESULT_SUCCESS) {
            return mockErrorGetRealPath;
        }
        if (path.compare(sysfsPahTelem1) == 0) {
            buf = realPathTelem1;
        } else if (path.compare(sysfsPahTelem2) == 0) {
            buf = realPathTelem2;
        } else if (path.compare(sysfsPahTelem3) == 0) {
            buf = realPathTelem3;
        } else if (path.compare(sysfsPahTelem4) == 0) {
            buf = realPathTelem4;
        } else if (path.compare(sysfsPahTelem5) == 0) {
            buf = realPathTelem5;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    MockTemperatureFsAccess() = default;
};

class PublicLinuxTemperatureImp : public L0::LinuxTemperatureImp {
  public:
    PublicLinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxTemperatureImp(pOsSysman, onSubdevice, subdeviceId) {}
    using LinuxTemperatureImp::pPmt;
    using LinuxTemperatureImp::type;
};
} // namespace ult
} // namespace L0
