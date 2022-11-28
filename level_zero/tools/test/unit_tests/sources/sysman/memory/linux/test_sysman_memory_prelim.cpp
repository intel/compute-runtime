/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt_xml_offsets.h"
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp_prelim.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_memory_prelim.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr int32_t memoryBusWidth = 128; // bus width in bits
constexpr int32_t numMemoryChannels = 8;
constexpr uint32_t memoryHandleComponentCount = 1u;
const std::string sampleGuid1 = "0xb15a0edc";
class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockMemorySysfsAccess> pSysfsAccess;
    std::unique_ptr<MockMemoryFsAccess> pFsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
    MockMemoryNeoDrm *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    PRODUCT_FAMILY productFamily;
    uint16_t stepping;
    std::map<uint32_t, L0::PlatformMonitoringTech *> pmtMapOriginal;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();

        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockMemorySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = new MockMemoryManagerSysman(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = false;
        device->getDriverHandle()->setMemoryManager(pMemoryManager);

        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));

        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
        pLinuxSysmanImp->pDrm = pDrm;
        pFsAccess = std::make_unique<MockMemoryFsAccess>();
        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();
        pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2e);
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20>(*pDrm));
        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
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
            auto pPmt = new MockMemoryPmt(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                          deviceProperties.subdeviceId);
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }
        auto &hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getHardwareInfo();
        productFamily = hwInfo.platform.eProductFamily;
        stepping = NEO::ProductHelper::get(productFamily)->getSteppingFromHwRevId(hwInfo);
        getMemoryHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        pLinuxSysmanImp->releasePmtObject();
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject = pmtMapOriginal;
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        if (pDrm != nullptr) {
            delete pDrm;
            pDrm = nullptr;
        }
        if (pMemoryManager != nullptr) {
            delete pMemoryManager;
            pMemoryManager = nullptr;
        }
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;

        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pMemoryHandleContext->init(deviceHandles);
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
    MemoryManager *pMemoryManagerOld;
};

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidHandlesIsReturned) {
    setLocalSupportedAndReinit(true);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidHandlesAreReturned) {
    setLocalSupportedAndReinit(false);

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesWithLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);

        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesAndQuerySystemInfoFailsThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown) {
    pDrm->mockQuerySystemInfoReturnValue.push_back(false);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown) {
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesWithHBMLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_HBM2);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesWithLPDDR4LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_LPDDR4);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_LPDDR4);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesWithLPDDR5LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_LPDDR5);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_LPDDR5);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetPropertiesWithDDRLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds) {
    pDrm->setMemoryType(INTEL_HWCONFIG_MEMORY_TYPE_GDDR6);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties;

        ze_result_t result = zesMemoryGetProperties(handle, &properties);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.numChannels, numMemoryChannels);
        EXPECT_EQ(properties.busWidth, memoryBusWidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceeds) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, probedSizeRegionOne);
        EXPECT_EQ(state.free, unallocatedSizeRegionOne);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthWhenPmtObjectIsNullThenFailureRetuned) {
    for (auto &subDeviceIdToPmtEntry : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        if (subDeviceIdToPmtEntry.second != nullptr) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthWhenVFID0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedTimestamp = 0, expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        hwInfo->platform.usRevId = NEO::ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));

        pPmt->mockVfid0Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters = vF0Hbm0ReadValue + vF0Hbm1ReadValue + vF0Hbm2ReadValue + vF0Hbm3ReadValue;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters = vF0Hbm0WriteValue + vF0Hbm1WriteValue + vF0Hbm2WriteValue + vF0Hbm3WriteValue;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedTimestamp |= vF0TimestampHValue;
        expectedTimestamp = (expectedTimestamp << 32) | vF0TimestampLValue;
        EXPECT_EQ(bandwidth.timestamp, expectedTimestamp);
        EXPECT_EQ(bandwidth.timestamp, expectedTimestamp);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        expectedBandwidth /= 8;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthWhenVFID1IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedTimestamp = 0, expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        hwInfo->platform.usRevId = NEO::ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));

        pPmt->mockVfid1Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters = vF1Hbm0ReadValue + vF1Hbm1ReadValue + vF1Hbm2ReadValue + vF1Hbm3ReadValue;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters = vF1Hbm0WriteValue + vF1Hbm1WriteValue + vF1Hbm2WriteValue + vF1Hbm3WriteValue;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedTimestamp |= vF1TimestampHValue;
        expectedTimestamp = (expectedTimestamp << 32) | vF1TimestampLValue;
        EXPECT_EQ(bandwidth.timestamp, expectedTimestamp);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        expectedBandwidth /= 8;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidUsRevIdForRevisionBWhenCallingzesSysmanMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        hwInfo->platform.usRevId = NEO::ProductHelper::get(productFamily)->getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockVfid1Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        expectedBandwidth /= 8;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformThenSuccessIsReturnedAndBandwidthIsValid) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_DG2;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (const auto &handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
        uint64_t mockMaxBwDg2 = 1343616u;
        pSysfsAccess->mockReadUInt64Value.push_back(mockMaxBwDg2);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
        EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
        expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
        EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
        expectedBandwidth = mockMaxBwDg2 * MbpsToBytesPerSecond;
        EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
        EXPECT_GT(bandwidth.timestamp, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForUnknownPlatformThenFailureIsReturned) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_UNKNOWN;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformIfIdiReadFailsTheFailureIsReturned) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_DG2;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockIdiReadValueFailureReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformAndReadingMaxBwFailsThenMaxBwIsReturnedAsZero, IsDG2) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_DG2;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (const auto &handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformIfIdiWriteFailsTheFailureIsReturned) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_DG2;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockIdiWriteFailureReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformIfDisplayVc1ReadFailsTheFailureIsReturned) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_DG2;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockDisplayVc1ReadFailureReturnStatus = ZE_RESULT_ERROR_UNKNOWN;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCForSteppingIsBAndOnSubDeviceThenHbmFrequencyShouldNotBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, REVISION_B, hbmFrequency);
    EXPECT_EQ(hbmFrequency, hbmRP0Frequency * 1000 * 1000);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCForSteppingA0ThenHbmFrequencyShouldBeNotZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, REVISION_A0, hbmFrequency);
    uint64_t expectedHbmFrequency = 3.2 * gigaUnitTransferToUnitTransfer;
    EXPECT_EQ(hbmFrequency, expectedHbmFrequency);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsXE_HP_SDVThenHbmFrequencyShouldBeNotZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(IGFX_XE_HP_SDV, REVISION_A0, hbmFrequency);
    uint64_t expectedHbmFrequency = 2.8 * gigaUnitTransferToUnitTransfer;
    EXPECT_EQ(hbmFrequency, expectedHbmFrequency);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsUnsupportedThenHbmFrequencyShouldBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(PRODUCT_FAMILY_FORCE_ULONG, REVISION_B, hbmFrequency);
    EXPECT_EQ(hbmFrequency, 0u);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCWhenSteppingIsUnknownThenHbmFrequencyShouldBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, 255, hbmFrequency);
    EXPECT_EQ(hbmFrequency, 0u);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCForSteppingIsBAndFailedToReadFrequencyThenHbmFrequencyShouldBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp(pOsSysman, true, 1);
    uint64_t hbmFrequency = 0;
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, REVISION_B, hbmFrequency);
    EXPECT_EQ(hbmFrequency, 0u);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreTrueThenErrorIsReturnedWhileGettingMemoryBandwidth) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_PVC;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreFalseThenErrorIsReturnedWhileGettingMemoryBandwidth) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_PVC;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingBandwidthAndIfPmtReadValueFailsThenErrorIsReturned) {
    setLocalSupportedAndReinit(true);
    auto hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.platform.eProductFamily = IGFX_XE_HP_SDV;
    pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironmentRef().setHwInfoAndInitHelpers(&hwInfo);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));

        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF0_VFID
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF1_VFID
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);

        if (productFamily == IGFX_XE_HP_SDV) {
            pPmt->mockReadArgumentValue.push_back(1);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF1_VFID
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF0_HBM0_READ
            EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);

            pPmt->mockReadArgumentValue.push_back(1);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF1_VFID
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0ReadValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM0_READ
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0WriteValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF0_HBM0_Write
            EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);

            pPmt->mockReadArgumentValue.push_back(1);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF1_VFID
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0ReadValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM0_READ
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0WriteValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM0_Write
            pPmt->mockReadArgumentValue.push_back(vF0Hbm1ReadValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM1_READ
            pPmt->mockReadArgumentValue.push_back(vF0Hbm1WriteValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM1_Write
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF0__TIMESTAMP_L
            EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);

            pPmt->mockReadArgumentValue.push_back(1);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF1_VFID
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0ReadValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM0_READ
            pPmt->mockReadArgumentValue.push_back(vF0Hbm0WriteValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM0_Write
            pPmt->mockReadArgumentValue.push_back(vF0Hbm1ReadValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM1_READ
            pPmt->mockReadArgumentValue.push_back(vF0Hbm1WriteValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0_HBM1_Write
            pPmt->mockReadArgumentValue.push_back(vF0TimestampLValue);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success while reading VF0__TIMESTAMP_L
            pPmt->mockReadArgumentValue.push_back(0);
            pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN); // Return Failure while reading VF0__TIMESTAMP_H
            EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
        }
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCAndSteppingIsNotA0ThenHbmFrequencyWillBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp;
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, REVISION_A1, hbmFrequency);
    EXPECT_EQ(hbmFrequency, 0u);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfMemoryHealthSysfsEntryNotAvaliableThenMemoryHealthWillsetUnknown) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfMemoryHealthSetRebootAlaramThenMemoryHealthWillsetDegraded) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadStringValue.push_back("REBOOT_ALARM");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_DEGRADED);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfMemoryHealthSetDegradedThenMemoryHealthWillReturnsetCritical) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadStringValue.push_back("DEGRADED");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_CRITICAL);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfMockMemoryHealthSetDegradeFailThenMemoryHealthWillsetReplace) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadStringValue.push_back("DEGRADED_FAILED");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_REPLACE);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfMockMemoryHealthSetRandomValueThenMemoryHealthWillsetUnknown) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadStringValue.push_back("RANDOM");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIfGetMemoryRegionsFailsThenErrorIsReturned) {
    setLocalSupportedAndReinit(true);

    pDrm->mockReturnEmptyRegions = true;
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_state_t state;
        EXPECT_EQ(zesMemoryGetState(handle, &state), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

TEST_F(SysmanMultiDeviceFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved) {
    zes_mem_properties_t properties = {};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_bool_t isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    Device::fromHandle(device)->getProperties(&deviceProperties);
    LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
    EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    delete pLinuxMemoryImp;
}

class SysmanMultiDeviceMemoryFixture : public SysmanMultiDeviceFixture {
  protected:
    std::unique_ptr<MockMemorySysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    MockMemoryNeoDrm *pDrm = nullptr;
    Drm *pOriginalDrm = nullptr;
    std::vector<ze_device_handle_t> deviceHandles;
    uint32_t subDeviceCount = 0;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();

        pSysfsAccessOld = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockMemorySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pMemoryManagerOld = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = new MockMemoryManagerSysman(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = true;
        device->getDriverHandle()->setMemoryManager(pMemoryManager);

        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<IoctlHelperPrelim20>(*pDrm));

        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
        pLinuxSysmanImp->pDrm = pDrm;

        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getMemoryHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOld);
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOld;
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        if (pDrm != nullptr) {
            delete pDrm;
            pDrm = nullptr;
        }
        if (pMemoryManager != nullptr) {
            delete pMemoryManager;
            pMemoryManager = nullptr;
        }
    }

    void setLocalSupportedAndReinit(bool supported) {
        pMemoryManager->localMemorySupported[0] = supported;

        for (auto handle : pSysmanDeviceImp->pMemoryHandleContext->handleList) {
            delete handle;
        }

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        std::vector<ze_device_handle_t> deviceHandles;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        pSysmanDeviceImp->pMemoryHandleContext->init(deviceHandles);
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
    MemoryManager *pMemoryManagerOld;
};

TEST_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingMemoryPropertiesWhileCallingGetValErrorThenValidMemoryPropertiesRetrieved) {
    pSysfsAccess->mockReadStringValue.push_back("0");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    pSysmanDeviceImp->pMemoryHandleContext->init(deviceHandles);
    for (auto deviceHandle : deviceHandles) {
        zes_mem_properties_t properties = {};
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        bool isSubDevice = deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
        LinuxMemoryImp *pLinuxMemoryImp = new LinuxMemoryImp(pOsSysman, isSubDevice, deviceProperties.subdeviceId);
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
        EXPECT_EQ(properties.subdeviceId, deviceProperties.subdeviceId);
        EXPECT_EQ(properties.onSubdevice, isSubDevice);
        EXPECT_EQ(properties.physicalSize, 0u);
        delete pLinuxMemoryImp;
    }
}

TEST_F(SysmanMultiDeviceMemoryFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved) {
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    setLocalSupportedAndReinit(true);
    std::vector<ze_device_properties_t> devicesProperties;
    for (auto deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        devicesProperties.push_back(deviceProperties);
    }
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, deviceHandles.size());

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    uint32_t subDeviceIndex = 0;
    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.subdeviceId, devicesProperties[subDeviceIndex].subdeviceId);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
        subDeviceIndex++;
    }
}

TEST_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceeds) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(subDeviceCount);
    zes_mem_state_t state1;
    ze_result_t result = zesMemoryGetState(handles[0], &state1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state1.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state1.size, probedSizeRegionOne);
    EXPECT_EQ(state1.free, unallocatedSizeRegionOne);

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state2.size, probedSizeRegionFour);
    EXPECT_EQ(state2.free, unallocatedSizeRegionFour);
}

} // namespace ult
} // namespace L0
