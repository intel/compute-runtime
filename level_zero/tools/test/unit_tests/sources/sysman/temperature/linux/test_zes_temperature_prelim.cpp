/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt_xml_offsets.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/temperature/linux/mock_sysfs_temperature.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

const static int fakeFileDescriptor = 123;
constexpr uint32_t handleComponentCountForSubDevices = 6u;
constexpr uint32_t handleComponentCountForNoSubDevices = 2u;
constexpr uint32_t invalidMaxTemperature = 125;
constexpr uint32_t invalidMinTemperature = 10;
const std::string sampleGuid1 = "0xb15a0edc";
const std::string sampleGuid2 = "0x490e01";

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
    PRODUCT_FAMILY productFamily;
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOriginal;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        for (auto &handle : pSysmanDeviceImp->pTempHandleContext->handleList) {
            delete handle;
            handle = nullptr;
        }
        pSysmanDeviceImp->pTempHandleContext->handleList.clear();
        pFsAccess = std::make_unique<NiceMock<Mock<TemperatureFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

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
            auto pPmt = new NiceMock<Mock<TemperaturePmt>>(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                           deviceProperties.subdeviceId);
            pPmt->mockedInit(pFsAccess.get());
            // Get keyOffsetMap
            auto keyOffsetMapEntry = guidToKeyOffsetMap.find(sampleGuid1);
            pPmt->keyOffsetMap = keyOffsetMapEntry->second;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }

        productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
        getTempHandles(0);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        for (const auto &pmtMapElement : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
            delete pmtMapElement.second;
        }
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = mapOriginal;
        SysmanMultiDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
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
    auto handles = getTempHandles(handleComponentCountForSubDevices);
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
        if (properties.type == ZES_TEMP_SENSORS_GLOBAL) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(tempArrForSubDevices[subDeviceMaxTempIndex]));
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
            EXPECT_EQ(temperature, static_cast<double>(tempArrForSubDevices[gtMaxTempIndex]));
        }
        if (properties.type == ZES_TEMP_SENSORS_MEMORY) {
            if (productFamily == IGFX_XE_HP_SDV) {
                ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
                EXPECT_EQ(temperature, static_cast<double>(std::max({tempArrForSubDevices[memory0MaxTempIndex], tempArrForSubDevices[memory1MaxTempIndex]})));
            }
            if (productFamily == IGFX_PVC) {
                ASSERT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetState(handle, &temperature));
                EXPECT_EQ(temperature, static_cast<double>(std::max({memory0MaxTemperature, memory1MaxTemperature, memory2MaxTemperature, memory3MaxTemperature})));
            }
        }
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenGettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(handleComponentCountForSubDevices);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetConfig(handle, &config));
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleWhenSettingTemperatureConfigThenUnsupportedIsReturned) {
    auto handles = getTempHandles(handleComponentCountForSubDevices);
    for (auto handle : handles) {
        zes_temp_config_t config = {};
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureSetConfig(handle, &config));
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenCreatePmtObjectsWhenRootTileIndexEnumeratesSuccessfulThenValidatePmtObjectsReceivedAndBranches) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), gpuUpstreamPortPathInTemperature, mapOfSubDeviceIdToPmtObject);
    uint32_t deviceHandlesIndex = 0;
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandles[deviceHandlesIndex++])->getProperties(&deviceProperties);
        EXPECT_NE(subDeviceIdToPmtEntry.second, nullptr);
        EXPECT_EQ(subDeviceIdToPmtEntry.first, deviceProperties.subdeviceId);
        delete subDeviceIdToPmtEntry.second; // delete memory to avoid mem leak here, as we finished our test validation just above.
    }
}

TEST_F(SysmanMultiDeviceTemperatureFixture, GivenValidTempHandleAndPmtReadValueFailsWhenGettingTemperatureThenFailureReturned) {
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
    auto handles = getTempHandles(handleComponentCountForSubDevices);
    for (auto &handle : handles) {
        zes_temp_properties_t properties = {};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesTemperatureGetProperties(handle, &properties));
        double temperature;
        ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesTemperatureGetState(handle, &temperature));
    }
}

class SysmanDeviceTemperatureFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicLinuxTemperatureImp> pPublicLinuxTemperatureImp;
    std::unique_ptr<Mock<TemperatureFsAccess>> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    PRODUCT_FAMILY productFamily;
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pFsAccess = std::make_unique<NiceMock<Mock<TemperatureFsAccess>>>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

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
            // Get keyOffsetMap
            auto keyOffsetMapEntry = guidToKeyOffsetMap.find(sampleGuid2);
            pPmt->keyOffsetMap = keyOffsetMapEntry->second;
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }

        productFamily = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo().platform.eProductFamily;
        getTempHandles(0);
    }
    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_temp_handle_t> getTempHandles(uint32_t count) {
        std::vector<zes_temp_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumTemperatureSensors(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceTemperatureFixture, GivenValidTempHandleWhenGettingGPUAndGlobalTemperatureThenValidTemperatureReadingsRetrieved) {
    auto handles = getTempHandles(handleComponentCountForNoSubDevices);
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
            if (productFamily == IGFX_DG1) {
                EXPECT_EQ(temperature, static_cast<double>(maxTemp));
            }
        }
        if (properties.type == ZES_TEMP_SENSORS_GPU) {
            if (productFamily == IGFX_DG1) {
                EXPECT_EQ(temperature, static_cast<double>(tempArrForNoSubDevices[computeIndexForNoSubDevices]));
            }
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
    auto handles = getTempHandles(handleComponentCountForNoSubDevices);
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
    pFsAccess->mockErrorListDirectory = ZE_RESULT_ERROR_NOT_AVAILABLE;
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE,
              PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess.get(), gpuUpstreamPortPathInTemperature));

    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), gpuUpstreamPortPathInTemperature, mapOfSubDeviceIdToPmtObject);
    EXPECT_TRUE(mapOfSubDeviceIdToPmtObject.empty());
}

TEST_F(SysmanDeviceTemperatureFixture, GivenValidatePmtReadValueWhenkeyOffsetMapIsNotThereThenFailureReturned) {
    auto pPmt = std::make_unique<NiceMock<Mock<TemperaturePmt>>>(pFsAccess.get(), 0, 0);
    pPmt->mockedInit(pFsAccess.get());
    // Get keyOffsetMap
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(sampleGuid2);
    pPmt->keyOffsetMap = keyOffsetMapEntry->second;
    uint32_t val = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPmt->readValue("SOMETHING", val));
}

TEST_F(SysmanDeviceTemperatureFixture, GivenCreatePmtObjectsWhenRootTileIndexEnumeratesSuccessfulThenValidatePmtObjectsReceivedAndBranches) {
    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject1;
    PlatformMonitoringTech::create(deviceHandles, pFsAccess.get(), gpuUpstreamPortPathInTemperature, mapOfSubDeviceIdToPmtObject1);
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject1) {
        EXPECT_NE(subDeviceIdToPmtEntry.second, nullptr);
        EXPECT_EQ(subDeviceIdToPmtEntry.first, 0u); // We know that subdeviceID is zero as core device didnt have any subdevices
        delete subDeviceIdToPmtEntry.second;        // delete memory to avoid mem leak here, as we finished our test validation just above.
    }

    std::map<uint32_t, L0::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject2;
    std::vector<ze_device_handle_t> testHandleVector;
    // If empty device handle vector is provided then empty map is retrieved
    PlatformMonitoringTech::create(testHandleVector, pFsAccess.get(), gpuUpstreamPortPathInTemperature, mapOfSubDeviceIdToPmtObject2);
    EXPECT_TRUE(mapOfSubDeviceIdToPmtObject2.empty());
}

} // namespace ult
} // namespace L0
