/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.h"
#include "level_zero/sysman/source/api/pci/sysman_pci_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::string deviceDir("device");
const std::string resourceFile("device/resource");
const std::string mockBdf = "0000:00:02.0";
const std::string mockRealPath = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/" + mockBdf;
const std::string mockRealPathConfig = mockRealPath + "/config";
const std::string mockRealPath2LevelsUp = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0";
const std::string mockRealPath2LevelsUpConfig = mockRealPath2LevelsUp + "/config";
constexpr std::string_view pcieDowngradeCapable = "device/auto_link_downgrade_capable";
constexpr std::string_view pcieDowngradeStatus = "device/auto_link_downgrade_status";

const std::vector<std::string> mockReadBytes =
    {
        "0x00000000bf000000 0x00000000bfffffff 0x0000000000140204",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000080000000 0x000000008fffffff 0x000000000014220c",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000004000 0x000000000000403f 0x0000000000040101",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x00000000000c0000 0x00000000000dffff 0x0000000000000212",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
        "0x0000000000000000 0x0000000000000000 0x0000000000000000",
};

struct MockPciSysfsAccess : public L0::Sysman::SysFsAccessInterface {

    bool isStringSymLinkEmpty = false;
    bool mockReadFailure = false;
    bool mockPcieDowngradeCapable = true;
    bool mockpcieDowngradeStatus = true;

    ze_result_t getValStringSymLinkEmpty(const std::string file, std::string &val) {
        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readSymLink(const std::string file, std::string &val) override {
        if (file.compare(deviceDir) == 0) {

            if (isStringSymLinkEmpty == true) {
                return getValStringSymLinkEmpty(file, val);
            }

            val = mockBdf;
            return ZE_RESULT_SUCCESS;
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getRealPath(const std::string file, std::string &val) override {
        if (file.compare(deviceDir) == 0) {
            val = mockRealPath;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare("device/config") == 0) {
            val = mockRealPathConfig;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, std::vector<std::string> &val) override {
        if (file.compare(resourceFile) == 0) {
            val = mockReadBytes;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {
        if (file.compare(pcieDowngradeCapable) == 0 && !mockReadFailure) {
            val = mockPcieDowngradeCapable;
            return ZE_RESULT_SUCCESS;
        }
        if (file.compare(pcieDowngradeStatus) == 0 && !mockReadFailure) {
            val = mockpcieDowngradeStatus;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ADDMETHOD_NOBASE(isRootUser, bool, true, ());

    MockPciSysfsAccess() = default;
};

class PublicLinuxPciImp : public L0::Sysman::LinuxPciImp {
  public:
    PublicLinuxPciImp(L0::Sysman::OsSysman *pOsSysman) : L0::Sysman::LinuxPciImp(pOsSysman) {}
    using L0::Sysman::LinuxPciImp::preadFunction;
    using L0::Sysman::LinuxPciImp::pSysfsAccess;
};

struct MockPcieDowngradeFwInterface : public L0::Sysman::FirmwareUtil {
    ze_result_t mockFwSetGfspConfigResult = ZE_RESULT_SUCCESS;
    ze_result_t mockFwGetGfspConfigResult = ZE_RESULT_SUCCESS;
    std::vector<uint8_t> mockBuf = {0, 0, 0, 0};

    ze_result_t fwSetGfspConfig(uint32_t gfspHeciCmdCode, std::vector<uint8_t> inBuf, std::vector<uint8_t> &outBuf) override {
        if (mockFwSetGfspConfigResult != ZE_RESULT_SUCCESS) {
            return mockFwSetGfspConfigResult;
        }

        mockBuf = inBuf;
        outBuf = mockBuf;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t fwGetGfspConfig(uint32_t gfspHeciCmdCode, std::vector<uint8_t> &outBuf) override {
        if (mockFwGetGfspConfigResult != ZE_RESULT_SUCCESS) {
            return mockFwGetGfspConfigResult;
        }

        outBuf = mockBuf;
        return ZE_RESULT_SUCCESS;
    }

    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(getFlashFirmwareProgress, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCompletionPercent));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwIfrApplied, ze_result_t, ZE_RESULT_SUCCESS, (bool &ifrStatus));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
    ADDMETHOD_NOBASE_VOIDRETURN(getLateBindingSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE(fwGetEccAvailable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pAvailable));
    ADDMETHOD_NOBASE(fwGetEccConfigurable, ze_result_t, ZE_RESULT_SUCCESS, (ze_bool_t * pConfigurable));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState, uint8_t *defaultState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));

    MockPcieDowngradeFwInterface() = default;
    ~MockPcieDowngradeFwInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
