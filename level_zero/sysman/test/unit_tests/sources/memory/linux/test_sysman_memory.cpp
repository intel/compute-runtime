/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

static const uint32_t memoryHandleComponentCount = 1u;
static const std::string mockPhysicalSize = "0x00000040000000";

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
        pDrm->ioctlHelper = static_cast<std::unique_ptr<NEO::IoctlHelper>>(std::make_unique<SysmanMemoryMockIoctlHelper>(*pDrm));
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

static int mockReadLinkSingleTelemetryNodeSuccess(const char *path, char *buf, size_t bufsize) {
    std::map<std::string, std::string> fileNameLinkMap = {
        {telem1NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {

    std::map<std::string, std::string> fileNameLinkMap = {
        {telem1NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem1/"},
        {telem2NodeName, "../../devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0/intel-dvsec-2.1.auto/intel_pmt/telem2/"},
    };
    auto it = fileNameLinkMap.find(std::string(path));
    if (it != fileNameLinkMap.end()) {
        std::memcpy(buf, it->second.c_str(), it->second.size());
        return static_cast<int>(it->second.size());
    }
    return -1;
}

static int mockOpenSingleTelemetryNodeSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem1OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem1GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem1TelemFileName) {
        returnValue = 6;
    } else if (strPathName == maxBwFileName) {
        returnValue = 7;
    }
    return returnValue;
}

static int mockOpenSuccess(const char *pathname, int flags) {

    int returnValue = -1;
    std::string strPathName(pathname);
    if (strPathName == telem2OffsetFileName) {
        returnValue = 4;
    } else if (strPathName == telem2GuidFileName) {
        returnValue = 5;
    } else if (strPathName == telem2TelemFileName) {
        returnValue = 6;
    } else if (strPathName == hbmFreqFilePath) {
        returnValue = 7;
    }
    return returnValue;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint32_t val = 0;
    if (fd == 4) {
        oStream << "0";
    } else if (fd == 5) {
        oStream << "0xb15a0ede";
    } else if (fd == 6) {
        if (offset == vF1Vfid) {
            val = 1;
        } else if (offset == vF1HbmReadL) {
            val = vFHbmLRead;
        } else if (offset == vF1HbmReadH) {
            val = vFHbmHRead;
        } else if (offset == vF1HbmWriteL) {
            val = vFHbmLWrite;
        } else if (offset == vF1HbmWriteH) {
            val = vFHbmHWrite;
        }
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7) {
        oStream << hbmRP0Frequency;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadIdiFilesSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    uint64_t val = 0;
    if (fd == 4) {
        oStream << "0";
    } else if (fd == 5) {
        oStream << "0x4f9502";
    } else if (fd == 6) {
        if (offset >= minIdiReadOffset && offset < minIdiWriteOffset) {
            val = mockIdiReadVal;
        } else if (offset >= minIdiWriteOffset && offset < minDisplayVc1ReadOffset) {
            val = mockIdiWriteVal;
        } else if (offset >= minDisplayVc1ReadOffset) {
            val = mockDisplayVc1ReadVal;
        }
        memcpy(buf, &val, count);
        return count;
    } else if (fd == 7) {
        oStream << mockMaxBwDg2;
    } else {
        oStream << "-1";
    }
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenI915DriverVersionWhenValidCallingSysfsFilenamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    EXPECT_STREQ("gt/gt0/mem_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, true).c_str());
    EXPECT_STREQ("gt/gt0/mem_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, true).c_str());
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedForDiscretePlatforms) {
    if (!defaultHwInfo->capabilityTable.isIntegratedDevice) {
        setLocalSupportedAndReinit(false);
    }
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesWithNoLocalMemorySupportThenZeroCountIsReturnedForDiscretePlatforms) {
    if (!defaultHwInfo->capabilityTable.isIntegratedDevice) {
        setLocalSupportedAndReinit(false);
    }
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(count, 1u);
    } else {
        EXPECT_EQ(count, 0u);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidCountIsReturned, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenInvalidComponentCountWhenEnumeratingMemoryModulesThenValidCountIsReturned, IsPVC) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);

    count = count + 1;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidCountIsReturned, IsDG1) {
    uint32_t count = 0;
    EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, memoryHandleComponentCount);
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenComponentCountZeroWhenEnumeratingMemoryModulesThenValidCountIsReturned, IsDG2) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithHbmLocalMemoryThenVerifySysmanMemoryGetPropertiesCallSucceeds, IsPVC) {
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

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetPropertiesWithInvalidMemoryTypeThenVerifyGetPropertiesCallReturnsMemoryTypeAsDdrAndNumberOfChannelsAsUnknown) {
    pDrm->setMemoryType(INT_MAX);
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        zes_mem_properties_t properties;
        ze_result_t result = zesMemoryGetProperties(handle, &properties);
        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
        if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
            EXPECT_EQ(properties.location, ZES_MEM_LOC_SYSTEM);
        } else {
            EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
        }
        EXPECT_EQ(properties.numChannels, -1);
        EXPECT_FALSE(properties.onSubdevice);
        EXPECT_EQ(properties.subdeviceId, 0u);
        EXPECT_EQ(properties.physicalSize, 0u);
        EXPECT_EQ(properties.busWidth, -1);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingMemoryGetStateThenVerifySysmanMemoryGetStateCallSucceeds, IsPVC) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenNoTelemNodesAvailableWhenCallingZesMemoryGetBandwidthThenErrorIsReturned, IsPVC) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenTelemOffsetFileNotAvailableWhenCallingZesMemoryGetBandwidthThenErrorIsReturned, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenBothVfid0AndVfid1AreTrueThenErrorIsReturnedWhileGettingMemoryBandwidth, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            oStream << "5";
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNKNOWN);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVfid0IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth{};
        uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
        auto hwInfo = pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = pSysmanDeviceImp->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        hwInfo->platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, *hwInfo);
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters |= vFHbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vFHbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthWhenVfid1IsActiveThenSuccessIsReturnedAndBandwidthIsValid, IsPVC) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0xb15a0ede";
        } else if (fd == 6) {
            if (offset == vF1Vfid) {
                val = 1;
            } else if (offset == vF1HbmReadL) {
                val = vFHbmLRead;
            } else if (offset == vF1HbmReadH) {
                val = vFHbmHRead;
            } else if (offset == vF1HbmWriteL) {
                val = vFHbmLWrite;
            } else if (offset == vF1HbmWriteH) {
                val = vFHbmHWrite;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            oStream << hbmRP0Frequency;
        } else {
            oStream << "-1";
        }
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

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_SUCCESS);
        expectedReadCounters |= vFHbmHRead;
        expectedReadCounters = (expectedReadCounters << 32) | vFHbmLRead;
        expectedReadCounters = expectedReadCounters * transactionSize;
        EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
        expectedWriteCounters |= vFHbmHWrite;
        expectedWriteCounters = (expectedWriteCounters << 32) | vFHbmLWrite;
        expectedWriteCounters = expectedWriteCounters * transactionSize;
        EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
        expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
        EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenTelemOffsetFileNotAvailableWhenCallingZesMemoryGetBandwidthThenErrorIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthThenSuccessIsReturnedAndBandwidthIsValid, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadIdiFilesSuccess);

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
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

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesMemoryGetBandwidthIfIdiReadFailsThenErrorIsReturned, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint32_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            memcpy(buf, &val, sizeof(val));
            return sizeof(val);
        } else if (fd == 7) {
            oStream << mockMaxBwDg2;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        zes_mem_properties_t properties = {};
        zesMemoryGetProperties(handle, &properties);
        zes_mem_bandwidth_t bandwidth;
        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_NOT_AVAILABLE);
    }
}

