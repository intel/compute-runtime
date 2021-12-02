/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/temperature/linux/mock_sysfs_temperature.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {
const static int fakeFileDescriptor = 123;
std::string rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
constexpr uint32_t handleComponentCountForSubDevices = 6u;
constexpr uint32_t handleComponentCountForNoSubDevices = 2u;
constexpr uint32_t invalidMaxTemperature = 125;
constexpr uint32_t invalidMinTemperature = 10;

const std::map<std::string, uint64_t> deviceKeyOffsetMapTemperature = {
    {"PACKAGE_ENERGY", 0x420},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

inline static int openMockTemp(const char *pathname, int flags) {
    if (strcmp(pathname, "/sys/class/intel_pmt/telem2/telem") == 0) {
        return fakeFileDescriptor;
    }
    if (strcmp(pathname, "/sys/class/intel_pmt/telem3/telem") == 0) {
        return fakeFileDescriptor;
    }
    return -1;
}

inline static int closeMockTemp(int fd) {
    if (fd == fakeFileDescriptor) {
        return 0;
    }
    return -1;
}

ssize_t preadMockTemp(int fd, void *buf, size_t count, off_t offset) {
    if (count == sizeof(uint32_t)) {
        uint32_t *mockBuf = static_cast<uint32_t *>(buf);
        if (offset == memory2MaxTempIndex) {
            *mockBuf = memory2MaxTemperature;
        } else if (offset == memory3MaxTempIndex) {
            *mockBuf = memory3MaxTemperature;
        } else {
            for (uint64_t i = 0; i < sizeof(uint32_t); i++) {
                *mockBuf |= (uint32_t)tempArrForSubDevices[(offset - offsetForSubDevices) + i] << (i * 8);
            }
        }
    } else {
        uint64_t *mockBuf = static_cast<uint64_t *>(buf);
        *mockBuf = 0;
        for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
            *mockBuf |= (uint64_t)tempArrForSubDevices[(offset - offsetForSubDevices) + i] << (i * 8);
        }
    }
    return count;
}

ssize_t preadMockTempNoSubDevices(int fd, void *buf, size_t count, off_t offset) {
    if (count == sizeof(uint32_t)) {
        uint32_t *mockBuf = static_cast<uint32_t *>(buf);
        for (uint64_t i = 0; i < sizeof(uint32_t); i++) {
            *mockBuf |= (uint32_t)tempArrForNoSubDevices[(offset - offsetForNoSubDevices) + i] << (i * 8);
        }
    } else {
        uint64_t *mockBuf = static_cast<uint64_t *>(buf);
        *mockBuf = 0;
        for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
            *mockBuf |= (uint64_t)tempArrForNoSubDevices[(offset - offsetForNoSubDevices) + i] << (i * 8);
        }
    }
    return count;
}

class SysmanMultiDeviceTemperatureFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxTemperatureImp> pPublicLinuxTemperatureImp;
    std::unique_ptr<Mock<TemperatureFsAccess>> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<TemperatureFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::getRealPathSuccess));

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
            auto pPmt = new NiceMock<Mock<TemperaturePmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                           deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            pPmt->keyOffsetMap = deviceKeyOffsetMapTemperature;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }

        pSysmanDeviceImp->pTempHandleContext->init(deviceHandles);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
    }

    std::vector<zes_temp_handle_t> get_temp_handles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenComponentCountZeroWhenCallingZetSysmanTemperatureGetThenZeroCountIsReturnedAndVerifySysmanTemperatureGetCallSucceeds) {
    uint32_t count = 0;
    ze_result_t result = zesDeviceEnumTemperatureSensors(device->toHandle(), &count, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(count, handleComponentCountForSubDevices);

    uint32_t testcount = count + 1;
    result = zesDeviceEnumTemperatureSensors(device->toHandle(), &testcount, NULL);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testcount, handleComponentCountForSubDevices);

    count = 0;
    std::vector<zes_temp_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCountForSubDevices);
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = get_temp_handles(handleComponentCountForSubDevices);
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = static_cast<NiceMock<Mock<TemperaturePmt>> *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(deviceProperties.subdeviceId));
        pPmt->openFunction = openMockTemp;
        pPmt->closeFunction = closeMockTemp;
        pPmt->preadFunction = preadMockTemp;
    }

    for (auto handle : handles) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = get_temp_handles(handleComponentCountForSubDevices);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = get_temp_handles(handleComponentCountForSubDevices);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenCreatePmtObjectsWhenRootTileIndexEnumeratesSuccessfulThenValidatePmtObjectsReceivedAndBranches) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject);
    uint32_t deviceHandlesIndex = 0;
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandles[deviceHandlesIndex++])->getProperties(&deviceProperties);
        EXPECT_NE(subDeviceIdToPmtEntry.second, nullptr);
        EXPECT_EQ(subDeviceIdToPmtEntry.first, deviceProperties.subdeviceId);
        delete subDeviceIdToPmtEntry.second; // delete memory to avoid mem leak here, as we finished our test validation just above.
    }
}

