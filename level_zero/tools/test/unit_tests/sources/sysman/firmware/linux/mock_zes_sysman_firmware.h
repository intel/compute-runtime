/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"
#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

namespace L0 {
namespace ult {

constexpr uint32_t mockHandleCount = 1;
const std::string mockFwVersion("DG01->0->2026");

class FirmwareInterface : public FirmwareUtil {};
template <>
struct Mock<FirmwareInterface> : public FirmwareUtil {

    ze_result_t mockFwDeviceInit(void) {
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockFwDeviceInitFail(void) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    ze_result_t mockFwGetVersion(std::string &fwVersion) {
        fwVersion = mockFwVersion;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t mockGetFirstDevice(igsc_device_info *info) {
        return ZE_RESULT_SUCCESS;
    }
    Mock<FirmwareInterface>() = default;

    MOCK_METHOD(ze_result_t, fwDeviceInit, (), (override));
    MOCK_METHOD(ze_result_t, fwGetVersion, (std::string & fwVersion), (override));
    MOCK_METHOD(ze_result_t, getFirstDevice, (igsc_device_info * info), (override));
};

class PublicLinuxFirmwareImp : public L0::LinuxFirmwareImp {
  public:
    using LinuxFirmwareImp::pFwInterface;
};
} // namespace ult
} // namespace L0
