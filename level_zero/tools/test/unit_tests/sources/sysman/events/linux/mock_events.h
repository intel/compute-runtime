/*
 * Copyright (C) 2020-2022 Intel Corporation
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

struct MockEventsFsAccess : public EventsFsAccess {
    std::vector<ze_result_t> mockReadStatus{ZE_RESULT_SUCCESS};
    uint32_t mockReadVal = 2;

    ze_result_t read(const std::string file, uint32_t &val) override {
        ze_result_t returnValue = ZE_RESULT_SUCCESS;

        if (!mockReadStatus.empty()) {
            returnValue = mockReadStatus.front();
            if (returnValue != ZE_RESULT_SUCCESS) {
                return returnValue;
            }
            mockReadStatus.erase(mockReadStatus.begin());
        }

        val = mockReadVal;
        return returnValue;
    }

    MockEventsFsAccess() = default;
};

class EventsSysfsAccess : public SysfsAccess {};

struct MockEventsSysfsAccess : public EventsSysfsAccess {
    ze_result_t mockReadSymLinkFailureError = ZE_RESULT_SUCCESS;

    ze_result_t readSymLink(const std::string file, std::string &val) override {
        if (mockReadSymLinkFailureError != ZE_RESULT_SUCCESS) {
            return mockReadSymLinkFailureError;
        }

        val = "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0";
        return ZE_RESULT_SUCCESS;
    }

    MockEventsSysfsAccess() = default;
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
