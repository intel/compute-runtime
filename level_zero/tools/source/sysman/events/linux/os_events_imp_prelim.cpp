/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/linux/os_events_imp_prelim.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp_prelim.h"

#include "sysman/events/events_imp.h"
#include "sysman/linux/os_sysman_imp.h"

#include <sys/stat.h>

namespace L0 {

const std::string LinuxEventsImp::add("add");
const std::string LinuxEventsImp::remove("remove");
const std::string LinuxEventsImp::change("change");
const std::string LinuxEventsImp::unbind("unbind");
const std::string LinuxEventsImp::bind("bind");

static bool checkRasEventOccured(Ras *rasHandle) {
    zes_ras_config_t config = {};
    zes_ras_state_t state = {};
    rasHandle->rasGetConfig(&config);
    if (ZE_RESULT_SUCCESS == rasHandle->rasGetState(&state, 0)) {
        uint64_t totalCategoryThreshold = 0;
        for (int i = 0; i < ZES_MAX_RAS_ERROR_CATEGORY_COUNT; i++) {
            totalCategoryThreshold += state.category[i];
            if ((config.detailedThresholds.category[i] > 0) && (state.category[i] > config.detailedThresholds.category[i])) {
                return true;
            }
        }
        if ((config.totalThreshold > 0) && (totalCategoryThreshold > config.totalThreshold)) {
            return true;
        }
    }
    return false;
}

bool LinuxEventsImp::checkRasEvent(zes_event_type_flags_t &pEvent) {
    for (auto rasHandle : pLinuxSysmanImp->getSysmanDeviceImp()->pRasHandleContext->handleList) {
        zes_ras_properties_t properties = {};
        rasHandle->rasGetProperties(&properties);
        if ((registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) && (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE)) {
            if (checkRasEventOccured(rasHandle) == true) {
                pEvent |= ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS;
                return true;
            }
        }
        if ((registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS) && (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE)) {
            if (checkRasEventOccured(rasHandle) == true) {
                pEvent |= ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS;
                return true;
            }
        }
    }
    return false;
}

bool LinuxEventsImp::isResetRequired(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }
    const char *str;
    str = pUdevLib->getEventPropertyValue(dev, "RESET_FAILED");
    if (str && atoi(str) == 1) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
        return true;
    }

    return false;
}

bool LinuxEventsImp::checkDeviceDetachEvent(zes_event_type_flags_t &pEvent) {
    if (action.compare(remove) == 0) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_DETACH;
        return true;
    }
    return false;
}

bool LinuxEventsImp::checkDeviceAttachEvent(zes_event_type_flags_t &pEvent) {
    if (action.compare(add) == 0) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH;
        return true;
    }
    return false;
}

bool LinuxEventsImp::checkIfMemHealthChanged(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }
    std::vector<std::string> properties{"MEM_HEALTH_ALARM", "REBOOT_ALARM", "EC_PENDING", "DEGRADED", "EC_FAILED", "SPARING_STATUS_UNKNOWN"};
    for (auto &property : properties) {
        const char *propVal = nullptr;
        propVal = pUdevLib->getEventPropertyValue(dev, property.c_str());
        if (propVal && atoi(propVal) == 1) {
            pEvent |= ZES_EVENT_TYPE_FLAG_MEM_HEALTH;
            return true;
        }
    }
    return false;
}

bool LinuxEventsImp::checkIfFabricPortStatusChanged(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }

    const char *str = pUdevLib->getEventPropertyValue(dev, "TYPE");
    if (str == nullptr) {
        return false;
    }
    const char *expectedStr = "PORT_CHANGE";
    const size_t expectedStrLen = 11; // length of "PORT_CHANGE" is 11
    if (strlen(str) != strlen(expectedStr)) {
        return false;
    }
    if (strncmp(str, expectedStr, expectedStrLen) == 0) {
        pEvent |= ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH;
        return true;
    }
    return false;
}

