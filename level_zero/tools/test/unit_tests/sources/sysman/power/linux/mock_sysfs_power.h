/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
#include "level_zero/tools/source/sysman/power/power_imp.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {
constexpr uint64_t setEnergyCounter = (83456u * 1048576u);
constexpr uint64_t offset = 0x400;
const std::string deviceName("device");
const std::string baseTelemSysFS("/sys/class/intel_pmt");
const std::string hwmonDir("device/hwmon");
const std::string i915HwmonDir("device/hwmon/hwmon2");
const std::string nonI915HwmonDir("device/hwmon/hwmon1");
const std::vector<std::string> listOfMockedHwmonDirs = {"hwmon0", "hwmon1", "hwmon2", "hwmon3", "hwmon4"};
const std::string sustainedPowerLimitEnabled("power1_max_enable");
const std::string sustainedPowerLimit("power1_max");
const std::string sustainedPowerLimitInterval("power1_max_interval");
const std::string burstPowerLimitEnabled("power1_cap_enable");
const std::string burstPowerLimit("power1_cap");
const std::string energyCounterNode("energy1_input");
const std::string defaultPowerLimit("power_default_limit");
const std::string minPowerLimit("power_min_limit");
const std::string maxPowerLimit("power_max_limit");
constexpr uint64_t expectedEnergyCounter = 123456785u;
constexpr uint32_t mockDefaultPowerLimitVal = 300000000;
constexpr uint32_t mockMaxPowerLimitVal = 490000000;
constexpr uint32_t mockMinPowerLimitVal = 10;

