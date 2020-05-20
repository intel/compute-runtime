/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/sysman_device/linux/os_sysman_device_imp.h"

#include "sysman/linux/fs_access.h"
#include "sysman/sysman.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("Unknown");
const std::string intelPciId("0x8086");
const std::string deviceDir("device");
const std::string vendorFile("device/vendor");
const std::string deviceFile("device/device");
const std::string subsystemVendorFile("device/subsystem_vendor");
const std::string driverFile("device/driver");
const std::string functionLevelReset("device/reset");
const std::string clientsDir("clients");
constexpr uint64_t pid1 = 1711u;
constexpr uint64_t pid2 = 1722u;
constexpr uint64_t engineTimeSpent = 123456u;
const std::string clientId1("4");
const std::string clientId2("5");
const std::string clientId3("6");
const std::string engine0("0");
const std::string engine1("1");
const std::string engine2("2");
const std::string engine3("3");

class SysmanDeviceSysfsAccess : public SysfsAccess {};

template <>
struct Mock<SysmanDeviceSysfsAccess> : public SysfsAccess {
    ze_result_t getValString(const std::string file, std::string &val) {
        if (file.compare(subsystemVendorFile) == 0) {
            val = "0x8086";
        }
        if (file.compare(deviceFile) == 0) {
            val = "0x3ea5";
        }
        if (file.compare(vendorFile) == 0) {
            val = "0x8086";
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        if ((file.compare("clients/4/pid") == 0) || (file.compare("clients/5/pid") == 0)) {
            val = pid1;
        }
        if (file.compare("clients/6/pid") == 0) {
            val = pid2;
        }
        if ((file.compare("clients/4/busy/0") == 0) || (file.compare("clients/4/busy/3") == 0) || (file.compare("clients/5/busy/1") == 0) || (file.compare("clients/6/busy/0") == 0)) {
            val = engineTimeSpent;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDirEntries(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId1);
            list.push_back(clientId2);
            list.push_back(clientId3);
        }
        if ((path.compare("clients/4/busy") == 0) || (path.compare("clients/5/busy") == 0) || (path.compare("clients/6/busy") == 0)) {
            list.push_back(engine0);
            list.push_back(engine1);
            list.push_back(engine2);
            list.push_back(engine3);
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock() = default;
    ~Mock() override = default;

    MOCK_METHOD2(read, ze_result_t(const std::string file, std::string &val));
    MOCK_METHOD2(read, ze_result_t(const std::string file, uint64_t &val));
    MOCK_METHOD2(scanDirEntries, ze_result_t(const std::string path, std::vector<std::string> &list));
};

class PublicLinuxSysmanDeviceImp : public L0::LinuxSysmanDeviceImp {
  public:
    using LinuxSysmanDeviceImp::pLinuxSysmanImp;
    using LinuxSysmanDeviceImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif