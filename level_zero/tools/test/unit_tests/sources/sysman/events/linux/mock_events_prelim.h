/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/tools/source/sysman/events/events_imp.h"
#include "level_zero/tools/source/sysman/events/linux/os_events_imp_prelim.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_driver_imp.h"

namespace L0 {
namespace ult {

const std::string ueventWedgedFile("/var/lib/libze_intel_gpu/wedged_file");
const std::string ueventDetachFile("/var/lib/libze_intel_gpu/remove-pci-0000_03_00_0");
const std::string ueventAttachFile("/var/lib/libze_intel_gpu/add-pci-0000_03_00_0");
const std::string ueventFabricFile("/var/lib/libze_intel_gpu/fabric-pci-0000_03_00_0");
const std::string deviceDir("device");
const std::string deviceMemoryHealth("device_memory_health");
const std::string eventsDir("/sys/devices/i915_0000_03_00.0/events");
constexpr int64_t mockPmuFd = 10;
constexpr uint64_t errorCount = 10u;
constexpr uint64_t mockTimeStamp = 1100u;

struct MockPmuInterfaceImpForEvents : public PmuInterfaceImp {
    using PmuInterfaceImp::perfEventOpen;
    MockPmuInterfaceImpForEvents(LinuxSysmanImp *pLinuxSysmanImp) : PmuInterfaceImp(pLinuxSysmanImp) {}

    bool mockPmuReadFail = false;

    int64_t perfEventOpen(perf_event_attr *attr, pid_t pid, int cpu, int groupFd, uint64_t flags) override {
        return mockPmuFd;
    }

    int pmuRead(int fd, uint64_t *data, ssize_t sizeOfdata) override {

        if (mockPmuReadFail == true) {
            return mockedPmuReadAndFailureReturn(fd, data, sizeOfdata);
        }

        data[0] = 0;
        data[1] = mockTimeStamp;
        data[2] = errorCount;
        return 0;
    }
    int mockedPmuReadAndFailureReturn(int fd, uint64_t *data, ssize_t sizeOfdata) {
        return -1;
    }
};

struct MockEventsFsAccess : public FsAccess {

    bool mockReadValSuccess = false;
    bool mockReadValOne = false;
    bool mockReadValZero = false;
    bool mockFileNotFoundError = false;
    ze_result_t mockListDirectoryResult = ZE_RESULT_SUCCESS;

    ze_result_t getValReturnValAsOne(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 1;
        } else if (file.compare(ueventDetachFile) == 0) {
            val = 1;
        } else if (file.compare(ueventAttachFile) == 0) {
            val = 1;
        } else if (file.compare(ueventFabricFile) == 0) {
            val = 1;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValReturnValAsZero(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 0;
        } else if (file.compare(ueventDetachFile) == 0) {
            val = 0;
        } else if (file.compare(ueventAttachFile) == 0) {
            val = 0;
        } else if (file.compare(ueventFabricFile) == 0) {
            val = 0;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, std::string &config) override {
        if (file.compare(eventsDir + "/" + "error--correctable-eu-grf") == 0) {
            config = "config=0x0000000000000001";
            return ZE_RESULT_SUCCESS;
        } else if (file.compare(eventsDir + "/" + "error--engine-reset") == 0) {
            config = "config=0x000000000000010";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValFileNotFound(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValFileInsufficientPermissions(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    ze_result_t listDirectory(const std::string directory, std::vector<std::string> &events) override {
        if (mockListDirectoryResult != ZE_RESULT_SUCCESS) {
            return mockListDirectoryResult;
        }

        if (directory.compare(eventsDir) == 0) {
            events.push_back("error--correctable-eu-grf");
            events.push_back("error--engine-reset");
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t readValSuccess(const std::string file, uint32_t &val) {
        val = 23;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t read(const std::string file, uint32_t &val) override {

        if (mockReadValOne == true) {
            return getValReturnValAsOne(file, val);
        }

        else if (mockReadValZero == true) {
            return getValReturnValAsZero(file, val);
        }

        else if (mockFileNotFoundError == true) {
            return getValFileNotFound(file, val);
        }

        else if (mockReadValSuccess == true) {
            val = 23;
            return readValSuccess(file, val);
        }

        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    bool isRootUser() override {
        return true;
    }
    bool userIsNotRoot() {
        return false;
    }

    MockEventsFsAccess() = default;
};

struct MockEventsSysfsAccess : public SysfsAccess {

    bool mockSymLinkFailure = false;
    bool mockReadMemHealthDegraded = false;
    bool mockReadCurrMemHealth = false;

    ze_result_t getRealPath(const std::string file, std::string &val) override {
        if (file.compare("device/") == 0) {
            val = "/sys/devices/pci0000:00/0000:00:02.0";
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readSymLink(const std::string file, std::string &val) override {

        if (mockSymLinkFailure == true) {
            return getValStringSymLinkFailure(file, val);
        }

        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValStringSymLinkFailure(const std::string file, std::string &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t read(const std::string file, uint64_t &val) override {
        if (file.compare("gt/gt0/error_counter/correctable_eu_grf") == 0) {
            val = 5u;
            return ZE_RESULT_SUCCESS;
        } else if (file.compare("gt/gt0/error_counter/engine_reset") == 0) {
            val = 8u;
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    MockEventsSysfsAccess() = default;
};

struct MockEventsFwInterface : public FirmwareUtil {
    bool mockIfrStatus = false;
    ze_result_t fwIfrApplied(bool &ifrStatus) override {
        ifrStatus = mockIfrStatus;
        return ZE_RESULT_SUCCESS;
    }
    MockEventsFwInterface() = default;

    ADDMETHOD_NOBASE(fwDeviceInit, ze_result_t, ZE_RESULT_SUCCESS, (void));
    ADDMETHOD_NOBASE(getFirstDevice, ze_result_t, ZE_RESULT_SUCCESS, (igsc_device_info * info));
    ADDMETHOD_NOBASE(getFwVersion, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, std::string &firmwareVersion));
    ADDMETHOD_NOBASE(flashFirmware, ze_result_t, ZE_RESULT_SUCCESS, (std::string fwType, void *pImage, uint32_t size));
    ADDMETHOD_NOBASE(fwSupportedDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::vector<std::string> & supportedDiagTests));
    ADDMETHOD_NOBASE(fwRunDiagTests, ze_result_t, ZE_RESULT_SUCCESS, (std::string & osDiagType, zes_diag_result_t *pResult));
    ADDMETHOD_NOBASE(fwGetMemoryErrorCount, ze_result_t, ZE_RESULT_SUCCESS, (zes_ras_error_type_t category, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count));
    ADDMETHOD_NOBASE(fwGetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t * currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE(fwSetEccConfig, ze_result_t, ZE_RESULT_SUCCESS, (uint8_t newState, uint8_t *currentState, uint8_t *pendingState));
    ADDMETHOD_NOBASE_VOIDRETURN(getDeviceSupportedFwTypes, (std::vector<std::string> & fwTypes));
    ADDMETHOD_NOBASE_VOIDRETURN(fwGetMemoryHealthIndicator, (zes_mem_health_t * health));
};

class PublicLinuxEventsImp : public L0::LinuxEventsImp {
  public:
    PublicLinuxEventsImp(OsSysman *pOsSysman) : LinuxEventsImp(pOsSysman) {}
};

class PublicLinuxEventsUtil : public L0::LinuxEventsUtil {
  public:
    PublicLinuxEventsUtil() : LinuxEventsUtil() {}
    using LinuxEventsUtil::listenSystemEvents;
    using LinuxEventsUtil::pUdevLib;
};

} // namespace ult
} // namespace L0
