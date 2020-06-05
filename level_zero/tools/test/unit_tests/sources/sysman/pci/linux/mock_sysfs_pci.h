/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/pci/pci_imp.h"

using ::testing::_;

namespace L0 {
namespace ult {

const std::string deviceDir("device");
const std::string resourceFile("device/resource");
const std::string maxLinkSpeedFile("device/max_link_speed");
const std::string maxLinkWidthFile("device/max_link_width");
const std::string mockBdf = "0000:00:02.0";
constexpr double mockMaxLinkSpeed = 2.5;
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

class PciSysfsAccess : public SysfsAccess {};

template <>
struct Mock<PciSysfsAccess> : public SysfsAccess {
    int mockMaxLinkWidth = 0;
    MOCK_METHOD(ze_result_t, read, (const std::string file, double &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, int &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, std::vector<std::string> &val), (override));
    MOCK_METHOD(ze_result_t, readSymLink, (const std::string file, std::string &buf), (override));

    ze_result_t getValDouble(const std::string file, double &val) {
        if (file.compare(maxLinkSpeedFile) == 0) {
            val = mockMaxLinkSpeed;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setValInt(const std::string file, int val) {
        if (file.compare(maxLinkWidthFile) == 0) {
            mockMaxLinkWidth = val;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValInt(const std::string file, int &val) {
        if (file.compare(maxLinkWidthFile) == 0) {
            val = mockMaxLinkWidth;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValString(const std::string file, std::string &val) {
        if (file.compare(deviceDir) == 0) {
            val = mockBdf;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValVector(const std::string file, std::vector<std::string> &val) {
        if (file.compare(resourceFile) == 0) {
            val = mockReadBytes;
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<PciSysfsAccess>() = default;
};

class PublicLinuxPciImp : public L0::LinuxPciImp {
  public:
    using LinuxPciImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