HWTEST2_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetBandwidthAndReadingMaxBwFailsThenMaxBwIsReturnedAsZero, IsDG2) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSingleTelemetryNodeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockOpenSingleTelemetryNodeSuccess);

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        uint64_t val = 0;
        if (fd == 4) {
            oStream << "0";
        } else if (fd == 5) {
            oStream << "0x4f9502";
        } else if (fd == 6) {
            if (offset >= minIdiReadOffset && offset < minIdiWriteOffset) {
                val = mockIdiReadVal;
            } else if (offset >= minIdiWriteOffset && offset < minDisplayVc1ReadOffset) {
                val = mockIdiWriteVal;
            } else if (offset >= minDisplayVc1ReadOffset) {
                val = mockDisplayVc1ReadVal;
            }
            memcpy(buf, &val, count);
            return count;
        } else if (fd == 7) {
            errno = ENOENT;
            return -1;
        } else {
            oStream << "-1";
        }
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (const auto &handle : handles) {
        ASSERT_NE(nullptr, handle);
        zes_mem_bandwidth_t bandwidth;

        EXPECT_EQ(zesMemoryGetBandwidth(handle, &bandwidth), ZE_RESULT_ERROR_NOT_AVAILABLE);
        EXPECT_EQ(bandwidth.maxBandwidth, 0u);
    }
}

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndIoctlReturnedErrorThenApiReturnsError) {
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        GTEST_SKIP() << "This test is not applicable for integrated devices";
    }
    auto ioctlHelper = static_cast<SysmanMemoryMockIoctlHelper *>(pDrm->ioctlHelper.get());
    ioctlHelper->returnEmptyMemoryInfo = true;
    ioctlHelper->mockErrorNumber = EBUSY;
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