ze_result_t LinuxEventsImp::readFabricDeviceStats(const std::string &devicePciPath, struct stat &iafStat) {
    const std::string iafDirectoryLegacy = "iaf.";
    const std::string iafDirectory = "i915.iaf.";
    const std::string fabricIdFile = "/iaf_fabric_id";
    int fd = -1;
    std::string path;
    path.clear();
    std::vector<std::string> list = NEO::Directory::getFiles(devicePciPath);
    for (auto entry : list) {
        if ((entry.find(iafDirectory) != std::string::npos) ||
            (entry.find(iafDirectoryLegacy) != std::string::npos)) {
            path = entry + fabricIdFile;
            break;
        }
    }
    if (path.empty()) {
        // This device does not have a fabric
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    fd = NEO::SysCalls::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    NEO::SysCalls::fstat(fd, &iafStat);
    NEO::SysCalls::close(fd);
    return ZE_RESULT_SUCCESS;
}

bool LinuxEventsImp::listenSystemEvents(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    bool retval = false;
    struct pollfd pfd;
    struct stat iafStat;
    std::vector<std::string> subsystemList;

    if (pUdevLib == nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "libudev library instantiation failed\n");
        return retval;
    }

    // Get fabric device stats
    const std::string iafPath = "device/";
    std::string iafRealPath = {};
    pLinuxSysmanImp->getSysfsAccess().getRealPath(iafPath, iafRealPath);
    auto result = readFabricDeviceStats(iafRealPath, iafStat);
    if (result == ZE_RESULT_SUCCESS) {
        subsystemList.push_back("platform");
    }

    subsystemList.push_back("drm");
    pfd.fd = pUdevLib->registerEventsFromSubsystemAndGetFd(subsystemList);
    pfd.events = POLLIN;

    auto pDrm = &pLinuxSysmanImp->getDrm();
    struct stat drmStat;
    NEO::SysCalls::fstat(pDrm->getFileDescriptor(), &drmStat);

    auto start = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> timeElapsed;
    while (NEO::SysCalls::poll(&pfd, 1, static_cast<int>(timeout)) > 0) {
        // Check again for registered events. Its possible while we are looping for events, registered events are cleared
        if (!registeredEvents) {
            return true;
        }

        dev_t devnum;
        void *dev = nullptr;
        dev = pUdevLib->allocateDeviceToReceiveData();
        if (dev == nullptr) {
            timeElapsed = std::chrono::steady_clock::now() - start;
            if (timeout > timeElapsed.count()) {
                timeout = timeout - timeElapsed.count();
                continue;
            } else {
                break;
            }
        }

        devnum = pUdevLib->getEventGenerationSourceDevice(dev);

        if ((memcmp(&drmStat.st_rdev, &devnum, sizeof(dev_t)) == 0) ||
            (memcmp(&iafStat.st_rdev, &devnum, sizeof(dev_t)) == 0)) {
            auto eventTypePtr = pUdevLib->getEventType(dev);

            if (eventTypePtr != nullptr) {
                action = std::string(eventTypePtr);
                if (registeredEvents & ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH) {
                    if (checkIfFabricPortStatusChanged(dev, pEvent)) {
                        registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH);
                        retval = true;
                    }
                }
                if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                    if (checkDeviceDetachEvent(pEvent)) {
                        registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_DETACH); // After receiving event unregister it
                        retval = true;
                    }
                }
                if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                    if (checkDeviceAttachEvent(pEvent)) {
                        registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH);
                        retval = true;
                    }
                }
                if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                    if (isResetRequired(dev, pEvent)) {
                        registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED);
                        retval = true;
                    }
                }
                if (registeredEvents & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
                    if (checkIfMemHealthChanged(dev, pEvent)) {
                        registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_MEM_HEALTH);
                        retval = true;
                    }
                }
            }
        }

        pUdevLib->dropDeviceReference(dev);
        if (retval) {
            break;
        }
        timeElapsed = std::chrono::steady_clock::now() - start;
        if (timeout > timeElapsed.count()) {
            timeout = timeout - timeElapsed.count();
            continue;
        } else {
            break;
        }
    }

    return retval;
}

bool LinuxEventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    bool retval = false;
    if (!registeredEvents) {
        return retval;
    }

    pEvent = 0;
    if ((registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) || (registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS)) {
        if (checkRasEvent(pEvent)) {
            if (pEvent & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) {
                registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS);
            } else {
                registeredEvents &= ~(ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS);
            }
            return true;
        }
    }

    // check if any reset required for reason field repair
    if (registeredEvents & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
        zes_device_state_t deviceState = {};
        pLinuxSysmanImp->getSysmanDeviceImp()->pGlobalOperations->deviceGetState(&deviceState);
        if (deviceState.reset & ZES_RESET_REASON_FLAG_REPAIR) {
            pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
            return true;
        }
    }

    return listenSystemEvents(pEvent, timeout);
}

ze_result_t LinuxEventsImp::eventRegister(zes_event_type_flags_t events) {
    if (0x7fff < events) {
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }
    if (!events) {
        // If user is trying to register events with empty events argument, then clear all the registered events
        registeredEvents = events;
    } else {
        // supportedEventMask --> this mask checks for events that supported currently
        zes_event_type_flags_t supportedEventMask = ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                                    ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED |
                                                    ZES_EVENT_TYPE_FLAG_MEM_HEALTH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                                    ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS;
        registeredEvents |= (events & supportedEventMask);
    }
    return ZE_RESULT_SUCCESS;
}

LinuxEventsImp::LinuxEventsImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    pUdevLib = pLinuxSysmanImp->getUdevLibHandle();
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    LinuxEventsImp *pLinuxEventsImp = new LinuxEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pLinuxEventsImp);
}

} // namespace L0
