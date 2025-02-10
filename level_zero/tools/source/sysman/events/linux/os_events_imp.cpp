/*
 * Copyright (C) 2022-2025 Intel Corporation
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
#include "level_zero/tools/source/sysman/memory/linux/os_memory_imp.h"

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

    if (globalOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman driver not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    static_cast<LinuxSysmanDriverImp *>(globalOsSysmanDriver)->eventRegister(events, pLinuxSysmanImp->getSysmanDeviceImp());
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

    zes_event_type_flags_t prevRegisteredEvents = 0;
    if (deviceEventsMap.find(pSysmanDevice) != deviceEventsMap.end()) {
        prevRegisteredEvents = deviceEventsMap[pSysmanDevice];
    }

    eventsMutex.lock();
    if (!events) {
        // If user is trying to register events with empty events argument, then clear all the registered events
        if (deviceEventsMap.find(pSysmanDevice) != deviceEventsMap.end()) {
            deviceEventsMap[pSysmanDevice] = events;
        } else {
            deviceEventsMap.emplace(pSysmanDevice, events);
        }
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
        deviceEventsMap[pSysmanDevice] = registeredEvents;
    }

    // Write to Pipe only if eventregister() is called during listen and previously registered events are modified.
    if ((pipeFd[1] != -1) && (prevRegisteredEvents != deviceEventsMap[pSysmanDevice])) {
        uint8_t value = 0x00;
        if (NEO::SysCalls::write(pipeFd[1], &value, 1) < 0) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "%s", "Write to Pipe failed\n");
        }
    }
    eventsMutex.unlock();
}

void LinuxEventsUtil::init() {
    pUdevLib = static_cast<LinuxSysmanDriverImp *>(globalOsSysmanDriver)->getUdevLibHandle();
}

ze_result_t LinuxEventsUtil::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    memset(pEvents, 0, count * sizeof(zes_event_type_flags_t));
    *pNumDeviceEvents = 0;
    std::vector<zes_event_type_flags_t> registeredEvents(count);
    for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
        auto device = static_cast<SysmanDeviceImp *>(L0::SysmanDevice::fromHandle(phDevices[devIndex]));
        eventsMutex.lock();
        if (deviceEventsMap.find(device) != deviceEventsMap.end()) {
            registeredEvents[devIndex] = deviceEventsMap[device];
        }
        eventsMutex.unlock();
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

bool LinuxEventsUtil::checkIfFabricPortStatusChanged(void *dev, zes_event_type_flags_t &pEvent) {
    if (action.compare(change) != 0) {
        return false;
    }

    const char *str = pUdevLib->getEventPropertyValue(dev, "TYPE");
    if (str == nullptr) {
        return false;
    }

    std::string expectedStr = "PORT_CHANGE";
    if (expectedStr == str) {
        pEvent |= ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH;
        return true;
    }
    return false;
}

void LinuxEventsUtil::getDevIndexToDevPathMap(std::vector<zes_event_type_flags_t> &registeredEvents, uint32_t count, zes_device_handle_t *phDevices, std::map<uint32_t, std::string> &mapOfDevIndexToDevPath) {
    for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
        auto device = static_cast<SysmanDeviceImp *>(L0::SysmanDevice::fromHandle(phDevices[devIndex]));
        registeredEvents[devIndex] = deviceEventsMap[device];
        if (!registeredEvents[devIndex]) {
            continue;
        } else {
            std::string bdf;
            auto pSysfsAccess = &static_cast<L0::LinuxSysmanImp *>(device->deviceGetOsInterface())->getSysfsAccess();
            if (pSysfsAccess->getRealPath("device", bdf) == ZE_RESULT_SUCCESS) {
                // /sys needs to be removed from real path inorder to equate with
                // DEVPATH property of uevent.
                // Example of real path: /sys/devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0
                // Example of DEVPATH: /devices/pci0000:97/0000:97:02.0/0000:98:00.0/0000:99:01.0/0000:9a:00.0/i915.iaf.0
                const auto loc = bdf.find("/devices");
                if (loc == std::string::npos) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                          "%s", "Invalid device path\n");
                    continue;
                }

                bdf = bdf.substr(loc);
                mapOfDevIndexToDevPath.insert({devIndex, bdf});
            } else {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                      "%s", "Failed to get real path of device\n");
            }
        }
    }
}

bool LinuxEventsUtil::checkDeviceEvents(std::vector<zes_event_type_flags_t> &registeredEvents, std::map<uint32_t, std::string> mapOfDevIndexToDevPath, zes_event_type_flags_t *pEvents, void *dev) {
    const char *devicePath = pUdevLib->getEventPropertyValue(dev, "DEVPATH");
    bool retVal = false;
    if (devicePath != nullptr) {
        std::string devPath(devicePath);
        for (auto it = mapOfDevIndexToDevPath.begin(); it != mapOfDevIndexToDevPath.end(); it++) {
            if (devPath.find(it->second.c_str()) != std::string::npos) {
                if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_DETACH) {
                    if (checkDeviceDetachEvent(pEvents[it->first])) {
                        retVal = true;
                    }
                }
                if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH) {
                    if (checkDeviceAttachEvent(pEvents[it->first])) {
                        retVal = true;
                    }
                }
                if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED) {
                    if (isResetRequired(dev, pEvents[it->first])) {
                        retVal = true;
                    }
                }
                if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_MEM_HEALTH) {
                    if (checkIfMemHealthChanged(dev, pEvents[it->first])) {
                        retVal = true;
                    }
                }
                if (registeredEvents[it->first] & ZES_EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH) {
                    if (checkIfFabricPortStatusChanged(dev, pEvents[it->first])) {
                        retVal = true;
                    }
                }
                break;
            }
        }
    }
    return retVal;
}

bool LinuxEventsUtil::listenSystemEvents(zes_event_type_flags_t *pEvents, uint32_t count, std::vector<zes_event_type_flags_t> &registeredEvents, zes_device_handle_t *phDevices, uint64_t timeout) {
    std::call_once(initEventsOnce, [this]() {
        this->init();
    });

    bool retval = false;
    struct pollfd pfd[2];
    std::vector<std::string> subsystemList;
    std::map<uint32_t, std::string> mapOfDevIndexToDevPath = {};

    if (pUdevLib == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "libudev library instantiation failed\n");
        return retval;
    }

    subsystemList.push_back("drm");
    subsystemList.push_back("auxiliary");
    pfd[0].fd = pUdevLib->registerEventsFromSubsystemAndGetFd(subsystemList);
    pfd[0].events = POLLIN;
    pfd[0].revents = 0;

    eventsMutex.lock();
    if (NEO::SysCalls::pipe(pipeFd) < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Creation of pipe failed\n");
    }

    pfd[1].fd = pipeFd[0];
    pfd[1].events = POLLIN;
    pfd[1].revents = 0;

    auto start = L0::SteadyClock::now();
    std::chrono::duration<double, std::milli> timeElapsed;
    getDevIndexToDevPathMap(registeredEvents, count, phDevices, mapOfDevIndexToDevPath);
    eventsMutex.unlock();
    while (NEO::SysCalls::poll(pfd, 2, static_cast<int>(timeout)) > 0) {
        bool eventReceived = false;
        for (auto i = 0; i < 2; i++) {
            if (pfd[i].revents != 0) {
                if (pfd[i].fd == pipeFd[0]) {
                    eventsMutex.lock();
                    uint8_t dummy;
                    NEO::SysCalls::read(pipeFd[0], &dummy, 1);
                    mapOfDevIndexToDevPath.clear();
                    getDevIndexToDevPathMap(registeredEvents, count, phDevices, mapOfDevIndexToDevPath);
                    eventsMutex.unlock();
                } else {
                    eventReceived = true;
                }
            }
        }

        if (mapOfDevIndexToDevPath.empty()) {
            break;
        }

        if (!eventReceived) {
            timeElapsed = L0::SteadyClock::now() - start;
            if (timeout > timeElapsed.count()) {
                timeout = timeout - timeElapsed.count();
                continue;
            } else {
                break;
            }
        }

        void *dev = nullptr;
        dev = pUdevLib->allocateDeviceToReceiveData();
        if (dev == nullptr) {
            timeElapsed = L0::SteadyClock::now() - start;
            if (timeout > timeElapsed.count()) {
                timeout = timeout - timeElapsed.count();
                continue;
            } else {
                break;
            }
        }

        auto eventTypePtr = pUdevLib->getEventType(dev);
        if (eventTypePtr != nullptr) {
            action = std::string(eventTypePtr);
        } else {
            break;
        }

        retval = checkDeviceEvents(registeredEvents, mapOfDevIndexToDevPath, pEvents, dev);
        pUdevLib->dropDeviceReference(dev);
        if (retval) {
            break;
        }
        timeElapsed = L0::SteadyClock::now() - start;
        if (timeout > timeElapsed.count()) {
            timeout = timeout - timeElapsed.count();
            continue;
        } else {
            break;
        }
    }

    eventsMutex.lock();
    for (uint8_t i = 0; i < 2; i++) {
        if (pipeFd[i] != -1) {
            NEO::SysCalls::close(pipeFd[i]);
            pipeFd[i] = -1;
        }
    }
    eventsMutex.unlock();
    return retval;
}

} // namespace L0
