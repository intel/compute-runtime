/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt_xml_offsets.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/memory/linux/mock_memory.h"

extern bool sysmanUltsEnable;

namespace L0 {
namespace ult {

constexpr static int32_t memoryBusWidth = 128; // bus width in bytes
constexpr static int32_t numMemoryChannels = 8;
constexpr static uint32_t memoryHandleComponentCount = 1u;

class SysmanMemoryMockIoctlHelper : public NEO::MockIoctlHelper {

  public:
    using NEO::MockIoctlHelper::MockIoctlHelper;
    bool returnEmptyMemoryInfo = false;
    int32_t mockErrorNumber = 0;

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {
        if (returnEmptyMemoryInfo) {
            errno = mockErrorNumber;
            return {};
        }
        return NEO::MockIoctlHelper::createMemoryInfo();
    }
};

class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  public:
    MockMemoryNeoDrm *pDrm = nullptr;

  protected:
    std::unique_ptr<MockMemorySysfsAccess> pSysfsAccess;
    std::unique_ptr<MockMemoryFsAccess> pFsAccess;
    SysfsAccess *pSysfsAccessOld = nullptr;
    FsAccess *pFsAccessOriginal = nullptr;
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
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<SysmanMemoryMockIoctlHelper>(*pDrm));

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
        auto &productHelper = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getProductHelper();
        stepping = productHelper.getSteppingFromHwRevId(hwInfo);
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

TEST_F(SysmanDeviceMemoryFixture, GivenWhenGettingMemoryPropertiesThenSuccessIsReturned) {
    zes_mem_properties_t properties = {};
    auto pLinuxMemoryImp = std::make_unique<LinuxMemoryImp>(pOsSysman, true, 0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
}

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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr4);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr5);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::gddr6);
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
        EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIoctlReturnedErrorThenApiReturnsError) {
    setLocalSupportedAndReinit(true);

    auto ioctlHelper = static_cast<SysmanMemoryMockIoctlHelper *>(pDrm->ioctlHelper.get());
    ioctlHelper->returnEmptyMemoryInfo = true;
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(state.size, 0u);
        EXPECT_EQ(state.free, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndDeviceIsNotAvailableThenDeviceLostErrorIsReturned) {
    setLocalSupportedAndReinit(true);

    auto ioctlHelper = static_cast<SysmanMemoryMockIoctlHelper *>(pDrm->ioctlHelper.get());
    ioctlHelper->returnEmptyMemoryInfo = true;
    ioctlHelper->mockErrorNumber = ENODEV;
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_ERROR_DEVICE_LOST);
        EXPECT_EQ(state.size, 0u);
        EXPECT_EQ(state.free, 0u);
        errno = 0;
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanResourcesAreReleasedAndReInitializedWhenCallingZesSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
    pMemoryManager->localMemorySupported[0] = true;

    pLinuxSysmanImp->releaseSysmanDeviceResources();
    pLinuxSysmanImp->pDrm = pDrm;
    pLinuxSysmanImp->reInitSysmanDeviceResources();

    VariableBackup<std::map<uint32_t, PlatformMonitoringTech *>> pmtBackup(&pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject);
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new MockMemoryPmt(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                      deviceProperties.subdeviceId);
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
    }

    VariableBackup<FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = new MockFwUtilInterface();

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
    }

    pLinuxSysmanImp->releasePmtObject();
    delete pLinuxSysmanImp->pFwUtilInterface;
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanResourcesAreReleasedAndReInitializedWhenCallingZesSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {
    pMemoryManager->localMemorySupported[0] = true;

    pLinuxSysmanImp->releaseSysmanDeviceResources();
    pLinuxSysmanImp->pDrm = pDrm;
    pLinuxSysmanImp->reInitSysmanDeviceResources();

    VariableBackup<std::map<uint32_t, PlatformMonitoringTech *>> pmtBackup(&pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject);
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    for (auto &deviceHandle : deviceHandles) {
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
        auto pPmt = new MockMemoryPmt(pFsAccess.get(), deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                      deviceProperties.subdeviceId);
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
    }

    VariableBackup<FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = new MockFwUtilInterface();

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;

        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
        EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
    }

    pLinuxSysmanImp->releasePmtObject();
    delete pLinuxSysmanImp->pFwUtilInterface;
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
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
        ASSERT_NE(nullptr, handle);
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthWhenVFID0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getProductHelper();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid0Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters |= vF0HbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vF0HbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vF0HbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vF0HbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthWhenVFID1IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getProductHelper();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid1Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters |= vF0HbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vF0HbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vF0HbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vF0HbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF0_HBM_READ_LFailsThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        pPmt->mockReadArgumentValue.push_back(2);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF0_VFIDFailsForOldGuidThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid("0xb15a0edd");
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF1_VFIDFailsForOldGuidThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid("0xb15a0edd");
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF0_HBM_READ_HFailsThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(2);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF0_HBM_WRITE_LFailsThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(2);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthAndVF0_HBM_WRITE_HFailsThenFailureIsReturned, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(4);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(2);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidUsRevIdForRevisionBWhenCallingzesSysmanMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    setLocalSupportedAndReinit(true);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pLinuxSysmanImp->getDeviceHandle()->getNEODevice()->getProductHelper();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid1Status = true;
        pSysfsAccess->mockReadUInt64Value.push_back(hbmRP0Frequency);
        pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
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
        ASSERT_NE(nullptr, handle);
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
        expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
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
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF0_VFID
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS); // Return success after reading VF1_VFID
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

