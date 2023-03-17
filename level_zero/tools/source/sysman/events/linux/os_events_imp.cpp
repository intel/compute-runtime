/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/events/linux/os_events_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/tools/source/sysman/events/events_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_driver_imp.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp_prelim.h"

#include <sys/stat.h>

namespace L0 {

const std::string LinuxEventsUtil::add("add");
const std::string LinuxEventsUtil::remove("remove");
const std::string LinuxEventsUtil::change("change");
const std::string LinuxEventsUtil::unbind("unbind");
const std::string LinuxEventsUtil::bind("bind");

bool LinuxEventsImp::eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    // This is dummy implementation, Actual implementation is handled at driver level
    // for all devices.
    return false;
}

ze_result_t LinuxEventsImp::eventRegister(zes_event_type_flags_t events) {
    if (0x7fff < events) {
        return ZE_RESULT_ERROR_INVALID_ENUMERATION;
    }

    if (GlobalOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman driver not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    static_cast<LinuxSysmanDriverImp *>(GlobalOsSysmanDriver)->eventRegister(events, pLinuxSysmanImp->getSysmanDeviceImp());
    return ZE_RESULT_SUCCESS;
}

LinuxEventsImp::LinuxEventsImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
}

OsEvents *OsEvents::create(OsSysman *pOsSysman) {
    LinuxEventsImp *pLinuxEventsImp = new LinuxEventsImp(pOsSysman);
    return static_cast<OsEvents *>(pLinuxEventsImp);
}

bool LinuxEventsUtil::checkRasEvent(zes_event_type_flags_t &pEvent, SysmanDeviceImp *pSysmanDeviceImp, zes_event_type_flags_t registeredEvents) {
    for (auto rasHandle : pSysmanDeviceImp->pRasHandleContext->handleList) {
        zes_ras_properties_t properties = {};
        rasHandle->rasGetProperties(&properties);
        if ((registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) && (properties.type == ZES_RAS_ERROR_TYPE_CORRECTABLE)) {
            if (LinuxEventsUtil::checkRasEventOccured(rasHandle) == true) {
                pEvent |= ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS;
                return true;
            }
        }
        if ((registeredEvents & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS) && (properties.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE)) {
            if (LinuxEventsUtil::checkRasEventOccured(rasHandle) == true) {
                pEvent |= ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS;
                return true;
            }
        }
    }
    return false;
}

bool LinuxEventsUtil::checkRasEventOccured(Ras *rasHandle) {
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

void LinuxEventsUtil::eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice) {
    std::call_once(initEventsOnce, [this]() {
        this->init();
    });

    if (!events) {
        // If user is trying to register events with empty events argument, then clear all the registered events
        deviceEventsMap.emplace(pSysmanDevice, events);
    } else {
        zes_event_type_flags_t registeredEvents = 0;
        // supportedEventMask --> this mask checks for events that supported currently
        zes_event_type_flags_t supportedEventMask = ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH | ZES_EVENT_TYPE_FLAG_DEVICE_DETACH |
                                                    ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH | ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED |
                                                    ZES_EVENT_TYPE_FLAG_MEM_HEALTH | ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
                                                    ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS;
        if (deviceEventsMap.find(pSysmanDevice) != deviceEventsMap.end()) {
            registeredEvents = deviceEventsMap[pSysmanDevice];
        }
        registeredEvents |= (events & supportedEventMask);
        deviceEventsMap.emplace(pSysmanDevice, registeredEvents);
    }
}

void LinuxEventsUtil::init() {
    pUdevLib = static_cast<LinuxSysmanDriverImp *>(GlobalOsSysmanDriver)->getUdevLibHandle();
}

ze_result_t LinuxEventsUtil::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    memset(pEvents, 0, count * sizeof(zes_event_type_flags_t));
    std::vector<zes_event_type_flags_t> registeredEvents(count);
    for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
        auto device = static_cast<SysmanDeviceImp *>(L0::SysmanDevice::fromHandle(phDevices[devIndex]));
        if (deviceEventsMap.find(device) != deviceEventsMap.end()) {
            registeredEvents[devIndex] = deviceEventsMap[device];
        }
        if (registeredEvents[devIndex]) {
            if ((registeredEvents[devIndex] & ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS) || (registeredEvents[devIndex] & ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS)) {
                if (checkRasEvent(pEvents[devIndex], device, registeredEvents[devIndex])) {
                    *pNumDeviceEvents = 1;
                    return ZE_RESULT_SUCCESS;
                }
            }

            if (registeredEvents[devIndex] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                zes_device_state_t deviceState = {};
                device->pGlobalOperations->deviceGetState(&deviceState);
                if (deviceState.reset & ZES_RESET_REASON_FLAG_REPAIR) {
                    pEvents[devIndex] |= ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
                    *pNumDeviceEvents = 1;
                    return ZE_RESULT_SUCCESS;
                }
            }
        }
    }

    if (listenSystemEvents(pEvents, count, registeredEvents, phDevices, timeout)) {
        *pNumDeviceEvents = 1;
    }

    return ZE_RESULT_SUCCESS;
}

