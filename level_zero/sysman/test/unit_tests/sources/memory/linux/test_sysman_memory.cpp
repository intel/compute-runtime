/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

static const uint32_t memoryHandleComponentCount = 1u;
static uint64_t hbmRP0Frequency = 4200;
static uint64_t mockMaxBwDg2 = 1343616u;

class SysmanDeviceMemoryFixture : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    DebugManagerStateRestore restorer;

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    void setLocalSupportedAndReinit(bool supported) {
        debugManager.flags.EnableLocalMemory.set(supported == true ? 1 : 0);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pSysmanDeviceImp->pMemoryHandleContext->init(pOsSysman->getSubDeviceCount());
    }
};

class SysmanDeviceMemoryFixtureI915 : public SysmanDeviceMemoryFixture {
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        device = pSysmanDeviceImp;
        auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pSysmanProductHelper);
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<NEO::MockIoctlHelper>(*pDrm));
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
        auto subdeviceId = 0u;
        auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        do {
            auto pPmt = new MockMemoryPmt();
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
        } while (++subdeviceId < subDeviceCount);
    }

    void TearDown() override {
        pLinuxSysmanImp->releasePmtObject();
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDeviceMemoryFixtureI915, GivenI915DriverVersionWhenValidCallingSysfsFilenamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("gt/gt0/mem_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, true).c_str());
    EXPECT_STREQ("gt/gt0/mem_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, true).c_str());
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithLocalMemorySupportThenValidCountIsReturned) {
    setLocalSupportedAndReinit(false);
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, 0u);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned, IsDG1) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenValidCountIsReturned, IsDG2) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidHandlesIsReturned, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    std::vector<zes_mem_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    for (auto handle : handles) {
        EXPECT_NE(handle, nullptr);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesAndQuerySystemInfoFailsThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesAndQuerySystemInfoSucceedsButMemSysInfoIsNullThenVerifySysmanMemoryGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithHBMLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithLPDDR4LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithLPDDR5LocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithDDRLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
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

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceeds) {
    auto pLinuxMemoryImp = std::make_unique<PublicLinuxMemoryImp>(pOsSysman, true, 0);
    zes_mem_state_t state;
    ze_result_t result = pLinuxMemoryImp->getState(&state);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
    EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
    EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingMemoryGetStateAndFwUtilInterfaceIsAbsentThenMemoryHealthWillBeUnknown) {
    VariableBackup<L0::Sysman::FirmwareUtil *> backup(&pLinuxSysmanImp->pFwUtilInterface);
    pLinuxSysmanImp->pFwUtilInterface = nullptr;
    auto pLinuxMemoryImp = std::make_unique<PublicLinuxMemoryImp>(pOsSysman, true, 0);
    zes_mem_state_t state;
    ze_result_t result = pLinuxMemoryImp->getState(&state);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state.health, ZES_MEM_HEALTH_UNKNOWN);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateThenSuccessIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_state_t state;
        EXPECT_EQ(zesMemoryGetState(handle, &state), ZE_RESULT_SUCCESS);
        EXPECT_EQ(state.health, ZES_MEM_HEALTH_OK);
        EXPECT_EQ(state.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state.free, NEO::unallocatedSizeRegionOne);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetBandwidthThenFailureIsReturned, IsDG1) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreTrueThenErrorIsReturnedWhileGettingMemoryBandwidth, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenPmtObjectIsNullThenFailureRetuned, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVFID1IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthIfIdiReadFailsTheFailureIsReturned, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthIfDisplayVc1ReadFailsTheFailureIsReturned, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetBandwidthForDg2PlatformAndReadingMaxBwFailsThenMaxBwIsReturnedAsZero, IsDG2) {
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

class SysmanDeviceMemoryFixtureXe : public SysmanDeviceMemoryFixture {
  protected:
    DebugManagerStateRestore restorer;
    MockMemoryNeoDrm *pDrm = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getProductFamily());
        pSysmanKmdInterface->pProcfsAccess = std::make_unique<MockProcFsAccessInterface>();
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockSysFsAccessInterface>();

        device = pSysmanDeviceImp;
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pDrm = new MockMemoryNeoDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockMemoryNeoDrm>(pDrm));
        pDrm->setMemoryType(NEO::DeviceBlobConstants::MemoryType::hbm2e);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDeviceMemoryFixtureXe, GivenXeDriverVersionWhenCallingIsPhysicalAddressAvailableThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_TRUE(pSysmanKmdInterface->isPhysicalMemorySizeSupported());
}

TEST_F(SysmanDeviceMemoryFixtureXe, GivenXeDriverVersionWhenValidCallingSysfsFilenamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("device/tile0/physical_vram_size_bytes", pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(0).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq_vram_rp0", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, true).c_str());
    EXPECT_STREQ("device/tile0/gt0/freq_vram_rpn", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, true).c_str());
}

HWTEST2_F(SysmanDeviceMemoryFixtureXe, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureXe, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesThenSuccessIsReturned, IsDG1) {
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
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_EQ(properties.busWidth, -1);
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
