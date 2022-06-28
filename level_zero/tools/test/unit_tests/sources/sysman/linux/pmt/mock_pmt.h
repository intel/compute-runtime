/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "sysman/linux/pmt/pmt.h"

namespace L0 {
namespace ult {

const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string telem("telem");
const std::string telemNodeForSubdevice0("telem2");
const std::string telemNodeForSubdevice1("telem3");
std::string gpuUpstreamPortPathInPmt = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
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

class PmtFsAccess : public FsAccess {};

template <>
struct Mock<PmtFsAccess> : public PmtFsAccess {

    Mock<PmtFsAccess>() {
        baseTelemSysFSNodeForSubdevice0 = baseTelemSysFS + "/" + telemNodeForSubdevice0;
        baseTelemSysFSNodeForSubdevice1 = baseTelemSysFS + "/" + telemNodeForSubdevice1;
        telemetryDeviceEntryForSubdevice0 = baseTelemSysFSNodeForSubdevice0 + "/" + telem;
        telemetryDeviceEntryForSubdevice1 = baseTelemSysFSNodeForSubdevice1 + "/" + telem;
    }

    ze_result_t getValString(const std::string file, std::string &val) {
        std::string guidPathForSubdevice0 = baseTelemSysFSNodeForSubdevice0 + std::string("/guid");
        std::string guidPathForSubdevice1 = baseTelemSysFSNodeForSubdevice1 + std::string("/guid");
        if ((file.compare(guidPathForSubdevice0) == 0) ||
            (file.compare(guidPathForSubdevice1) == 0)) {
            val = "0xfdc76194";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        if ((file.compare(baseTelemSysFSNodeForSubdevice0 + std::string("/size")) == 0) ||
            (file.compare(baseTelemSysFSNodeForSubdevice1 + std::string("/size")) == 0) ||
            (file.compare(baseTelemSysFSNodeForSubdevice0 + std::string("/offset")) == 0) ||
            (file.compare(baseTelemSysFSNodeForSubdevice1 + std::string("/offset")) == 0)) {
            val = 0;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool isFileExists(const std::string file) {
        if ((file.compare(telemetryDeviceEntryForSubdevice0) == 0) ||
            (file.compare(telemetryDeviceEntryForSubdevice1) == 0)) {
            return true;
        }

        return false;
    }

    ze_result_t getRealPathSuccess(const std::string path, std::string &buf) {
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

    ze_result_t listDirectorySuccess(const std::string directory, std::vector<std::string> &listOfTelemNodes) {
        if (directory.compare(baseTelemSysFS) == 0) {
            listOfTelemNodes.push_back("crashlog2");
            listOfTelemNodes.push_back("crashlog1");
            listOfTelemNodes.push_back("telem3");
            listOfTelemNodes.push_back("telem2");
            listOfTelemNodes.push_back("telem1");
            listOfTelemNodes.push_back("telem4");
            listOfTelemNodes.push_back("telem5");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t listDirectoryNoTelemNode(const std::string directory, std::vector<std::string> &listOfTelemNodes) {
        if (directory.compare(baseTelemSysFS) == 0) {
            listOfTelemNodes.push_back("crashlog2");
            listOfTelemNodes.push_back("crashlog1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, getRealPath, (const std::string path, std::string &buf), (override));
    MOCK_METHOD(ze_result_t, listDirectory, (const std::string path, std::vector<std::string> &list), (override));
    MOCK_METHOD(bool, fileExists, (const std::string file), (override));

    std::string telemetryDeviceEntryForSubdevice0;
    std::string telemetryDeviceEntryForSubdevice1;
    std::string baseTelemSysFSNodeForSubdevice0;
    std::string baseTelemSysFSNodeForSubdevice1;
};

class PublicPlatformMonitoringTech : public L0::PlatformMonitoringTech {
  public:
    PublicPlatformMonitoringTech(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    using PlatformMonitoringTech::closeFunction;
    using PlatformMonitoringTech::doInitPmtObject;
    using PlatformMonitoringTech::init;
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::openFunction;
    using PlatformMonitoringTech::preadFunction;
    using PlatformMonitoringTech::telemetryDeviceEntry;
};

} // namespace ult
} // namespace L0
