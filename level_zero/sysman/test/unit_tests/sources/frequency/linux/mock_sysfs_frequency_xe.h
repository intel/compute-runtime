/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/sysman/source/api/frequency/linux/sysman_os_frequency_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/kmd_interface/mock_sysman_kmd_interface_xe.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string minFreqFile("device/tile0/gt0/freq0/min_freq");
const std::string minFreqFileMedia("device/tile0/gt1/freq0/min_freq");
const std::string maxFreqFile("device/tile0/gt0/freq0/max_freq");
const std::string maxFreqFileMedia("device/tile0/gt1/freq0/max_freq");
const std::string requestFreqFile("device/tile0/gt0/freq0/cur_freq");
const std::string requestFreqFileMedia("device/tile0/gt1/freq0/cur_freq");
const std::string actualFreqFile("device/tile0/gt0/freq0/act_freq");
const std::string actualFreqFileMedia("device/tile0/gt1/freq0/act_freq");
const std::string efficientFreqFile("device/tile0/gt0/freq0/rpe_freq");
const std::string efficientFreqFileMedia("device/tile0/gt1/freq0/rpe_freq");
const std::string maxValFreqFile("device/tile0/gt0/freq0/rp0_freq");
const std::string maxValFreqFileMedia("device/tile0/gt1/freq0/rp0_freq");
const std::string minValFreqFile("device/tile0/gt0/freq0/rpn_freq");
const std::string minValFreqFileMedia("device/tile0/gt1/freq0/rpn_freq");
const std::string throttleReasonStatusFile("device/tile0/gt0/freq0/throttle/status");
const std::string throttleReasonStatusFileMedia("device/tile0/gt1/freq0/throttle/status");
const std::string throttleReasonPL1File("device/tile0/gt0/freq0/throttle/reason_pl1");
const std::string throttleReasonPL1FileMedia("device/tile0/gt1/freq0/throttle/reason_pl1");
const std::string throttleReasonPL2File("device/tile0/gt0/freq0/throttle/reason_pl2");
const std::string throttleReasonPL2FileMedia("device/tile0/gt1/freq0/throttle/reason_pl2");
const std::string throttleReasonPL4File("device/tile0/gt0/freq0/throttle/reason_pl4");
const std::string throttleReasonPL4FileMedia("device/tile0/gt1/freq0/throttle/reason_pl4");
const std::string throttleReasonThermalFile("device/tile0/gt0/freq0/throttle/reason_thermal");
const std::string throttleReasonThermalFileMedia("device/tile0/gt1/freq0/throttle/reason_thermal");
const std::string throttleReasonFile("device/tile0/gt0/freq0/throttle_reason");
const std::string throttleReasonFileMedia("device/tile0/gt1/freq0/throttle_reason");
const std::string detailedThrottleReasonStatusFile("device/tile0/gt0/freq0/throttle/status");
const std::string detailedThrottleReasonCardPL1File("device/tile0/gt0/freq0/throttle/reason_psys_pl1");
const std::string detailedThrottleReasonFastVMode("device/tile0/gt0/freq0/throttle/reason_fastvmode");
const std::string detailedThrottleReasonMemoryThermalFile("device/tile0/gt0/freq0/throttle/reason_memory_thermal");
const std::string detailedThrottleReasonVoltageP0File("device/tile0/gt0/freq0/throttle/reason_p0_freq");
const std::string ratlReasonFile("device/tile0/gt0/freq0/throttle/reason_ratl");

struct MockXeFrequencySysfsAccess : public L0::Sysman::SysFsAccessInterface {
    std::string throttleReason = {};

    double mockMin = 0;
    double mockMax = 0;
    double mockRequest = 0;
    double mockActual = 0;
    double mockEfficient = 0;
    double mockMaxVal = 0;
    double mockMinVal = 0;
    uint32_t throttleVal = 0;
    uint32_t throttleReasonPL1Val = 0;
    uint32_t throttleReasonPL2Val = 0;
    uint32_t throttleReasonPL4Val = 0;
    uint32_t throttleReasonThermalVal = 0;
    uint32_t throttleReasonVal = 0;
    uint32_t detailedThrottleReasonVal = 0;
    uint32_t throttleReasonRatlVal = 0;
    bool mockRatlReasonReadError = false;

