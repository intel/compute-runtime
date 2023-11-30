/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.h"
#include "level_zero/sysman/source/api/pci/sysman_pci_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"

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

    ADDMETHOD_NOBASE(isRootUser, bool, true, ());

    MockPciSysfsAccess() = default;
};

class PublicLinuxPciImp : public L0::Sysman::LinuxPciImp {
  public:
    PublicLinuxPciImp(L0::Sysman::OsSysman *pOsSysman) : L0::Sysman::LinuxPciImp(pOsSysman) {}
    using L0::Sysman::LinuxPciImp::preadFunction;
    using L0::Sysman::LinuxPciImp::pSysfsAccess;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
