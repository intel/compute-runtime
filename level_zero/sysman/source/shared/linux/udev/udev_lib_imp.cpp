/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/udev/udev_lib_imp.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/directory.h"

namespace L0 {
namespace Sysman {
static constexpr std::string_view libUdevlFile = "libudev.so";
const std::string udevNewRoutine = "udev_new";
const std::string udevMonitorNewFromNetlinkRoutine = "udev_monitor_new_from_netlink";
const std::string udevMonitorFilterAddMatchSubsystemDevtypeRoutine = "udev_monitor_filter_add_match_subsystem_devtype";
const std::string udevMonitorEnableReceivingRoutine = "udev_monitor_enable_receiving";
const std::string udevMonitorGetFdRoutine = "udev_monitor_get_fd";
const std::string udevMonitorReceiveDeviceRoutine = "udev_monitor_receive_device";
const std::string udevDeviceGetDevnumRoutine = "udev_device_get_devnum";
const std::string udevDeviceGetActionRoutine = "udev_device_get_action";
const std::string udevDeviceGetPropertyValueRoutine = "udev_device_get_property_value";
const std::string udevDeviceUnrefRoutine = "udev_device_unref";

bool UdevLibImp::loadEntryPoints() {
    bool ok = getSymbolAddr(udevMonitorNewFromNetlinkRoutine, pUdevMonitorNewFromNetlinkEntry);
    ok = ok && getSymbolAddr(udevNewRoutine, pUdevNewEntry);
    ok = ok && getSymbolAddr(udevMonitorFilterAddMatchSubsystemDevtypeRoutine, pUdevMonitorFilterAddMatchSubsystemDevtypeEntry);
    ok = ok && getSymbolAddr(udevMonitorEnableReceivingRoutine, pUdevMonitorEnableReceivingEntry);
    ok = ok && getSymbolAddr(udevMonitorGetFdRoutine, pUdevMonitorGetFdEntry);
    ok = ok && getSymbolAddr(udevMonitorReceiveDeviceRoutine, pUdevMonitorReceiveDeviceEntry);
    ok = ok && getSymbolAddr(udevDeviceGetDevnumRoutine, pUdevDeviceGetDevnumEntry);
    ok = ok && getSymbolAddr(udevDeviceGetActionRoutine, pUdevDeviceGetActionEntry);
    ok = ok && getSymbolAddr(udevDeviceGetPropertyValueRoutine, pUdevDeviceGetPropertyValueEntry);
    ok = ok && getSymbolAddr(udevDeviceUnrefRoutine, pUdevDeviceUnrefEntry);

    return ok;
}

const char *UdevLibImp::getEventPropertyValue(void *dev, const char *key) {
    return pUdevDeviceGetPropertyValueEntry(reinterpret_cast<struct udev_device *>(dev), key);
}

const char *UdevLibImp::getEventType(void *dev) {
    return pUdevDeviceGetActionEntry(reinterpret_cast<struct udev_device *>(dev));
}

void *UdevLibImp::allocateDeviceToReceiveData() {
    struct udev_device *dev = pUdevMonitorReceiveDeviceEntry(mon);
    return reinterpret_cast<void *>(dev);
}

dev_t UdevLibImp::getEventGenerationSourceDevice(void *dev) {
    return pUdevDeviceGetDevnumEntry(reinterpret_cast<struct udev_device *>(dev));
}

int UdevLibImp::registerEventsFromSubsystemAndGetFd(std::vector<std::string> &subsystemList) {
    for (const auto &subsystem : subsystemList) {
        pUdevMonitorFilterAddMatchSubsystemDevtypeEntry(mon, subsystem.c_str(), NULL);
    }
    if (pUdevMonitorEnableReceivingEntry(mon) != 0) {
        return -1;
    }
    return pUdevMonitorGetFdEntry(mon);
}

void UdevLibImp::dropDeviceReference(void *dev) {
    pUdevDeviceUnrefEntry(reinterpret_cast<struct udev_device *>(dev));
}

bool UdevLibImp::init() {
    mon = pUdevMonitorNewFromNetlinkEntry(pUdevNewEntry(), "kernel");
    return (mon != nullptr);
}

UdevLibImp::~UdevLibImp(){};

UdevLib *UdevLib::create() {
    UdevLibImp *pUdevLib = new UdevLibImp();
    UNRECOVERABLE_IF(nullptr == pUdevLib);
    pUdevLib->libraryHandle.reset(NEO::OsLibrary::loadFunc(std::string(libUdevlFile)));
    if (pUdevLib->libraryHandle == nullptr || pUdevLib->loadEntryPoints() == false || !pUdevLib->init()) {
        delete pUdevLib;
        return nullptr;
    }
    return static_cast<UdevLib *>(pUdevLib);
}

} // namespace Sysman
} // namespace L0
