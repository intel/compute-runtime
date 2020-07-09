/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/global_operations/linux/os_global_operations_imp.h"

#include "sysman/sysman.h"

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
constexpr uint64_t pid3 = 1723u;
constexpr uint64_t engineTimeSpent = 123456u;
const std::string clientId1("4");
const std::string clientId2("5");
const std::string clientId3("6");
const std::string clientId4("7");
const std::string engine0("0");
const std::string engine1("1");
const std::string engine2("2");
const std::string engine3("3");

class GlobalOperationsSysfsAccess : public SysfsAccess {};

template <>
struct Mock<GlobalOperationsSysfsAccess> : public GlobalOperationsSysfsAccess {
    ze_result_t getValString(const std::string file, std::string &val) {
        if (file.compare(subsystemVendorFile) == 0) {
            val = "0x8086";
        } else if (file.compare(deviceFile) == 0) {
            val = "0x3ea5";
        } else if (file.compare(vendorFile) == 0) {
            val = "0x8086";
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValUnsignedLong(const std::string file, uint64_t &val) {
        if ((file.compare("clients/4/pid") == 0) || (file.compare("clients/5/pid") == 0)) {
            val = pid1;
        } else if (file.compare("clients/6/pid") == 0) {
            val = pid2;
        } else if (file.compare("clients/7/pid") == 0) {
            val = pid3;
        } else if ((file.compare("clients/4/busy/0") == 0) || (file.compare("clients/4/busy/3") == 0) ||
                   (file.compare("clients/5/busy/1") == 0) || (file.compare("clients/6/busy/0") == 0)) {
            val = engineTimeSpent;
        } else if ((file.compare("clients/4/busy/1") == 0) || (file.compare("clients/4/busy/2") == 0) ||
                   (file.compare("clients/5/busy/0") == 0) || (file.compare("clients/5/busy/2") == 0) ||
                   (file.compare("clients/7/busy/0") == 0) || (file.compare("clients/7/busy/2") == 0) ||
                   (file.compare("clients/5/busy/3") == 0) || (file.compare("clients/6/busy/1") == 0) ||
                   (file.compare("clients/6/busy/2") == 0) || (file.compare("clients/6/busy/3") == 0)) {
            val = 0;
        } else if ((file.compare("clients/4/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/5/total_device_memory_buffer_objects/created_bytes") == 0) ||
                   (file.compare("clients/6/total_device_memory_buffer_objects/created_bytes") == 0)) {
            val = 1024;
        } else if (file.compare("clients/7/total_device_memory_buffer_objects/created_bytes") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDir4Entries(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId1);
            list.push_back(clientId2);
            list.push_back(clientId3);
            list.push_back(clientId4);
        } else if ((path.compare("clients/4/busy") == 0) || (path.compare("clients/5/busy") == 0) ||
                   (path.compare("clients/6/busy") == 0) || (path.compare("clients/7/busy") == 0)) {
            list.push_back(engine0);
            list.push_back(engine1);
            list.push_back(engine2);
            list.push_back(engine3);
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getScannedDirEntries(const std::string path, std::vector<std::string> &list) {
        if (path.compare(clientsDir) == 0) {
            list.push_back(clientId1);
            list.push_back(clientId2);
            list.push_back(clientId3);
        } else if ((path.compare("clients/4/busy") == 0) || (path.compare("clients/5/busy") == 0) ||
                   (path.compare("clients/6/busy") == 0)) {
            list.push_back(engine0);
            list.push_back(engine1);
            list.push_back(engine2);
            list.push_back(engine3);
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    Mock<GlobalOperationsSysfsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, std::string &val), (override));
    MOCK_METHOD(ze_result_t, read, (const std::string file, uint64_t &val), (override));
    MOCK_METHOD(ze_result_t, scanDirEntries, (const std::string path, std::vector<std::string> &list), (override));
};

class PublicLinuxGlobalOperationsImp : public L0::LinuxGlobalOperationsImp {
  public:
    using LinuxGlobalOperationsImp::pLinuxSysmanImp;
    using LinuxGlobalOperationsImp::pSysfsAccess;
};

} // namespace ult
} // namespace L0
