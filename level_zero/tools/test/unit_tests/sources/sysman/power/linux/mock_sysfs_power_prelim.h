/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/power/linux/os_power_imp_prelim.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "sysman/linux/pmt/pmt.h"
#include "sysman/power/power_imp.h"
#include "sysman/sysman_imp.h"

extern bool sysmanUltsEnable;

using ::testing::DoDefault;
using ::testing::Matcher;
using ::testing::Return;

namespace L0 {
namespace ult {
constexpr uint64_t setEnergyCounter = (83456u * 1048576u);
constexpr uint64_t offset = 0x400;
const std::string deviceName("device");
const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string hwmonDir("device/hwmon");
const std::string i915HwmonDir("device/hwmon/hwmon2");
const std::string nonI915HwmonDir("device/hwmon/hwmon1");
const std::string i915HwmonDirTile0("device/hwmon/hwmon3");
const std::string i915HwmonDirTile1("device/hwmon/hwmon4");
const std::vector<std::string> listOfMockedHwmonDirs = {"hwmon0", "hwmon1", "hwmon2", "hwmon3", "hwmon4"};
const std::string sustainedPowerLimit("power1_max");
const std::string criticalPowerLimit("power1_crit");
const std::string energyCounterNode("energy1_input");
const std::string defaultPowerLimit("power1_max_default");
constexpr uint64_t expectedEnergyCounter = 123456785u;
constexpr uint64_t expectedEnergyCounterTile0 = 123456785u;
constexpr uint64_t expectedEnergyCounterTile1 = 128955785u;
constexpr uint32_t mockDefaultPowerLimitVal = 300000000;
const std::map<std::string, uint64_t> deviceKeyOffsetMapPower = {
    {"PACKAGE_ENERGY", 0x400},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

class PowerSysfsAccess : public SysfsAccess {};

template <>
struct Mock<PowerSysfsAccess> : public PowerSysfsAccess {
    ze_result_t mockReadResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadValUnsignedLongResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteResult = ZE_RESULT_SUCCESS;
    ze_result_t mockscanDirEntriesResult = ZE_RESULT_SUCCESS;

    ze_result_t getValString(const std::string file, std::string &val) {
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        if (file.compare(i915HwmonDir + "/" + "name") == 0) {
            val = "i915";
            result = ZE_RESULT_SUCCESS;
        } else if (file.compare(nonI915HwmonDir + "/" + "name") == 0) {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        } else if (file.compare(i915HwmonDirTile1 + "/" + "name") == 0) {
            val = "i915_gt1";
            result = ZE_RESULT_SUCCESS;
        } else if (file.compare(i915HwmonDirTile0 + "/" + "name") == 0) {
            val = "i915_gt0";
            result = ZE_RESULT_SUCCESS;
        } else {
            val = "garbageI915";
            result = ZE_RESULT_SUCCESS;
        }
        return result;
    }

    uint64_t sustainedPowerLimitVal = 0;
    uint64_t criticalPowerLimitVal = 0;

    ze_result_t getValUnsignedLongHelper(const std::string file, uint64_t &val);
    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            val = sustainedPowerLimitVal;
        } else if (file.compare(i915HwmonDir + "/" + criticalPowerLimit) == 0) {
            val = criticalPowerLimitVal;
        } else if (file.compare(i915HwmonDirTile0 + "/" + energyCounterNode) == 0) {
            val = expectedEnergyCounterTile0;
        } else if (file.compare(i915HwmonDirTile1 + "/" + energyCounterNode) == 0) {
            val = expectedEnergyCounterTile1;
        } else if (file.compare(i915HwmonDir + "/" + energyCounterNode) == 0) {
            val = expectedEnergyCounter;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t getValUnsignedInt(const std::string file, uint32_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + defaultPowerLimit) == 0) {
            val = mockDefaultPowerLimitVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t setVal(const std::string file, const int val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
            sustainedPowerLimitVal = static_cast<uint64_t>(val);
        } else if (file.compare(i915HwmonDir + "/" + criticalPowerLimit) == 0) {
            criticalPowerLimitVal = static_cast<uint64_t>(val);
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

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (mockReadValUnsignedLongResult != ZE_RESULT_SUCCESS) {
            return mockReadValUnsignedLongResult;
        }

        return getValUnsignedLong(file, val);
    }

    ze_result_t read(const std::string file, std::string &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }

        return getValString(file, val);
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadResult != ZE_RESULT_SUCCESS) {
            return mockReadResult;
        }
        return getValUnsignedInt(file, val);
    }

