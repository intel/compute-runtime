/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/string.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"
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

class PowerSysfsAccess : public SysfsAccess {};

template <>
struct Mock<PowerSysfsAccess> : public PowerSysfsAccess {

    ze_result_t getValStringHelper(const std::string file, std::string &val);
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

    ze_result_t getValUnsignedLongHelper(const std::string file, uint64_t &val);
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

    ze_result_t getValUnsignedInt(const std::string file, uint32_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
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

    ze_result_t getValUnsignedIntMax(const std::string file, uint32_t &val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (file.compare(i915HwmonDir + "/" + maxPowerLimit) == 0) {
            val = std::numeric_limits<uint32_t>::max();
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t setVal(const std::string file, const int val) {
        ze_result_t result = ZE_RESULT_SUCCESS;
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
    ze_result_t getscanDirEntries(const std::string file, std::vector<std::string> &listOfEntries) {
        if (file.compare(hwmonDir) == 0) {
            listOfEntries = listOfMockedHwmonDirs;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock<PowerSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    MOCK_METHOD(ze_result_t, write, (const std::string file, const int val), (override));
    MOCK_METHOD(ze_result_t, scanDirEntries, (const std::string file, std::vector<std::string> &listOfEntries), (override));
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
        std::string rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
        if (ZE_RESULT_SUCCESS != PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, rootPciPathOfGpuDevice)) {
            return;
        }

        telemetryDeviceEntry = "/sys/class/intel_pmt/telem2/telem";
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
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::getRealPathSuccess));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint32_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedInt));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setVal));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntries));
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

        pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
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
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<PowerFsAccess>::getRealPathSuccess));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<std::string &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValString));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint64_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedLong));
        ON_CALL(*pSysfsAccess.get(), read(_, Matcher<uint32_t &>(_)))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getValUnsignedInt));
        ON_CALL(*pSysfsAccess.get(), write(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::setVal));
        ON_CALL(*pSysfsAccess.get(), scanDirEntries(_, _))
            .WillByDefault(::testing::Invoke(pSysfsAccess.get(), &Mock<PowerSysfsAccess>::getscanDirEntries));
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

        pSysmanDeviceImp->pPowerHandleContext->init(deviceHandles, device->toHandle());
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        for (const auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
            delete pmtMapElement.second;
        }
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = mapOriginal;
    }

    std::vector<zes_pwr_handle_t> getPowerHandles(uint32_t count) {
        std::vector<zes_pwr_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumPowerDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace L0
