/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

#include "sysman/events/events_imp.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxEventsImp::varFs("/var/lib/libze_intel_gpu/");
const std::string LinuxEventsImp::detachEvent("remove");
const std::string LinuxEventsImp::attachEvent("add");

bool LinuxEventsImp::isResetRequired(zes_event_type_flags_t &pEvent) {
    zes_device_state_t pState = {};
    pLinuxSysmanImp->getSysmanDeviceImp()->deviceGetState(&pState);
    if (pState.reset) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
        return true;
    }
    return false;
}

bool LinuxEventsImp::checkDeviceDetachEvent(zes_event_type_flags_t &pEvent) {
    // When device detach uevent is generated, then L0 udev rules will create a file:
    // /var/lib/libze_intel_gpu/remove-<ID_PATH_TAG>
    // For <ID_PATH_TAG>, check comment in LinuxEventsImp::init()
    const std::string deviceDetachFile = detachEvent + "-" + pciIdPathTag;
    const std::string deviceDetachFileAbsolutePath = varFs + deviceDetachFile;
    uint32_t val = 0;
    auto result = pFsAccess->read(deviceDetachFileAbsolutePath, val);
    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (val == 1) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_DETACH;
        return true;
    }
    return false;
}

bool LinuxEventsImp::checkDeviceAttachEvent(zes_event_type_flags_t &pEvent) {
    // When device detach uevent is generated, then L0 udev rules will create a file:
    // /var/lib/libze_intel_gpu/add-<ID_PATH_TAG>
    // For <ID_PATH_TAG>, check comment in LinuxEventsImp::init()
    const std::string deviceAttachFile = attachEvent + "-" + pciIdPathTag;
    const std::string deviceAttachFileAbsolutePath = varFs + deviceAttachFile;
    uint32_t val = 0;
    auto result = pFsAccess->read(deviceAttachFileAbsolutePath, val);
    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (val == 1) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH;
        return true;
    }
    return false;
}

bool LinuxEventsImp::checkIfMemHealthChanged(zes_event_type_flags_t &pEvent) {
    if (currentMemHealth() != memHealthAtEventRegister) {
        pEvent |= ZES_EVENT_TYPE_FLAG_MEM_HEALTH;
        return true;
    }
    return false;
}

bool LinuxEventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
        if (isResetRequired(pEvent)) {
            registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED); //After receiving event unregister it
            return true;
        }
    }
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
        if (checkDeviceDetachEvent(pEvent)) {
            registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH);
            return true;
        }
    }
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
        if (checkDeviceAttachEvent(pEvent)) {
            registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH);
            return true;
        }
    }
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
        if (checkIfMemHealthChanged(pEvent)) {
            registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_MEM_HEALTH);
            return true;
        }
    }
    return false;
}

ze_result_t LinuxEventsImp::eventRegister(zes_event_type_flags_t events) {
    if (0x7fff < events) {
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }
    registeredEvents |= events;
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
        memHealthAtEventRegister = currentMemHealth();
    }
    return ZE_RESULT_SUCCESS;
}

zes_mem_health_t LinuxEventsImp::currentMemHealth() {
    return ZES_MEM_HEALTH_UNKNOWN;
}

void LinuxEventsImp::getPciIdPathTag() {
    std::string bdfDir;
    ze_result_t result = pSysfsAccess->readSymLink("device", bdfDir);
    if (ZE_RESULT_SUCCESS != result) {
        return;
    }
    const auto loc = bdfDir.find_last_of('/');
    auto bdf = bdfDir.substr(loc + 1);
    std::replace(bdf.begin(), bdf.end(), ':', '_');
    std::replace(bdf.begin(), bdf.end(), '.', '_');
    // ID_PATH_TAG key is received when uevent related to device add/remove is generated.
    // Example of ID_PATH_TAG is:
    // ID_PATH_TAG=pci-0000_8c_00_0
    pciIdPathTag = "pci-" + bdf;
}

LinuxEventsImp::LinuxEventsImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    getPciIdPathTag();
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    LinuxEventsImp *pLinuxEventsImp = new LinuxEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pLinuxEventsImp);
}

} // namespace L0