const std::map<std::string, uint64_t> deviceKeyOffsetMapPower = {
    {"PACKAGE_ENERGY", 0x400},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

struct MockPowerSysfsAccess : public SysfsAccess {

    std::vector<ze_result_t> mockReadReturnStatus{};
    std::vector<ze_result_t> mockWriteReturnStatus{};
    std::vector<ze_result_t> mockScanDirEntriesReturnStatus{};
    std::vector<uint64_t> mockReadUnsignedLongValue{};
    std::vector<uint32_t> mockReadUnsignedIntValue{};
    bool isRepeated = false;

    ze_result_t read(const std::string file, std::string &val) override {

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
            return ZE_RESULT_ERROR_NOT_AVAILABLE; // mocking the condition when user passes nullptr for sustained and peak power in zesPowerGetLimit and burst power file is absent
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLongReturnErrorForSustainedPowerLimitEnabled(const std::string file, uint64_t &val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE; // mocking the condition when user passes nullptr for burst and peak power in zesPowerGetLimit and sustained power file is absent
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

    ze_result_t setValUnsignedLongReturnErrorForSustainedPowerLimitEnabled(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValUnsignedLongReturnInsufficientForSustainedPowerLimitEnabled(const std::string file, const int val) {
        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
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

    ze_result_t read(const std::string file, uint64_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (!mockReadReturnStatus.empty()) {
            result = mockReadReturnStatus.front();
            if (!mockReadUnsignedLongValue.empty()) {
                val = mockReadUnsignedLongValue.front();
            }
            if (isRepeated != true) {
                if (mockReadUnsignedLongValue.size() != 0) {
                    mockReadUnsignedLongValue.erase(mockReadUnsignedLongValue.begin());
                }
                mockReadReturnStatus.erase(mockReadReturnStatus.begin());
            }
            return result;
        }

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

    ze_result_t read(const std::string file, uint32_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (!mockReadReturnStatus.empty()) {
            result = mockReadReturnStatus.front();
            if (!mockReadUnsignedIntValue.empty()) {
                val = mockReadUnsignedIntValue.front();
            }
            if (isRepeated != true) {
                if (mockReadUnsignedIntValue.size() != 0) {
                    mockReadUnsignedIntValue.erase(mockReadUnsignedIntValue.begin());
                }
                mockReadReturnStatus.erase(mockReadReturnStatus.begin());
            }
            return result;
        }

        if (file.compare(i915HwmonDir + "/" + defaultPowerLimit) == 0) {
            val = mockDefaultPowerLimitVal;
        } else if (file.compare(i915HwmonDir + "/" + maxPowerLimit) == 0) {
            val = mockMaxPowerLimitVal;
        } else if (file.compare(i915HwmonDir + "/" + minPowerLimit) == 0) {
            val = mockMinPowerLimitVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t write(const std::string file, const int val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (!mockWriteReturnStatus.empty()) {
            ze_result_t result = mockWriteReturnStatus.front();
            if (isRepeated != true) {
                mockWriteReturnStatus.erase(mockWriteReturnStatus.begin());
            }
            return result;
        }

        if (file.compare(i915HwmonDir + "/" + sustainedPowerLimitEnabled) == 0) {
            sustainedPowerLimitEnabledVal = static_cast<uint64_t>(val);
        } else if (file.compare(i915HwmonDir + "/" + sustainedPowerLimit) == 0) {
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

    ze_result_t scanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) override {
        ze_result_t result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        if (!mockScanDirEntriesReturnStatus.empty()) {
            ze_result_t result = mockScanDirEntriesReturnStatus.front();
            if (isRepeated != true) {
                mockScanDirEntriesReturnStatus.erase(mockScanDirEntriesReturnStatus.begin());
            }
            return result;
        }

        if (file.compare(hwmonDir) == 0) {
            listOfEntries = listOfMockedHwmonDirs;
            result = ZE_RESULT_SUCCESS;
        }
        return result;
    }

    MockPowerSysfsAccess() = default;
};

struct MockPowerPmt : public PlatformMonitoringTech {
    using PlatformMonitoringTech::keyOffsetMap;
    using PlatformMonitoringTech::preadFunction;
    using PlatformMonitoringTech::telemetryDeviceEntry;

    MockPowerPmt(FsAccess *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId) : PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId) {}
    ~MockPowerPmt() override {
        rootDeviceTelemNodeIndex = 0;
    }

    void mockedInit(FsAccess *pFsAccess) {
        std::string gpuUpstreamPortPath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, gpuUpstreamPortPath)) {
            return;
        }
        telemetryDeviceEntry = "/sys/class/intel_pmt/telem2/telem";
    }
};

struct MockPowerFsAccess : public FsAccess {

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

    ze_result_t listDirectoryFailure(const std::string directory, std::vector<std::string> &events) {
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

    ze_result_t getRealPathFailure(const std::string path, std::string &buf) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockPowerFsAccess() = default;
};

class PublicLinuxPowerImp : public L0::LinuxPowerImp {
  public:
    PublicLinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId) {}
    using LinuxPowerImp::pPmt;
};

class SysmanDevicePowerFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxPowerImp> pPublicLinuxPowerImp;
    std::unique_ptr<MockPowerPmt> pPmt;
    std::unique_ptr<MockPowerFsAccess> pFsAccess;
    std::unique_ptr<MockPowerSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    OsPower *pOsPowerOriginal = nullptr;
    std::map<uint32_t, L0::PlatformMonitoringTech *> pmtMapOriginal;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<MockPowerFsAccess>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockPowerSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        uint32_t subDeviceCount = 0;
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }

        pmtMapOriginal = pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject;
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
        for (auto &deviceHandle : deviceHandles) {
            ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
            Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
            auto pPmt = new MockPowerPmt(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
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
        pLinuxSysmanImp->releasePmtObject();
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = pmtMapOriginal;
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
    std::unique_ptr<MockPowerPmt> pPmt;
    std::unique_ptr<MockPowerFsAccess> pFsAccess;
    std::unique_ptr<MockPowerSysfsAccess> pSysfsAccess;
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
        pFsAccess = std::make_unique<MockPowerFsAccess>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockPowerSysfsAccess>();
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
            auto pPmt = new MockPowerPmt(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
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
        for (const auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
            delete pmtMapElement.second;
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
