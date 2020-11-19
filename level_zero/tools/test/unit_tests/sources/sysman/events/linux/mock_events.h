/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/events/events_imp.h"
#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

namespace L0 {
namespace ult {

const std::string ueventWedgedFile("/var/lib/libze_intel_gpu/wedged_file");
const std::string ueventDetachFile("/var/lib/libze_intel_gpu/remove-pci-0000_03_00_0");
const std::string ueventAttachFile("/var/lib/libze_intel_gpu/add-pci-0000_03_00_0");
const std::string deviceDir("device");

class EventsFsAccess : public FsAccess {};

template <>
struct Mock<EventsFsAccess> : public EventsFsAccess {

    ze_result_t getValReturnValAsOne(const std::string file, uint32_t &val) {
        if (file.compare(ueventWedgedFile) == 0) {
            val = 1;
        } else if (file.compare(ueventDetachFile) == 0) {
            val = 1;
        } else if (file.compare(ueventAttachFile) == 0) {
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
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getValFileNotFound(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    ze_result_t getValFileInsufficientPermissions(const std::string file, uint32_t &val) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }

    Mock<EventsFsAccess>() = default;

    MOCK_METHOD(ze_result_t, read, (const std::string file, uint32_t &val), (override));
    MOCK_METHOD(ze_result_t, canWrite, (const std::string file), (override));
};

class EventsSysfsAccess : public SysfsAccess {};
template <>
struct Mock<EventsSysfsAccess> : public EventsSysfsAccess {
    MOCK_METHOD(ze_result_t, readSymLink, (const std::string file, std::string &buf), (override));
    ze_result_t getValStringSymLinkSuccess(const std::string file, std::string &val) {
        if (file.compare(deviceDir) == 0) {
            val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    ze_result_t getValStringSymLinkFailure(const std::string file, std::string &val) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    Mock<EventsSysfsAccess>() = default;
};

class PublicLinuxEventsImp : public L0::LinuxEventsImp {
  public:
    PublicLinuxEventsImp(OsSysman *pOsSysman) : LinuxEventsImp(pOsSysman) {}
    using LinuxEventsImp::getPciIdPathTag;
    using LinuxEventsImp::memHealthAtEventRegister;
    using LinuxEventsImp::pciIdPathTag;
};

} // namespace ult
} // namespace L0
