/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/driver_info.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/memory/linux/sysman_os_memory_imp_prelim.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt_xml_offsets.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory_prelim.h"

namespace L0 {
namespace Sysman {
namespace ult {

static std::string mockPhysicalSize = "0x00000040000000";
static const uint64_t hbmRP0Frequency = 4200;
static const uint64_t hbmA0Frequency = 3200;
static const uint64_t mockMaxBwDg2 = 1343616u;
static const int32_t memoryBusWidth = 128;
static const int32_t numMemoryChannels = 8;
static const uint32_t memoryHandleComponentCount = 1u;
static const std::string sampleGuid1 = "0xb15a0edc";

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
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    DebugManagerStateRestore restorer;
    uint16_t stepping;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pSysmanProductHelper);
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<SysmanMemoryMockIoctlHelper>(*pDrm));
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
        auto subdeviceId = 0u;
        auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        do {
            auto pPmt = new MockMemoryPmt();
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
        } while (++subdeviceId < subDeviceCount);
        device = pSysmanDevice;
    }

    void TearDown() override {
        pLinuxSysmanImp->releasePmtObject();
        SysmanDeviceFixture::TearDown();
    }

    void setLocalSupportedAndReinit(bool supported) {
        debugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

TEST_F(SysmanDeviceMemoryFixture, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidHandlesIsReturned) {
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesAndQuerySystemInfoFailsThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown, IsPVC) {
    pDrm->mockQuerySystemInfoReturnValue.push_back(false);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown, IsPVC) {
    pDrm->mockQuerySystemInfoReturnValue.push_back(true);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithHBMLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithLPDDR4LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr4);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithLPDDR5LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::lpddr5);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithDDRLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
    pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::gddr6);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {

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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
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

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZetSysmanMemoryGetStateAndIoctlReturnedErrorThenApiReturnsError) {
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanResourcesAreReleasedAndReInitializedWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {
    pLinuxSysmanImp->releaseSysmanDeviceResources();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->reInitSysmanDeviceResources());

    VariableBackup<std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *>> pmtBackup(&pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject);
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    auto subdeviceId = 0u;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    do {
        auto pPmt = new MockMemoryPmt();
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
    } while (++subdeviceId < subDeviceCount);

    VariableBackup<L0::Sysman::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenSysmanResourcesAreReleasedAndReInitializedWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
    pLinuxSysmanImp->releaseSysmanDeviceResources();
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->reInitSysmanDeviceResources());

    VariableBackup<std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *>> pmtBackup(&pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject);
    pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
    auto subdeviceId = 0u;
    auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
    do {
        auto pPmt = new MockMemoryPmt();
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
    } while (++subdeviceId < subDeviceCount);

    VariableBackup<L0::Sysman::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenPmtObjectIsNullThenFailureIsReturned, IsPVC) {
    for (auto &subDeviceIdToPmtEntry : pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject) {
        if (subDeviceIdToPmtEntry.second != nullptr) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveAndReadingVFID0FailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveAndReadingVFID1FailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid0Status = true;

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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBMReadLFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBMReadHFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmLRead);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBMWriteLFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmLRead);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmHRead);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBMWriteHFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmLRead);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmHRead);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0HbmLWrite);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleAndGuidNotSetWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = vF0Hbm0ReadValue + vF0Hbm1ReadValue + vF0Hbm2ReadValue + vF0Hbm3ReadValue;
        uint64_t expectedWriteCounters = vF0Hbm0WriteValue + vF0Hbm1WriteValue + vF0Hbm2WriteValue + vF0Hbm3WriteValue;
        uint64_t expectedBandwidth = 0;
        constexpr uint64_t transactionSize = 32;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->mockVfid0Status = true;

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleAndGuidNotSetWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBM0ReadFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleAndGuidNotSetWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveWhenReadingHBM0WriteFailsThenFailureIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));

        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(vF0Hbm0ReadValue);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID1IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid1Status = true;

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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreTrueThenErrorIsReturnedWhileGettingMemoryBandwidth, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(1);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreFalseThenErrorIsReturnedWhileGettingMemoryBandwidth, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        pPmt->mockReadArgumentValue.push_back(0);
        pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID1IsActiveAndHwRevIdIsRevisionA0ThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
        uint64_t expectedBandwidth = 0;
        zes_mem_properties_t properties = {ZES_STRUCTURE_TYPE_MEM_PROPERTIES};
        zesMemoryGetProperties(handle, &properties);

        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, *hwInfo);

        auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
        pPmt->setGuid(guid64BitMemoryCounters);
        pPmt->mockVfid1Status = true;

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters |= vF0HbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vF0HbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vF0HbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vF0HbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmA0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockMaxBwDg2;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);
        zes_mem_bandwidth_t bandwidth;
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthIfIdiReadFailsTheFailureIsReturned, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthIfDisplayVc1ReadFailsTheFailureIsReturned, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthThenUnsupportedFeatureIsReturned, IsDG1) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetBandwidthForDg2PlatformAndReadingMaxBwFailsThenMaxBwIsReturnedAsZero, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        errno = ENOENT;
        return -1;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);

        zes_mem_bandwidth_t bandwidth;

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(bandwidth.maxBandwidth, 0u);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingzesSysmanMemoryGetBandwidthForDg2PlatformIfIdiWriteFailsTheFailureIsReturned, IsDG2) {
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

TEST_F(SysmanDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndFwUtilInterfaceIsAbsentThenMemoryHealthWillBeUnknown) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    VariableBackup<L0::Sysman::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_state_t state;
        ze_result_t result = zesMemoryGetState(handle, &state);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
    }
}

