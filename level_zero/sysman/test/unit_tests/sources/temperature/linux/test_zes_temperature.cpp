/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/temperature/linux/mock_sysfs_temperature.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t handleComponentCountForTwoTileDevices = 6u;
constexpr uint32_t handleComponentCountForSingleTileDevice = 3u;
constexpr uint32_t handleComponentCountForNoSubDevices = 2u;
constexpr uint32_t invalidMaxTemperature = 125;
constexpr uint32_t invalidMinTemperature = 10;

class SysmanMultiDeviceTemperatureFixture : public SysmanMultiDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    void SetUp() override {
        SysmanMultiDeviceFixture::SetUp();
        device = pSysmanDevice;
        pSysmanDeviceImp->pTempHandleContext->handleList.clear();
    }
    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

static int mockReadLinkMultiTelemetryNodesSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1},
        {sysfsPathTelem2, realPathTelem2},
        {sysfsPathTelem3, realPathTelem3},
        {sysfsPathTelem4, realPathTelem4},
        {sysfsPathTelem5, realPathTelem5},
        {sysfsPathTelem6, realPathTelem6},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockReadLinkSingleTelemetryNodesSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {sysfsPathTelem1, realPathTelem1},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName || strPathName == telem2OffsetFileName || strPathName == telem3OffsetFileName || strPathName == telem5OffsetFileName || strPathName == telem6OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName || strPathName == telem2GuidFileName || strPathName == telem3GuidFileName || strPathName == telem5GuidFileName || strPathName == telem6GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName || strPathName == telem2TelemFileName || strPathName == telem3TelemFileName || strPathName == telem5TelemFileName || strPathName == telem6TelemFileName) {
        returnValue = 6;
    }
    return returnValue;
}

static ssize_t mockReadSuccessPvc(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 5) {
        oStream << "0xb15a0ede";
    } else if (fd == 6) {
        if (offset == offsetTileMaxTemperature) {
            val = tileMaxTemperature;
        } else if (offset == offsetGtMaxTemperature) {
            val = gtMaxTemperature;
        } else if (offset == offsetHbm0MaxDeviceTemperature) {
            val = memory0MaxTemperature;
        } else if (offset == offsetHbm1MaxDeviceTemperature) {
            val = memory1MaxTemperature;
        } else if (offset == offsetHbm2MaxDeviceTemperature) {
            val = memory2MaxTemperature;
        } else if (offset == offsetHbm3MaxDeviceTemperature) {
            val = memory3MaxTemperature;
        }
        memcpy(buf, &val, count);
        return count;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadSuccessDg1(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t intVal = 0;
    uint64_t longVal = 0;
    if (fd == 4) {
        memcpy(buf, &intVal, count);
        return count;
    } else if (fd == 5) {
        oStream << "0x490e";
    } else if (fd == 6) {
        if (offset == offsetComputeTemperatures) {
            for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                intVal |= (uint32_t)tempArrForNoSubDevices[(computeTempIndex) + i] << (i * 8);
            }
            memcpy(buf, &intVal, sizeof(intVal));
            return sizeof(intVal);
        } else if (offset == offsetCoreTemperatures) {
            for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
                intVal |= (uint32_t)tempArrForNoSubDevices[(coreTempIndex) + i] << (i * 8);
            }
            memcpy(buf, &intVal, sizeof(intVal));
            return sizeof(intVal);
        } else if (offset == offsetSocTemperatures1) {
            for (uint8_t i = 0; i < sizeof(uint64_t); i++) {
                longVal |= (uint64_t)tempArrForNoSubDevices[(socTempIndex) + i] << (i * 8);
            }
            memcpy(buf, &longVal, sizeof(longVal));
            return sizeof(longVal);
        }
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadSuccessDg2(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint64_t val = 0;
    if (fd == 4) {
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 5) {
        oStream << "0x4f9502";
    } else if (fd == 6) {
        if (offset == offsetSocTemperatures2) {
            for (uint8_t i = 0; i < sizeof(uint64_t); i++) {
                val |= (uint64_t)tempArrForNoSubDevices[(socTempIndex) + i] << (i * 8);
            }
        }
        memcpy(buf, &val, sizeof(val));
        return sizeof(val);
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

inline static int mockStatSuccess(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IWUSR | S_IRUSR | S_IFREG;
    return 0;
}

HWTEST2_F(SysmanMultiDeviceTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleComponentCountForTwoTileDevices);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, handleComponentCountForTwoTileDevices);

    count = 0;
    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCountForTwoTileDevices);
}

