/*
 * Copyright (C) 2023 Intel Corporation
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

namespace L0 {
namespace Sysman {
namespace ult {

static const uint64_t hbmRP0Frequency = 4200;
static const uint64_t mockMaxBwDg2 = 1343616u;

static uint32_t mockMemoryType = NEO::DeviceBlobConstants::MemoryType::hbm2e;

static const uint32_t vF0HbmLRead = 16;
static const uint32_t vF0HbmHRead = 2;
static const uint32_t vF0HbmLWrite = 8;
static const uint32_t vF0HbmHWrite = 2;

static const uint8_t vF0Hbm0ReadValue = 92;
static const uint8_t vF0Hbm0WriteValue = 96;
static const uint8_t vF0Hbm1ReadValue = 104;
static const uint8_t vF0Hbm1WriteValue = 108;
static const uint8_t vF0TimestampLValue = 168;
static const uint8_t vF0TimestampHValue = 172;
static const uint8_t vF0Hbm2ReadValue = 113;
static const uint8_t vF0Hbm2WriteValue = 125;
static const uint8_t vF0Hbm3ReadValue = 135;
static const uint8_t vF0Hbm3WriteValue = 20;
static const uint64_t mockIdiReadVal = 8u;
static const uint64_t mockIdiWriteVal = 9u;
static const uint64_t mockDisplayVc1ReadVal = 10u;
static const uint64_t numberMcChannels = 16;
static const uint64_t transactionSize = 32;

class MockDrm : public NEO::Drm {
  public:
    MockDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
    void resetSystemInfo() {
        systemInfo.reset(nullptr);
    }

    void setMemoryType(uint32_t memory) {
        mockMemoryType = memory;
    }

    bool querySystemInfo() override {
        bool returnValue = true;
        uint32_t hwBlob[] = {NEO::DeviceBlobConstants::maxMemoryChannels, 1, 8, NEO::DeviceBlobConstants::memoryType, 0, mockMemoryType};
        std::vector<uint32_t> inputBlobData(reinterpret_cast<uint32_t *>(hwBlob), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob, sizeof(hwBlob))));
        this->systemInfo.reset(new SystemInfo(inputBlobData));
        return returnValue;
    }
};

class MockPmt : public L0::Sysman::PlatformMonitoringTech {
  public:
    using L0::Sysman::PlatformMonitoringTech::guid;
    MockPmt() = default;

    void setGuid(std::string guid) {
        this->guid = guid;
    }

    ze_result_t readValue(const std::string key, uint32_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (key.compare("VF0_VFID") == 0) {
            val = 1;
        } else if (key.compare("VF1_VFID") == 0) {
            val = 0;
        } else if (key.compare("VF0_HBM0_READ") == 0) {
            val = vF0Hbm0ReadValue;
        } else if (key.compare("VF0_HBM0_WRITE") == 0) {
            val = vF0Hbm0WriteValue;
        } else if (key.compare("VF0_HBM1_READ") == 0) {
            val = vF0Hbm1ReadValue;
        } else if (key.compare("VF0_HBM1_WRITE") == 0) {
            val = vF0Hbm1WriteValue;
        } else if (key.compare("VF0_TIMESTAMP_L") == 0) {
            val = vF0TimestampLValue;
        } else if (key.compare("VF0_TIMESTAMP_H") == 0) {
            val = vF0TimestampHValue;
        } else if (key.compare("VF0_HBM2_READ") == 0) {
            val = vF0Hbm2ReadValue;
        } else if (key.compare("VF0_HBM2_WRITE") == 0) {
            val = vF0Hbm2WriteValue;
        } else if (key.compare("VF0_HBM3_READ") == 0) {
            val = vF0Hbm3ReadValue;
        } else if (key.compare("VF0_HBM3_WRITE") == 0) {
            val = vF0Hbm3WriteValue;
        } else if (key.compare("VF0_HBM_READ_L") == 0) {
            val = vF0HbmLRead;
        } else if (key.compare("VF0_HBM_READ_H") == 0) {
            val = vF0HbmHRead;
        } else if (key.compare("VF0_HBM_WRITE_L") == 0) {
            val = vF0HbmLWrite;
        } else if (key.compare("VF0_HBM_WRITE_H") == 0) {
            val = vF0HbmHWrite;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return result;
    }

    ze_result_t readValue(const std::string key, uint64_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (key.compare("IDI_READS[0]") == 0 || key.compare("IDI_READS[1]") == 0 || key.compare("IDI_READS[2]") == 0 || key.compare("IDI_READS[3]") == 0 || key.compare("IDI_READS[4]") == 0 || key.compare("IDI_READS[5]") == 0 || key.compare("IDI_READS[6]") == 0 || key.compare("IDI_READS[7]") == 0 || key.compare("IDI_READS[8]") == 0 || key.compare("IDI_READS[9]") == 0 || key.compare("IDI_READS[10]") == 0 || key.compare("IDI_READS[11]") == 0 || key.compare("IDI_READS[12]") == 0 || key.compare("IDI_READS[13]") == 0 || key.compare("IDI_READS[14]") == 0 || key.compare("IDI_READS[15]") == 0) {
            val = mockIdiReadVal;
        } else if (key.compare("IDI_WRITES[0]") == 0 || key.compare("IDI_WRITES[1]") == 0 || key.compare("IDI_WRITES[2]") == 0 || key.compare("IDI_WRITES[3]") == 0 || key.compare("IDI_WRITES[4]") == 0 || key.compare("IDI_WRITES[5]") == 0 || key.compare("IDI_WRITES[6]") == 0 || key.compare("IDI_WRITES[7]") == 0 || key.compare("IDI_WRITES[8]") == 0 || key.compare("IDI_WRITES[9]") == 0 || key.compare("IDI_WRITES[10]") == 0 || key.compare("IDI_WRITES[11]") == 0 || key.compare("IDI_WRITES[12]") == 0 || key.compare("IDI_WRITES[13]") == 0 || key.compare("IDI_WRITES[14]") == 0 || key.compare("IDI_WRITES[15]") == 0) {
            val = mockIdiWriteVal;
        } else if (key.compare("DISPLAY_VC1_READS[0]") == 0 || key.compare("DISPLAY_VC1_READS[1]") == 0 || key.compare("DISPLAY_VC1_READS[2]") == 0 || key.compare("DISPLAY_VC1_READS[3]") == 0 || key.compare("DISPLAY_VC1_READS[4]") == 0 || key.compare("DISPLAY_VC1_READS[5]") == 0 || key.compare("DISPLAY_VC1_READS[6]") == 0 || key.compare("DISPLAY_VC1_READS[7]") == 0 || key.compare("DISPLAY_VC1_READS[8]") == 0 || key.compare("DISPLAY_VC1_READS[9]") == 0 || key.compare("DISPLAY_VC1_READS[10]") == 0 || key.compare("DISPLAY_VC1_READS[11]") == 0 || key.compare("DISPLAY_VC1_READS[12]") == 0 || key.compare("DISPLAY_VC1_READS[13]") == 0 || key.compare("DISPLAY_VC1_READS[14]") == 0 || key.compare("DISPLAY_VC1_READS[15]") == 0) {
            val = mockDisplayVc1ReadVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }
};

class SysmanProductHelperMemoryTest : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    DebugManagerStateRestore restorer;
    MockDrm *pDrm = nullptr;

    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        SysmanDeviceFixture::SetUp();
        device = pSysmanDeviceImp;
        auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(defaultHwInfo->platform.eProductFamily);
        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        pLinuxSysmanImp->pSysmanProductHelper = std::move(pSysmanProductHelper);
        pDrm = new MockDrm(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
        auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
        osInterface->setDriverModel(std::unique_ptr<MockDrm>(pDrm));
        pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.clear();
        auto subdeviceId = 0u;
        auto subDeviceCount = pLinuxSysmanImp->getSubDeviceCount();
        do {
            auto pPmt = new MockPmt();
            pLinuxSysmanImp->mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
        } while (++subdeviceId < subDeviceCount);
    }

    void TearDown() override {
        pLinuxSysmanImp->releasePmtObject();
        SysmanDeviceFixture::TearDown();
    }
};

HWTEST2_F(SysmanProductHelperMemoryTest, GivenSysmanProductHelperInstanceWhenCallingMemoryAPIsThenErrorIsReturned, IsPVC) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::ostringstream oStream;
        oStream << hbmRP0Frequency;
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
    EXPECT_EQ(properties.type, ZES_MEM_TYPE_HBM);
    EXPECT_EQ(properties.location, ZES_MEM_LOC_DEVICE);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
    EXPECT_EQ(properties.physicalSize, 0u);
    EXPECT_EQ(properties.numChannels, numMemoryChannels);
    EXPECT_EQ(properties.busWidth, memoryBusWidth);

    auto pPmt = static_cast<MockPmt *>(pLinuxSysmanImp->getPlatformMonitoringTechAccess(properties.subdeviceId));
    pPmt->setGuid(guid64BitMemoryCounters);

    zes_mem_bandwidth_t bandwidth;
    result = pSysmanProductHelper->getMemoryBandwidth(&bandwidth, pLinuxSysmanImp->getPlatformMonitoringTechAccess(subDeviceId), pSysmanDeviceImp, pLinuxSysmanImp->getSysmanKmdInterface(), subDeviceId);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
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

TEST(SysmanProductHelperTest, GivenInvalidProductFamilyWhenCallingProductHelperCreateThenNullPtrIsReturned) {
    auto pSysmanProductHelper = L0::Sysman::SysmanProductHelper::create(IGFX_UNKNOWN);
    EXPECT_EQ(nullptr, pSysmanProductHelper);
}

} // namespace ult
} // namespace Sysman
} // namespace L0