    ze_result_t mockReadDoubleValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadRequestResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadEfficientResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadActualResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMinValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxValResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadVal32Result = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMaxResult = ZE_RESULT_SUCCESS;
    ze_result_t mockWriteMinResult = ZE_RESULT_SUCCESS;
    ze_result_t mockReadStringResult = ZE_RESULT_SUCCESS;

    bool mockReadPL1Error = false;
    bool mockReadPL2Error = false;
    bool mockReadPL4Error = false;
    bool mockReadThermalError = false;
    bool mockThrottleReasonStatusReadSuccess = true;
    bool mockThrottleReasonStatusReadForceSuccess = false;
    bool mockCardPL1DetailedReasonReadSuccess = true;
    bool mockMemoryThermalDetailedReasonReadSuccess = true;
    bool mockFastVModeDetailedReasonReadSuccess = true;
    bool mockVoltageP0DetailedReasonReadSuccess = true;
    bool mockDetailedReasonReadSuccess = true;
    bool mockDirectoryExists = true;
    bool mockMediaDirectoryExists = true;

    ze_result_t setValU32(const std::string file, uint32_t val) {
        if ((file.compare(throttleReasonStatusFile) == 0) || (file.compare(throttleReasonStatusFileMedia) == 0)) {
            throttleVal = val;
        }
        if ((file.compare(throttleReasonPL1File) == 0) || (file.compare(throttleReasonPL1FileMedia) == 0)) {
            throttleReasonPL1Val = val;
        }
        if ((file.compare(throttleReasonPL2File) == 0) || (file.compare(throttleReasonPL2FileMedia) == 0)) {
            throttleReasonPL2Val = val;
        }
        if ((file.compare(throttleReasonPL4File) == 0) || (file.compare(throttleReasonPL4FileMedia) == 0)) {
            throttleReasonPL4Val = val;
        }
        if ((file.compare(throttleReasonThermalFile) == 0) || (file.compare(throttleReasonThermalFileMedia) == 0)) {
            throttleReasonThermalVal = val;
        }
        if (file.compare(detailedThrottleReasonStatusFile) == 0) {
            throttleReasonVal = val;
        }
        if ((file.compare(detailedThrottleReasonCardPL1File) == 0) || (file.compare(detailedThrottleReasonFastVMode) == 0) ||
            (file.compare(detailedThrottleReasonMemoryThermalFile) == 0) || (file.compare(detailedThrottleReasonVoltageP0File) == 0)) {
            detailedThrottleReasonVal = val;
        }
        if (file.compare(ratlReasonFile) == 0) {
            throttleReasonRatlVal = val;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, double &val) override {

        if (mockReadDoubleValResult != ZE_RESULT_SUCCESS) {
            return mockReadDoubleValResult;
        }
        if ((file.compare(minFreqFile) == 0) || (file.compare(minFreqFileMedia) == 0)) {
            val = mockMin;
        } else if ((file.compare(maxFreqFile) == 0) || (file.compare(maxFreqFileMedia) == 0)) {
            if (mockReadMaxResult != ZE_RESULT_SUCCESS) {
                return mockReadMaxResult;
            }
            val = mockMax;
        } else if ((file.compare(requestFreqFile) == 0) || (file.compare(requestFreqFileMedia) == 0)) {
            if (mockReadRequestResult != ZE_RESULT_SUCCESS) {
                return mockReadRequestResult;
            }
            val = mockRequest;
        } else if ((file.compare(actualFreqFile) == 0) || (file.compare(actualFreqFileMedia) == 0)) {
            if (mockReadActualResult != ZE_RESULT_SUCCESS) {
                return mockReadActualResult;
            }
            val = mockActual;
        } else if ((file.compare(efficientFreqFile) == 0) || (file.compare(efficientFreqFileMedia) == 0)) {
            if (mockReadEfficientResult != ZE_RESULT_SUCCESS) {
                return mockReadEfficientResult;
            }
            val = mockEfficient;
        } else if ((file.compare(maxValFreqFile) == 0) || (file.compare(maxValFreqFileMedia) == 0)) {
            if (mockReadMaxValResult != ZE_RESULT_SUCCESS) {
                return mockReadMaxValResult;
            }
            val = mockMaxVal;
        } else if ((file.compare(minValFreqFile) == 0) || (file.compare(minValFreqFileMedia) == 0)) {
            if (mockReadMinValResult != ZE_RESULT_SUCCESS) {
                return mockReadMinValResult;
            }
            val = mockMinVal;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setVal(const std::string file, const double val) {
        if ((file.compare(minFreqFile) == 0) || (file.compare(minFreqFileMedia) == 0)) {
            if (mockWriteMinResult != ZE_RESULT_SUCCESS) {
                return mockWriteMinResult;
            }
            mockMin = val;
        }
        if ((file.compare(maxFreqFile) == 0) || (file.compare(maxFreqFileMedia) == 0)) {
            if (mockWriteMaxResult != ZE_RESULT_SUCCESS) {
                return mockWriteMaxResult;
            }
            mockMax = val;
        }
        if ((file.compare(requestFreqFile) == 0) || (file.compare(requestFreqFileMedia) == 0)) {
            mockRequest = val;
        }
        if ((file.compare(actualFreqFile) == 0) || (file.compare(actualFreqFileMedia) == 0)) {
            mockActual = val;
        }
        if ((file.compare(efficientFreqFile) == 0) || (file.compare(efficientFreqFileMedia) == 0)) {
            mockEfficient = val;
        }
        if ((file.compare(maxValFreqFile) == 0) || (file.compare(maxValFreqFileMedia) == 0)) {
            mockMaxVal = val;
        }
        if ((file.compare(minValFreqFile) == 0) || (file.compare(minValFreqFileMedia) == 0)) {
            mockMinVal = val;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValU32(const std::string file, uint32_t &val) {
        if ((file.compare(throttleReasonStatusFile) == 0) || (file.compare(throttleReasonStatusFileMedia) == 0)) {
            val = throttleVal;
        }
        if ((file.compare(throttleReasonPL1File) == 0) || (file.compare(throttleReasonPL1FileMedia) == 0)) {
            if (mockReadPL1Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL1Val;
        }
        if ((file.compare(throttleReasonPL2File) == 0) || (file.compare(throttleReasonPL2FileMedia) == 0)) {
            if (mockReadPL2Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL2Val;
        }
        if ((file.compare(throttleReasonPL4File) == 0) || (file.compare(throttleReasonPL4FileMedia) == 0)) {
            if (mockReadPL4Error) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonPL4Val;
        }
        if ((file.compare(throttleReasonThermalFile) == 0) || (file.compare(throttleReasonThermalFileMedia) == 0)) {
            if (mockReadThermalError) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonThermalVal;
        }
        if (file.compare(ratlReasonFile) == 0) {
            if (mockRatlReasonReadError) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            val = throttleReasonRatlVal;
            return ZE_RESULT_SUCCESS;
        }
        if ((file.compare(detailedThrottleReasonStatusFile) == 0) || mockThrottleReasonStatusReadForceSuccess) {
            mockThrottleReasonStatusReadForceSuccess = false;
            val = throttleReasonVal;
            return mockThrottleReasonStatusReadSuccess ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        if (((file.compare(detailedThrottleReasonCardPL1File) == 0) && mockCardPL1DetailedReasonReadSuccess) ||
            ((file.compare(detailedThrottleReasonFastVMode) == 0) && mockFastVModeDetailedReasonReadSuccess) ||
            ((file.compare(detailedThrottleReasonMemoryThermalFile) == 0) && mockMemoryThermalDetailedReasonReadSuccess) ||
            ((file.compare(detailedThrottleReasonVoltageP0File) == 0) && mockVoltageP0DetailedReasonReadSuccess)) {
            val = detailedThrottleReasonVal;
            return mockDetailedReasonReadSuccess ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (mockReadVal32Result != ZE_RESULT_SUCCESS) {
            return mockReadVal32Result;
        }
        return getValU32(file, val);
    }

    ze_result_t write(const std::string &file, double val) override {
        return setVal(file, val);
    }

    ze_result_t read(const std::string file, std::string &val) override {

        if (mockReadStringResult != ZE_RESULT_SUCCESS) {
            return mockReadStringResult;
        }

        if (file.compare(throttleReasonFile) == 0) {
            val = throttleReason;
        }

        return ZE_RESULT_SUCCESS;
    }

    bool directoryExists(const std::string path) override {
        if (path.find("gt1") != std::string::npos) {
            return mockMediaDirectoryExists;
        }
        return mockDirectoryExists;
    }

    MockXeFrequencySysfsAccess() = default;
    ~MockXeFrequencySysfsAccess() override = default;
};

class PublicLinuxFrequencyImp : public L0::Sysman::LinuxFrequencyImp {
  public:
    PublicLinuxFrequencyImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_freq_domain_t type) : L0::Sysman::LinuxFrequencyImp(pOsSysman, onSubdevice, subdeviceId, type) {}
    using L0::Sysman::LinuxFrequencyImp::getMaxVal;
    using L0::Sysman::LinuxFrequencyImp::getMin;
    using L0::Sysman::LinuxFrequencyImp::getMinVal;
    using L0::Sysman::LinuxFrequencyImp::pSysfsAccess;
};

constexpr double minFreq = 300.0;
constexpr double maxFreq = 1100.0;
constexpr double request = 300.0;
constexpr double actual = 300.0;
constexpr double efficient = 300.0;
constexpr double maxVal = 1100.0;
constexpr double minVal = 300.0;

class SysmanDeviceFrequencyFixtureXe : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;
    MockSysmanKmdInterfaceXe *pSysmanKmdInterface = nullptr;
    uint32_t numClocks = 0;
    double step = 0;
    uint32_t handleComponentCount = 1u;
    MockXeFrequencySysfsAccess *sysfsAccess = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;

        pSysmanKmdInterface = new MockSysmanKmdInterfaceXe(pLinuxSysmanImp->getSysmanProductHelper());
        pSysmanKmdInterface->pSysfsAccess = std::make_unique<MockXeFrequencySysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysmanKmdInterface->pSysfsAccess.get();
        pLinuxSysmanImp->pSysmanKmdInterface.reset(pSysmanKmdInterface);

        auto &rootDeviceEnvironment = pLinuxSysmanImp->getParentSysmanDeviceImp()->getRootDeviceEnvironmentRef();
        if (rootDeviceEnvironment.getHardwareInfo()->capabilityTable.supportsImages) {
            handleComponentCount = 2;
        }

        sysfsAccess = static_cast<MockXeFrequencySysfsAccess *>(pSysmanKmdInterface->pSysfsAccess.get());
        sysfsAccess->setVal(minFreqFile, minFreq);
        sysfsAccess->setVal(minFreqFileMedia, minFreq);
        sysfsAccess->setVal(maxFreqFile, maxFreq);
        sysfsAccess->setVal(maxFreqFileMedia, maxFreq);
        sysfsAccess->setVal(requestFreqFile, request);
        sysfsAccess->setVal(requestFreqFileMedia, request);
        sysfsAccess->setVal(actualFreqFile, actual);
        sysfsAccess->setVal(actualFreqFileMedia, actual);
        sysfsAccess->setVal(efficientFreqFile, efficient);
        sysfsAccess->setVal(efficientFreqFileMedia, efficient);
        sysfsAccess->setVal(maxValFreqFile, maxVal);
        sysfsAccess->setVal(maxValFreqFileMedia, maxVal);
        sysfsAccess->setVal(minValFreqFile, minVal);
        sysfsAccess->setVal(minValFreqFileMedia, minVal);
        step = 50;
        numClocks = static_cast<uint32_t>((maxFreq - minFreq) / step) + 1;
        for (auto handle : pSysmanDeviceImp->pFrequencyHandleContext->handleList) {
            delete handle;
        }
        pSysmanDeviceImp->pFrequencyHandleContext->handleList.clear();
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }

    double clockValue(const double calculatedClock) {
        uint32_t actualClock = static_cast<uint32_t>(calculatedClock + 0.5);
        return static_cast<double>(actualClock);
    }

    std::vector<zes_freq_handle_t> getFreqHandles(uint32_t count) {
        std::vector<zes_freq_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumFrequencyDomains(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