TEST_F(SysmanDeviceMemoryFixtureI915, GivenValidMemoryHandleWhenCallingZesSysmanMemoryGetStateAndDeviceIsNotAvailableThenDeviceLostErrorIsReturned) {
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        GTEST_SKIP() << "This test is not applicable for integrated devices";
    }
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

class SysmanDeviceMemoryFixtureXe : public SysmanDeviceMemoryFixture {
  protected:
    DebugManagerStateRestore restorer;
    MockMemoryNeoDrm *pDrm = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pProcfsAccess = std::make_unique<MockProcFsAccessInterface>();
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockMemorySysFsAccessInterface>();

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

class SysmanMultiDeviceMemoryFixture : public SysmanMultiDeviceFixture {
  protected:
    MockMemoryNeoDrm *pDrm = nullptr;
    L0::Sysman::SysmanDevice *device = nullptr;
    MockMemorySysFsAccessInterface *pSysfsAccess = nullptr;
    MockSysmanKmdInterfacePrelim *pSysmanKmdInterface = nullptr;
    MockMemoryFsAccessInterface *pFsAccess = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanMultiDeviceFixture::SetUp();

        pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
        pSysfsAccess = new MockMemorySysFsAccessInterface();
        pFsAccess = new MockMemoryFsAccessInterface();
        pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
        pSysmanKmdInterface->pFsAccess.reset(pFsAccess);
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
        pLinuxSysmanImp->pSysfsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getSysFsAccess();
        pLinuxSysmanImp->pFsAccess = pLinuxSysmanImp->pSysmanKmdInterface->getFsAccess();
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

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

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
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(state1.size, mockIntegratedDeviceAvailableMemory);
        EXPECT_EQ(state1.free, mockIntegratedDeviceFreeMemory);
    } else {
        EXPECT_EQ(state1.size, NEO::probedSizeRegionOne);
        EXPECT_EQ(state1.free, NEO::unallocatedSizeRegionOne);
    }

    zes_mem_state_t state2;
    result = zesMemoryGetState(handles[1], &state2);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(state2.health, ZES_MEM_HEALTH_UNKNOWN);
    if (defaultHwInfo->capabilityTable.isIntegratedDevice) {
        EXPECT_EQ(state2.size, mockIntegratedDeviceAvailableMemory);
        EXPECT_EQ(state2.free, mockIntegratedDeviceFreeMemory);
    } else {
        EXPECT_EQ(state2.size, NEO::probedSizeRegionFour);
        EXPECT_EQ(state2.free, NEO::unallocatedSizeRegionFour);
    }
}

TEST_F(SysmanMultiDeviceMemoryFixture, GivenMemFreeAndMemAvailableMissingInMemInfoWhenCallingGetStateThenFreeAndSizeValuesAreZero) {
    if (!defaultHwInfo->capabilityTable.isIntegratedDevice) {
        GTEST_SKIP() << "This test is only applicable for integrated devices.";
    }
    pFsAccess->mockMemInfoIncorrectValue = true;
    auto handles = getMemoryHandles(pOsSysman->getSubDeviceCount());
    zes_mem_state_t state = {};
    ze_result_t result = zesMemoryGetState(handles[0], &state);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
    EXPECT_EQ(state.free, 0u);
    EXPECT_EQ(state.size, 0u);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