TEST_F(SysmanDeviceMemoryFixture, GivenCallinggetHbmFrequencyWhenProductFamilyIsPVCAndSteppingIsNotA0ThenHbmFrequencyWillBeZero) {
    PublicLinuxMemoryImp *pLinuxMemoryImp = new PublicLinuxMemoryImp;
    uint64_t hbmFrequency = 0;
    pLinuxMemoryImp->getHbmFrequency(IGFX_PVC, REVISION_A1, hbmFrequency);
    EXPECT_EQ(hbmFrequency, 0u);
    delete pLinuxMemoryImp;
}

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndFwUtilInterfaceIsAbsentThenMemoryHealthWillBeUnknown) {
    setLocalSupportedAndReinit(true);

    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    auto deviceImp = static_cast<L0::DeviceImp *>(pLinuxSysmanImp->getDeviceHandle());
    deviceImp->driverInfo.reset(nullptr);
    VariableBackup<L0::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
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
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<NEO::MockIoctlHelper>(*pDrm));

        pSysmanDevice = device->getSysmanHandle();
        pSysmanDeviceImp = static_cast<SysmanDeviceImp *>(pSysmanDevice);
        pOsSysman = pSysmanDeviceImp->pOsSysman;
        pLinuxSysmanImp = static_cast<PublicLinuxSysmanImp *>(pOsSysman);
        pLinuxSysmanImp->pDrm = pDrm;

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
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize.data());
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
        EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.data(), nullptr, 16));
        subDeviceIndex++;
    }
}

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(subDeviceCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }
    zes_mem_state_t state1;
    ze_result_t result = zesMemoryGetState(handles[0], &state1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state1.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state1.size, NEO::probedSizeRegionOne);
    EXPECT_EQ(state1.free, NEO::unallocatedSizeRegionOne);

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state2.size, NEO::probedSizeRegionFour);
    EXPECT_EQ(state2.free, NEO::unallocatedSizeRegionFour);
}

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {
    setLocalSupportedAndReinit(true);

    auto handles = getMemoryHandles(subDeviceCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }
    zes_mem_state_t state1;
    ze_result_t result = zesMemoryGetState(handles[0], &state1);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state1.health, ZES_MEM_HEALTH_UNKNOWN);
    EXPECT_EQ(state1.size, NEO::probedSizeRegionOne);
    EXPECT_EQ(state1.free, NEO::unallocatedSizeRegionOne);

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_UNKNOWN);
    EXPECT_EQ(state2.size, NEO::probedSizeRegionFour);
    EXPECT_EQ(state2.free, NEO::unallocatedSizeRegionFour);
}

} // namespace ult
} // namespace L0
