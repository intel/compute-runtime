/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/linux/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_i915.h"

namespace L0 {
namespace Sysman {
namespace ult {

static const std::string mockPhysicalSize = "0x00000040000000";

class SysmanProductHelperMemoryTest : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    DebugManagerStateRestore restorer;
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

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndVfid0IsActiveThenVerifyBandWidthIsValid, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
    pPmt->mockVfid0Status = true;

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = vF0Hbm0ReadValue + vF0Hbm1ReadValue + vF0Hbm2ReadValue + vF0Hbm3ReadValue;
    uint64_t expectedWriteCounters = vF0Hbm0WriteValue + vF0Hbm1WriteValue + vF0Hbm2WriteValue + vF0Hbm3WriteValue;
    uint64_t expectedBandwidth = 0;

    expectedReadCounters = expectedReadCounters * transactionSize;
    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAndVfid1IsActiveThenVerifyBandWidthIsValid, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
    pPmt->setGuid(guid64BitMemoryCounters);
    pPmt->mockVfid1Status = true;

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0;
    expectedReadCounters |= vF0HbmHRead;
    expectedReadCounters = (expectedReadCounters << 32) | vF0HbmLRead;
    expectedReadCounters = expectedReadCounters * transactionSize;
    expectedWriteCounters |= vF0HbmHWrite;
    expectedWriteCounters = (expectedWriteCounters << 32) | vF0HbmLWrite;
    expectedWriteCounters = expectedWriteCounters * transactionSize;
    uint64_t expectedBandwidth = 128 * hbmRP0Frequency * 1000 * 1000 * 4;

    EXPECT_EQ(bandwidth.readCounter, expectedReadCounters);
    EXPECT_EQ(bandwidth.writeCounter, expectedWriteCounters);
    EXPECT_EQ(bandwidth.maxBandwidth, expectedBandwidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenHbm0ReadFailsAndGuidNotSetWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
    pPmt->mockReadArgumentValue.push_back(1);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadArgumentValue.push_back(0);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenHbm0WriteFailsAndGuidNotSetWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    auto result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
    pPmt->mockReadArgumentValue.push_back(1);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadArgumentValue.push_back(0);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadArgumentValue.push_back(vF0Hbm0ReadValue);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);

    zes_mem_bandwidth_t bandwidth = {};
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ReadFailsWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId));
    pPmt->setGuid(guid64BitMemoryCounters);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);

    zes_mem_bandwidth_t bandwidth{};
    auto result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid0ReadFailsAndGuidNotSetBitWhenCallingGetBandwidthAndVfid0IsActiveThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId));
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);

    zes_mem_bandwidth_t bandwidth{};
    auto result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenVfid1ReadFailsWhenCallingGetBandwidthAndVfid1IsActiveThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId));
    pPmt->setGuid(guid64BitMemoryCounters);
    pPmt->mockReadArgumentValue.push_back(1);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_ERROR_UNKNOWN);

    zes_mem_bandwidth_t bandwidth{};
    auto result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenBothVfid0AndVfid1AreFalseWhenGettingMemoryBandwidthThenFailureIsReturned, IsPVC) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    uint32_t subDeviceId = 0;

    auto pPmt = static_cast<MockMemoryPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId));
    pPmt->setGuid(guid64BitMemoryCounters);
    pPmt->mockReadArgumentValue.push_back(0);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pPmt->mockReadArgumentValue.push_back(0);
    pPmt->mockReadValueReturnStatus.push_back(ZE_RESULT_SUCCESS);

    zes_mem_bandwidth_t bandwidth{};
    auto result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNKNOWN);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingGetMemoryBandwidthAPIsThenErrorIsReturned, IsDG1) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_DDR);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, -1);
    EXPECT_EQ(properties.busWidth, -1);
    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWithPrelimInterfaceWhenCallingGetMemoryPropertiesThenVerifyPropertiesAreProper, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back(mockPhysicalSize);
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_SUCCESS);
    pSysfsAccess->isRepeated = true;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pSysmanKmdInterface, subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, strtoull(mockPhysicalSize.c_str(), nullptr, 16));
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndPhysicalSizeReadFailsWhenCallingGetMemoryPropertiesThenVerifyPhysicalSizeIsZero, IsDG2) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;

    auto pSysmanKmdInterface = new MockSysmanKmdInterfacePrelim(pLinuxSysmanImp->getSysmanProductHelper());
    MockMemorySysFsAccessInterface *pSysfsAccess = new MockMemorySysFsAccessInterface();
    pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);
    pSysmanKmdInterface->pSysfsAccess.reset(pSysfsAccess);
    pSysfsAccess->mockReadStringValue.push_back("0");
    pSysfsAccess->mockReadReturnStatus.push_back(ZE_RESULT_ERROR_NOT_AVAILABLE);
    pSysfsAccess->isRepeated = true;

    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pSysmanKmdInterface, subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.physicalSize, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockMaxBwDg2;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);

    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
    EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
    expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
    EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
    expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
    EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
    EXPECT_GT(bandwidth.timestamp, 0u);
}

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceAndPhysicalSizeSupportedWhenCallingMemoryAPIsThenErrorIsReturned, IsDG2) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << mockMaxBwDg2;
        std::string value = oStream.str();
        memcpy(buf, value.data(), count);
        return count;
    });

    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
    zes_mem_properties_t properties;
    uint64_t expectedReadCounters = 0, expectedWriteCounters = 0, expectedBandwidth = 0;
    bool isSubdevice = true;
    uint32_t subDeviceId = 0;
    ze_result_t result = pSysmanProductHelper->getMemoryProperties(&properties, pLinuxSysmanImp, pDrm, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId, isSubdevice);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);

    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    expectedReadCounters = numberMcChannels * (mockIdiReadVal + mockDisplayVc1ReadVal) * transactionSize;
    EXPECT_EQ(expectedReadCounters, bandwidth.readCounter);
    expectedWriteCounters = numberMcChannels * mockIdiWriteVal * transactionSize;
    EXPECT_EQ(expectedWriteCounters, bandwidth.writeCounter);
    expectedBandwidth = mockMaxBwDg2 * mbpsToBytesPerSecond;
    EXPECT_EQ(expectedBandwidth, bandwidth.maxBandwidth);
    EXPECT_GT(bandwidth.timestamp, 0u);
}

TEST(SysmanProductHelperTest, GivenInvalidProductFamilyWhenCallingProductHelperCreateThenNullPtrIsReturned) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(IGFX_UNKNOWN);
    EXPECT_EQ(nullptr, pSysmanProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0