class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxTemperatureImp> pPublicLinuxTemperatureImp;
    std::unique_ptr<Mock<TemperatureFsAccess>> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<TemperatureFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        ON_CALL(*pFsAccess.get(), listDirectory(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::listDirectorySuccess));
        ON_CALL(*pFsAccess.get(), getRealPath(_, _))
            .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::getRealPathSuccess));

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
            auto pPmt = new NiceMock<Mock<TemperaturePmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                           deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            pPmt->keyOffsetMap = deviceKeyOffsetMapTemperature;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }

        pSysmanDeviceImp->pTempHandleContext->init(deviceHandles);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::TearDown();
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
    }

    std::vector<zes_temp_handle_t> get_temp_handles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUAndGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = get_temp_handles(handleComponentCountForNoSubDevices);
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = static_cast<NiceMock<Mock<TemperaturePmt>> *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(deviceProperties.subdeviceId));
        pPmt->openFunction = openMockTemp;
        pPmt->closeFunction = closeMockTemp;
        pPmt->preadFunction = preadMockTempNoSubDevices;
    }

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

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleAndPmtReadValueFailsWhenGettingTemperatureThenFailureReturned) {
    // delete previously allocated pPmt objects
    for (auto &subDeviceIdToPmtEntry : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        delete subDeviceIdToPmtEntry.second;
    }
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    // delete previously created temp handles
    for (auto &handle : pSysmanDeviceImp->pTempHandleContext->handleList) {
        delete handle;
        handle = nullptr;
        pSysmanDeviceImp->pTempHandleContext->handleList.pop_back();
    }
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new NiceMock<Mock<TemperaturePmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                       deviceProperties.subdeviceId);
        pPmt->mockedInit(pFsAccess.get());
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
    }

    pSysmanDeviceImp->pTempHandleContext->init(deviceHandles);
    auto handles = get_temp_handles(handleComponentCountForNoSubDevices);
    for (auto &handle : handles) {
        double temperature;
        ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
    }
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingUnsupportedSensorsTemperatureThenUnsupportedReturned) {
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(device->toHandle())->getProperties(&deviceProperties);
    auto pPublicLinuxTemperatureImp = std::make_unique<LinuxTemperatureImp>(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                                            deviceProperties.subdeviceId);
    pPublicLinuxTemperatureImp->setSensorType(ZES_TEMP_SENSORS_MEMORY_MIN);
    double temperature;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPublicLinuxTemperatureImp->getSensorTemperature(&temperature));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidateEnumerateRootTelemIndexWhengetRealPathFailsThenFailureReturned) {
    ON_CALL(*pFsAccess.get(), getRealPath(_, _))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::getRealPathFailure));
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE,
              PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess.get(), rootPciPathOfGpuDevice));

    ON_CALL(*pFsAccess.get(), listDirectory(_, _))
        .WillByDefault(::testing::Invoke(pFsAccess.get(), &Mock<TemperatureFsAccess>::listDirectoryFailure));
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE,
              PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess.get(), rootPciPathOfGpuDevice));

    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject);
    EXPECT_TRUE(mapOfSubDeviceIdToPmtObject.empty());
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidatePmtReadValueWhenkeyOffsetMapIsNotThereThenFailureReturned) {
    auto pPmt = std::make_unique<NiceMock<Mock<TemperaturePmt>>>(pFsAccess.get(), 0, 0);
    pPmt->mockedInit(pFsAccess.get());
    pPmt->keyOffsetMap = deviceKeyOffsetMapTemperature;
    uint32_t val = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPmt->readValue("SOMETHING", val));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenCreatePmtObjectsWhenRootTileIndexEnumeratesSuccessfulThenValidatePmtObjectsReceivedAndBranches) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject1;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject1);
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject1) {
        EXPECT_NE(subDeviceIdToPmtEntry.second, nullptr);
        EXPECT_EQ(subDeviceIdToPmtEntry.first, 0u); // We know that subdeviceID is zero as core device didnt have any subdevices
        delete subDeviceIdToPmtEntry.second;        // delete memory to avoid mem leak here, as we finished our test validation just above.
    }

    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject2;
    std::vector<ze_device_handle_t> testHandleVector;
    // If empty device handle vector is provided then empty map is retrieved
    PlatformMonitoringTech::create(testHandleVector, pFsAccess.get(), rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject2);
    EXPECT_TRUE(mapOfSubDeviceIdToPmtObject2.empty());
}

} // namespace ult
} // namespace L0
