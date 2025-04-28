/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint64_t expectedEnergyCounter = 123456785u;
const std::string xeHwmonDir("device/hwmon/hwmon1");
const std::string cardEnergyCounterNode("energy1_input");
const std::string packageEnergyCounterNode("energy2_input");

class MockXePowerSysfsAccess : public L0::Sysman::SysFsAccessInterface {

  public:
    ze_result_t mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    std::vector<ze_result_t> mockReadValUnsignedLongResult{};

    bool isCardEnergyCounterFilePresent = true;
    bool isPackageEnergyCounterFilePresent = true;

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        if (file.compare(xeHwmonDir + "/" + "name") == 0) {
            val = "xe";
            result = ZE_RESULT_SUCCESS;
        } else {
            val = "";
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (!mockReadValUnsignedLongResult.empty()) {
            result = mockReadValUnsignedLongResult.front();
            mockReadValUnsignedLongResult.erase(mockReadValUnsignedLongResult.begin());
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
        }

        if ((file.compare(xeHwmonDir + "/" + cardEnergyCounterNode) == 0) || (file.compare(xeHwmonDir + "/" + packageEnergyCounterNode) == 0)) {
            val = expectedEnergyCounter;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t scanDirEntries(const std::string path, std::vector<std::string> &listOfEntries) override {
        const std::string hwmonDir("device/hwmon");
        if (mockScanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return mockScanDirEntriesResult;
        }
        if (path.compare(hwmonDir) == 0) {
            listOfEntries.push_back("hwmon1");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool fileExists(const std::string file) override {
        if (file.find(cardEnergyCounterNode) != std::string::npos) {
            return isCardEnergyCounterFilePresent;
        } else if (file.find(packageEnergyCounterNode) != std::string::npos) {
            return isPackageEnergyCounterFilePresent;
        }
        return false;
    }

    MockXePowerSysfsAccess() = default;
};

struct MockXePowerFsAccess : public L0::Sysman::FsAccessInterface {
    MockXePowerFsAccess() = default;
};

class PublicLinuxPowerImp : public L0::Sysman::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain) : L0::Sysman::LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId, powerDomain) {}
    using L0::Sysman::LinuxPowerImp::isTelemetrySupportAvailable;
    using L0::Sysman::LinuxPowerImp::pSysfsAccess;
};

class SysmanDevicePowerFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockXePowerFsAccess *pFsAccess = nullptr;
    MockXePowerSysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pFsAccess = new MockXePowerFsAccess();
        pSysfsAccess = new MockXePowerSysfsAccess();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pFsAccess = pFsAccess;
        pSysfsAccess->mockScanDirEntriesResult = ZE_RESULT_SUCCESS;
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0