bool LinuxEventsUtil::isResetRequired(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }

    std::vector<std::string> properties{"RESET_FAILED", "RESET_REQUIRED"};
    for (auto &property : properties) {
        const char *propVal = nullptr;
        propVal = pUdevLib->getEventPropertyValue(dev, property.c_str());
        if (propVal && atoi(propVal) == 1) {
            pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED;
            return true;
        }
    }

    return false;
}

bool LinuxEventsUtil::checkDeviceDetachEvent(zes_event_type_flags_t &pEvent) {
    if (action.compare(remove) == 0) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_DETACH;
        return true;
    }
    return false;
}

bool LinuxEventsUtil::checkDeviceAttachEvent(zes_event_type_flags_t &pEvent) {
    if (action.compare(add) == 0) {
        pEvent |= ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH;
        return true;
    }
    return false;
}

bool LinuxEventsUtil::checkIfMemHealthChanged(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }

    std::vector<std::string> properties{"MEM_HEALTH_ALARM", "RESET_REQUIRED", "EC_PENDING", "DEGRADED", "EC_FAILED", "SPARING_STATUS_UNKNOWN"};
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

bool LinuxEventsUtil::listenSystemEvents(zes_event_type_flags_t *pEvents, uint32_t count, std::vector<zes_event_type_flags_t> &registeredEvents, zes_device_handle_t *phDevices, uint64_t timeout) {
    std::call_once(initEventsOnce, [this]() {
        this->init();
    });

    bool retval = false;
    struct pollfd pfd;
    std::vector<std::string> subsystemList;
    std::multimap<uint32_t, dev_t> mapOfDevIndexToDevNum;

    if (pUdevLib == nullptr) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "libudev library instantiation failed\n");
        return retval;
    }

    subsystemList.push_back("drm");
    pfd.fd = pUdevLib->registerEventsFromSubsystemAndGetFd(subsystemList);
    pfd.events = POLLIN;

    auto start = L0::steadyClock::now();
    std::chrono::duration<double, std::milli> timeElapsed;
    while (NEO::SysCalls::poll(&pfd, 1, static_cast<int>(timeout)) > 0) {
        mapOfDevIndexToDevNum.clear();
        for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
            auto device = static_cast<SysmanDeviceImp *>(L0::SysmanDevice::fromHandle(phDevices[devIndex]));
            registeredEvents[devIndex] = deviceEventsMap[device];
            if (!registeredEvents[devIndex]) {
                continue;
            } else {
                auto pDrm = &static_cast<L0::LinuxSysmanImp *>(device->deviceGetOsInterface())->getDrm();
                struct stat drmStat = {};
                NEO::SysCalls::fstat(pDrm->getFileDescriptor(), &drmStat);
                mapOfDevIndexToDevNum.insert({devIndex, drmStat.st_rdev});
                mapOfDevIndexToDevNum.insert({devIndex, drmStat.st_rdev - 127});
            }
        }

        if (mapOfDevIndexToDevNum.empty()) {
            break;
        }

        dev_t devnum;
        void *dev = nullptr;
        dev = pUdevLib->allocateDeviceToReceiveData();
        if (dev == nullptr) {
            timeElapsed = L0::steadyClock::now() - start;
            if (timeout > timeElapsed.count()) {
                timeout = timeout - timeElapsed.count();
                continue;
            } else {
                break;
            }
        }

        devnum = pUdevLib->getEventGenerationSourceDevice(dev);
        for (auto it = mapOfDevIndexToDevNum.begin(); it != mapOfDevIndexToDevNum.end(); it++) {
            if (it->second == devnum) {
                auto eventTypePtr = pUdevLib->getEventType(dev);
                if (eventTypePtr != nullptr) {
                    action = std::string(eventTypePtr);
                    if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                        if (checkDeviceDetachEvent(pEvents[it->first])) {
                            retval = true;
                        }
                    }
                    if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                        if (checkDeviceAttachEvent(pEvents[it->first])) {
                            retval = true;
                        }
                    }
                    if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                        if (isResetRequired(dev, pEvents[it->first])) {
                            retval = true;
                        }
                    }
                    if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
                        if (checkIfMemHealthChanged(dev, pEvents[it->first])) {
                            retval = true;
                        }
                    }
                }
                break;
            }
        }

        pUdevLib->dropDeviceReference(dev);
        if (retval) {
            break;
        }
        timeElapsed = L0::steadyClock::now() - start;
        if (timeout > timeElapsed.count()) {
            timeout = timeout - timeElapsed.count();
            continue;
        } else {
            break;
        }
    }
    return retval;
}

} // namespace L0