HWTEST2_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureThenValidTemperatureReadingsRetrieved, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForTwoTileDevices);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(tileMaxTemperature));
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(gtMaxTemperature));
        }
        if (properties.type == ZES_TEMP_SENSORS_MEMORY) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(std::max({memory0MaxTemperature, memory1MaxTemperature, memory2MaxTemperature, memory3MaxTemperature})));
        }
    }
}

HWTEST2_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForTwoTileDevices);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

HWTEST2_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForTwoTileDevices);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanDeviceTemperatureFixture, GivenValidPowerHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumPowerHandleSucceeds, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCountForSingleTileDevice);

    pSysmanDeviceImp->pTempHandleContext->handleList.clear();

    pLinuxSysmanImp->reInitSysmanDeviceResources();

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCountForSingleTileDevice);
}

HWTEST2_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUAndGlobalTemperatureThenValidTemperatureReadingsRetrieved, IsDG1) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessDg1);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForNoSubDevices);
    for (auto &handle : handles) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
        if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            uint8_t maxTemp = 0;
            for (uint64_t i = 0; i < sizeof(tempArrForNoSubDevices) / sizeof(uint8_t); i++) {
                if ((tempArrForNoSubDevices[i] > invalidMaxTemperature) ||
                    (tempArrForNoSubDevices[i] < invalidMinTemperature) || (maxTemp > tempArrForNoSubDevices[i])) {
                    continue;
                }
                maxTemp = tempArrForNoSubDevices[i];
            }
            EXPECT_EQ(temperature, static_cast<double>(maxTemp));
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(temperature, static_cast<double>(tempArrForNoSubDevices[computeIndexForNoSubDevices]));
        }
    }
}

HWTEST2_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUAndGlobalTemperatureThenValidTemperatureReadingsRetrieved, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessDg2);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForNoSubDevices);
    for (auto &handle : handles) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
        if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            uint8_t maxTemp = 0;
            for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
                if ((tempArrForNoSubDevices[i] > invalidMaxTemperature) ||
                    (tempArrForNoSubDevices[i] < invalidMinTemperature) || (maxTemp > tempArrForNoSubDevices[i])) {
                    continue;
                }
                maxTemp = tempArrForNoSubDevices[i];
            }
            EXPECT_EQ(temperature, static_cast<double>(maxTemp));
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            EXPECT_EQ(temperature, static_cast<double>(tempArrForNoSubDevices[gtTempIndexForNoSubDevices]));
        }
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    auto pPublicLinuxTemperatureImp = std::make_unique<L0::Sysman::LinuxTemperatureImp>(pOsSysman, onSubdevice, subdeviceId);
    pPublicLinuxTemperatureImp->setSensorType(ZES_TEMP_SENSORS_MEMORY_MIN);
    double temperature;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPublicLinuxTemperatureImp->getSensorTemperature(&temperature));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTempSensorsMemoryMinSupportThenFalseIsReturned) {
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    ze_bool_t onSubdevice = (subDeviceCount == 0) ? false : true;
    uint32_t subdeviceId = 0;

    auto pPublicLinuxTemperatureImp = std::make_unique<L0::Sysman::LinuxTemperatureImp>(pOsSysman, onSubdevice, subdeviceId);
    pPublicLinuxTemperatureImp->setSensorType(ZES_TEMP_SENSORS_MEMORY_MIN);
    EXPECT_EQ(false, pPublicLinuxTemperatureImp->isTempModuleSupported());
}

HWTEST2_F(SysmanDeviceTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleComponentCountForSingleTileDevice);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, handleComponentCountForSingleTileDevice);

    count = 0;
    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCountForSingleTileDevice);
}

HWTEST2_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureThenValidTemperatureReadingsRetrieved, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkMultiTelemetryNodesSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccessPvc);
    VariableBackup<decltype(NEO::SysCalls::sysCallsStat)> mockStat(&NEO::SysCalls::sysCallsStat, &mockStatSuccess);
    VariableBackup<bool> allowFakeDevicePathBackup(&NEO::SysCalls::allowFakeDevicePath, true);

    auto handles = getTempHandles(handleComponentCountForSingleTileDevice);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(tileMaxTemperature));
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(gtMaxTemperature));
        }
        if (properties.type == ZES_TEMP_SENSORS_MEMORY) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(std::max({memory0MaxTemperature, memory1MaxTemperature, memory2MaxTemperature, memory3MaxTemperature})));
        }
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