    ze_result_t write(const std::string file, const int val) override {
        if (mockWriteResult != ZE_RESULT_SUCCESS) {
            return mockWriteResult;
        }

        return setVal(file, val);
    }

    ze_result_t scanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) override {
        if (mockscanDirEntriesResult != ZE_RESULT_SUCCESS) {
            return mockscanDirEntriesResult;
        }

        return getscanDirEntries(file, listOfEntries);
    }

    Mock<PowerSysfsAccess>() = default;
};

class PowerPmt : public PlatformMonitoringTech {
  public:
    PowerPmt(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    using PlatformMonitoringTech::closeFunction;
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::openFunction;
    using PlatformMonitoringTech::preadFunction;
    using PlatformMonitoringTech::telemetryDeviceEntry;
};

template <>
struct Mock<PowerPmt> : public PowerPmt {
    ~Mock() override {
        rootDeviceTelemNodeIndex = 0;
    }

    Mock<PowerPmt>(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PowerPmt(pFsAccess, onSubdevice, subdeviceId) {}

    void mockedInit(FsAccess *pFsAccess) {
        std::string gpuUpstreamPortPath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, gpuUpstreamPortPath)) {
            return;
        }

        telemetryDeviceEntry = "/sys/class/intel_pmt/telem2/telem";
    }
};

class PowerFsAccess : public FsAccess {};

template <>
struct Mock<PowerFsAccess> : public PowerFsAccess {

    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &listOfTelemNodes) override {
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

    Mock<PowerFsAccess>() = default;
};

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId) {}
    using LinuxPowerImp::pPmt;
};

class SysmanDevicePowerFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxPowerImp> pPublicLinuxPowerImp;
    std::unique_ptr<Mock<PowerPmt>> pPmt;
    std::unique_ptr<Mock<PowerFsAccess>> pFsAccess;
    std::unique_ptr<Mock<PowerSysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    OsPower *pOsPowerOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<PowerFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<PowerSysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        for (auto &deviceHandle : deviceHandles) {
            ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
            Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
            auto pPmt = new NiceMock<Mock<PowerPmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            pPmt->keyOffsetMap = deviceKeyOffsetMapPower;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }
        getPowerHandles(0);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        SysmanDeviceFixture::TearDown();
    }
    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

class SysmanDevicePowerMultiDeviceFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxPowerImp> pPublicLinuxPowerImp;
    std::unique_ptr<Mock<PowerPmt>> pPmt;
    std::unique_ptr<Mock<PowerFsAccess>> pFsAccess;
    std::unique_ptr<Mock<PowerSysfsAccess>> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    OsPower *pOsPowerOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOriginal;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<PowerFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<NiceMock<Mock<PowerSysfsAccess>>>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();
        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        mapOriginal = pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject;
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();

        for (auto &deviceHandle : deviceHandles) {
            ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};

            Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
            auto pPmt = new NiceMock<Mock<PowerPmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                     deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            pPmt->keyOffsetMap = deviceKeyOffsetMapPower;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        for (auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
            if (pmtMapElement.second) {
                delete pmtMapElement.second;
                pmtMapElement.second = nullptr;
            }
        }
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = mapOriginal;
        SysmanMultiDeviceFixture::TearDown();
    }
    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace L0
