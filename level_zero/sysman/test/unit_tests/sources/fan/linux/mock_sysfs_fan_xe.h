/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/fan/linux/sysman_os_fan_imp.h"
#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string fanHwmonDir("device/hwmon/hwmon0");
const std::string fanInputNode("fan1_input");
const std::string fanMaxNode("fan1_max");
const std::string pwmNode("pwm1");
const std::string pwmEnableNode("pwm1_enable");
const std::string pwmAutoPointTempNode0("pwm1_auto_point1_temp");
const std::string pwmAutoPointPwmNode0("pwm1_auto_point1_pwm");
const std::string pwmAutoPointTempNode1("pwm1_auto_point2_temp");
const std::string pwmAutoPointPwmNode1("pwm1_auto_point2_pwm");

constexpr int32_t mockFanRpm = 2000;
constexpr int32_t mockFanMaxRpm = 4000;
constexpr int32_t mockPwmEnableAutoStock = 2;
constexpr int32_t mockPwmEnableManual = 1;
constexpr int32_t mockPwmEnableFullSpeed = 0;
constexpr int32_t mockPwmVal = 128;
constexpr int32_t mockTempMilliDeg = 60000;

struct MockFanSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    ze_result_t mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadStringResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadInt32Result = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteInt32Result = ZE_RESULT_SUCCESS;

    bool returnWrongHwmonName = false;
    bool returnTwoEntriesFirstReadFails = false;
    bool failTempRead = false;
    bool failPwmValRead = false;
    bool allAutoPointsExist = false;
    bool failHwmonDirScan = false;
    uint32_t fanCount = 1;
    int32_t writeFailAfterCount = -1;
    int32_t writeCount = 0;

    bool fanMaxExists = true;
    bool pwmExists = true;
    bool pwmAutoPoint0Exists = true;
    bool pwmAutoPoint1Exists = true;

    int32_t fanRpmVal = mockFanRpm;
    int32_t fanMaxRpmVal = mockFanMaxRpm;
    int32_t pwmEnableVal = mockPwmEnableAutoStock;
    int32_t pwmVal0 = mockPwmVal;
    int32_t tempVal0 = mockTempMilliDeg;
    int32_t pwmVal1 = 200;
    int32_t tempVal1 = 80000;

    ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &list) override {
        if (mockScanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return mockScanDirEntriesResult;
        }
        if (path == "device/hwmon") {
            list = returnTwoEntriesFirstReadFails ? std::vector<std::string>{"hwmon_bad", "hwmon0"}
                                                  : std::vector<std::string>{"hwmon0"};
        } else if (path == fanHwmonDir) {
            if (failHwmonDirScan) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            list.clear();
            list.push_back("pwm1");
            list.push_back("fan_max");
            list.push_back("fan1_enable");
            for (uint32_t i = 1; i <= fanCount; i++) {
                list.push_back("fan" + std::to_string(i) + "_input");
            }
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadStringResult != ZE_RESULT_SUCCESS) {
            return mockReadStringResult;
        }
        // First entry deliberately fails its name read
        if (returnTwoEntriesFirstReadFails && file == "device/hwmon/hwmon_bad/name") {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        if (file == fanHwmonDir + "/name") {
            val = returnWrongHwmonName ? "not_xe" : "xe";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, int32_t &val) override {
        if (mockReadInt32Result != ZE_RESULT_SUCCESS) {
            return mockReadInt32Result;
        }
        if (file == fanHwmonDir + "/" + fanInputNode) {
            val = fanRpmVal;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + fanMaxNode) {
            val = fanMaxRpmVal;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + pwmEnableNode) {
            val = pwmEnableVal;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + pwmAutoPointTempNode0) {
            if (failTempRead) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = tempVal0;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + pwmAutoPointPwmNode0) {
            if (failPwmValRead) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = pwmVal0;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + pwmAutoPointTempNode1) {
            if (failTempRead) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = tempVal1;
            return ZE_RESULT_SUCCESS;
        }
        if (file == fanHwmonDir + "/" + pwmAutoPointPwmNode1) {
            if (failPwmValRead) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = pwmVal1;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t write(const std::string &file, const int val) override {
        if (writeFailAfterCount >= 0) {
            if (writeCount >= writeFailAfterCount) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            writeCount++;
        }
        if (mockWriteInt32Result != ZE_RESULT_SUCCESS) {
            return mockWriteInt32Result;
        }
        return ZE_RESULT_SUCCESS;
    }

    bool fileExists(const std::string file) override {
        if (file == fanHwmonDir + "/" + pwmNode) {
            return pwmExists;
        }
        // auto_point temp nodes: points 0..9 (pwm1_auto_point1_temp .. pwm1_auto_point10_temp)
        for (uint32_t p = 0; p < 10; p++) {
            const std::string node = fanHwmonDir + "/pwm1_auto_point" + std::to_string(p + 1) + "_temp";
            if (file == node) {
                if (allAutoPointsExist) {
                    return true;
                }
                return (p == 0) ? pwmAutoPoint0Exists : (p == 1) ? pwmAutoPoint1Exists
                                                                 : false;
            }
        }
        // fan[N]_input nodes
        for (uint32_t i = 1; i <= 3; i++) {
            const std::string node = fanHwmonDir + "/fan" + std::to_string(i) + "_input";
            if (file == node) {
                return false; // fan existence is now driven by scanDirEntries / fanCount
            }
        }
        return false;
    }
};

class PublicLinuxFanImp : public L0::Sysman::LinuxFanImp {
  public:
    using LinuxFanImp::hwmonDir;
    using LinuxFanImp::pSysfsAccess;
    using LinuxFanImp::pSysmanKmdInterface;
    PublicLinuxFanImp(OsSysman *pOsSysman, uint32_t fanIndex, bool multipleFansSupported)
        : LinuxFanImp(pOsSysman, fanIndex, multipleFansSupported) {}
};

class SysmanDeviceFanFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockFanSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysfsAccess = new MockFanSysfsAccess();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::unique_ptr<PublicLinuxFanImp> makeLinuxFanImp(uint32_t fanIndex = 1) {
        return std::make_unique<PublicLinuxFanImp>(pOsSysman, fanIndex, false);
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