class SysmanMultiDeviceMemoryFixture : public SysmanMultiDeviceFixture {
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    MockMemorySysfsAccess *pSysfsAccess = nullptr;
    MockSysmanKmdInterfaceI915Prelim *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanMultiDeviceFixture::SetUp();

        pSysmanKmdInterface = new MockSysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getProductFamily());
        pSysfsAccess = new MockMemorySysfsAccess();
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getSysFsAccess();
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<NEO::MockIoctlHelper>(*pDrm));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        device = pSysmanDeviceImp;
    }

    void TearDown() override {
        SysmanMultiDeviceFixture::TearDown();
    }

    void mockInitFsAccess() {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
            return sizeofPath;
        });
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }

    void setLocalSupportedAndReinit(bool supported) {
        debugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenGettingMemoryPropertiesIsSysReadCallFailsThenValidMemoryPropertiesRetrieved, IsPVC) {
    pSysfsAccess->mockReadStringValue.push_back("0");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);

    pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    for (auto subDeviceId = 0u; subDeviceId < pOsSysman->getSubDeviceCount(); subDeviceId++) {
        zes_mem_properties_t properties = {};
        auto isSubDevice = pOsSysman->getSubDeviceCount() > 0u;
        auto pLinuxMemoryImp = std::make_unique<L0::Sysman::LinuxMemoryImp>(pOsSysman, isSubDevice, subDeviceId);
        EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
        EXPECT_EQ(properties.subdeviceId, subDeviceId);
        EXPECT_EQ(properties.onSubdevice, isSubDevice);
        EXPECT_EQ(properties.physicalSize, 0u);
    }
}

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWithNoSubdeviceWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved, IsPVC) {
    pSysmanDeviceImp->pMemoryHandleContext->init(0);
    zes_mem_properties_t properties = {};
    auto isSubDevice = false;
    auto subDeviceId = 0u;
    auto pLinuxMemoryImp = std::make_unique<L0::Sysman::LinuxMemoryImp>(pOsSysman, isSubDevice, subDeviceId);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxMemoryImp->getProperties(&properties));
    EXPECT_EQ(properties.subdeviceId, subDeviceId);
    EXPECT_EQ(properties.onSubdevice, isSubDevice);
    EXPECT_EQ(properties.physicalSize, 0u);
}

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidDevicePointerWhenGettingMemoryPropertiesThenValidMemoryPropertiesRetrieved, IsPVC) {
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, std::max(pOsSysman->getSubDeviceCount(), 1u));

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        EXPECT_EQ(zesMemoryGetProperties(handle, &properties), ZE_RESULT_SUCCESS);
        EXPECT_TRUE(properties.onSubdevice);
        EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
    }
}

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsOK, IsPVC) {
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
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

HWTEST2_F(SysmanMultiDeviceMemoryFixture, GivenValidMemoryHandleWhenCallingZesMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceedsAndHealthIsUnknown, IsNotPVC) {
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
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
} // namespace Sysman
} // namespace L0
