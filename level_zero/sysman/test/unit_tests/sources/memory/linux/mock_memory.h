/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/api/memory/linux/sysman_os_memory_imp.h"
#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

using namespace NEO;

constexpr uint64_t transactionSize = 32;
constexpr uint64_t hbmRP0Frequency = 4200;
constexpr uint64_t mockMaxBwDg2 = 1343616u;

constexpr uint32_t vFHbmLRead = 16;
constexpr uint32_t vFHbmHRead = 2;
constexpr uint32_t vFHbmLWrite = 8;
constexpr uint32_t vFHbmHWrite = 2;

constexpr uint32_t vF0Vfid = 88;
constexpr uint32_t vF1Vfid = 176;
constexpr uint32_t vF0HbmReadL = 384;
constexpr uint32_t vF0HbmReadH = 388;
constexpr uint32_t vF0HbmWriteL = 392;
constexpr uint32_t vF0HbmWriteH = 396;

constexpr uint32_t vF1HbmReadL = 400;
constexpr uint32_t vF1HbmReadH = 404;
constexpr uint32_t vF1HbmWriteL = 408;
constexpr uint32_t vF1HbmWriteH = 412;

constexpr uint32_t vF0Hbm0Read = 92;
constexpr uint32_t vF0Hbm0Write = 96;
constexpr uint32_t vF0Hbm1Read = 104;
constexpr uint32_t vF0Hbm1Write = 108;
constexpr uint32_t vF0Hbm2Read = 312;
constexpr uint32_t vF0Hbm2Write = 316;
constexpr uint32_t vF0Hbm3Read = 328;
constexpr uint32_t vF0Hbm3Write = 332;

constexpr uint32_t vFHbm0ReadValue = 92;
constexpr uint32_t vFHbm0WriteValue = 96;
constexpr uint32_t vFHbm1ReadValue = 104;
constexpr uint32_t vFHbm1WriteValue = 108;
constexpr uint32_t vFHbm2ReadValue = 113;
constexpr uint32_t vFHbm2WriteValue = 125;
constexpr uint32_t vFHbm3ReadValue = 135;
constexpr uint32_t vFHbm3WriteValue = 20;

constexpr uint32_t minIdiReadOffset = 1096;
constexpr uint32_t minIdiWriteOffset = 1224;
constexpr uint32_t minDisplayVc1ReadOffset = 1352;

constexpr uint64_t mockIdiReadVal = 8u;
constexpr uint64_t mockIdiWriteVal = 9u;
constexpr uint64_t mockDisplayVc1ReadVal = 10u;
constexpr uint64_t numberMcChannels = 16;

namespace L0 {
namespace Sysman {
namespace ult {

const std::string deviceMemoryHealth("device_memory_health");
const std::string telem1NodeName("/sys/class/intel_pmt/telem1");
const std::string telem2NodeName("/sys/class/intel_pmt/telem2");
const std::string telem3NodeName("/sys/class/intel_pmt/telem3");
const std::string telem1OffsetFileName("/sys/class/intel_pmt/telem1/offset");
const std::string telem1GuidFileName("/sys/class/intel_pmt/telem1/guid");
const std::string telem1TelemFileName("/sys/class/intel_pmt/telem1/telem");
const std::string telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");
const std::string telem3OffsetFileName("/sys/class/intel_pmt/telem3/offset");
const std::string telem3GuidFileName("/sys/class/intel_pmt/telem3/guid");
const std::string telem3TelemFileName("/sys/class/intel_pmt/telem3/telem");
const std::string hbmFreqFilePath("gt/gt0/mem_RP0_freq_mhz");
const std::string hbmFreqFilePath2("gt/gt1/mem_RP0_freq_mhz");
const std::string maxBwFileName("prelim_lmem_max_bw_Mbps");

struct MockMemoryNeoDrm : public NEO::Drm {
    using Drm::ioctlHelper;
    const int mockFd = 33;
    uint32_t mockMemoryType = NEO::DeviceBlobConstants::MemoryType::hbm2e;
    std::vector<bool> mockQuerySystemInfoReturnValue{};
    bool isRepeated = false;
    bool mockReturnEmptyRegions = false;
    MockMemoryNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    void setMemoryType(uint32_t memory) {
        mockMemoryType = memory;
    }

    std::vector<uint8_t> getMemoryRegionsReturnsEmpty() {
        return {};
    }

    void resetSystemInfo() {
        systemInfo.reset(nullptr);
    }

    bool querySystemInfo() override {
        bool returnValue = true;
        if (!mockQuerySystemInfoReturnValue.empty()) {
            returnValue = mockQuerySystemInfoReturnValue.front();
            if (isRepeated != true) {
                mockQuerySystemInfoReturnValue.erase(mockQuerySystemInfoReturnValue.begin());
            }
            resetSystemInfo();
            return returnValue;
        }

        constexpr uint32_t numHbmStacksPerTile = 2;
        constexpr uint32_t numChannelsPerHbmStack = 4;

        uint32_t hwBlob[] = {NEO::DeviceBlobConstants::maxMemoryChannels, 1, 8, NEO::DeviceBlobConstants::memoryType, 1, mockMemoryType, NEO::DeviceBlobConstants::numHbmStacksPerTile, 1, numHbmStacksPerTile, NEO::DeviceBlobConstants::numChannelsPerHbmStack, 1, numChannelsPerHbmStack};
        std::vector<uint32_t> inputBlobData(reinterpret_cast<uint32_t *>(hwBlob), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob, sizeof(hwBlob))));
        this->systemInfo.reset(new SystemInfo(inputBlobData));
        return returnValue;
    }
};

class PublicLinuxMemoryImp : public L0::Sysman::LinuxMemoryImp {
  public:
    PublicLinuxMemoryImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId) {}
    PublicLinuxMemoryImp() = default;
    using L0::Sysman::LinuxMemoryImp::pSysmanKmdInterface;
};

struct MockMemorySysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
    std::vector<ze_result_t> mockReadReturnStatus{};
    std::vector<std::string> mockReadStringValue{};
    bool isRepeated = false;

    ze_result_t read(const std::string file, std::string &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;
        if (!mockReadReturnStatus.empty()) {
            result = mockReadReturnStatus.front();
            if (!mockReadStringValue.empty()) {
                val = mockReadStringValue.front();
            }

            if (isRepeated != true) {
                mockReadReturnStatus.erase(mockReadReturnStatus.begin());
                if (!mockReadStringValue.empty()) {
                    mockReadStringValue.erase(mockReadStringValue.begin());
                }
            }
            return result;
        }

        if (file.compare(deviceMemoryHealth) == 0) {
            val = "OK";
        }
        return result;
    }
};

class MockProcFsAccessInterface : public L0::Sysman::ProcFsAccessInterface {
  public:
    MockProcFsAccessInterface() = default;
    ~MockProcFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
