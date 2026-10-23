/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/sysman/source/shared/linux/udev/udev_lib.h"

#include <cinttypes>
#include <libudev.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

using pUdevNew = decltype(&udev_new);
using pUdevMonitorNewFromNetlink = decltype(&udev_monitor_new_from_netlink);
using pUdevMonitorFilterAddMatchSubsystemDevtype = decltype(&udev_monitor_filter_add_match_subsystem_devtype);
using pUdevMonitorEnableReceiving = decltype(&udev_monitor_enable_receiving);
using pUdevMonitorGetFd = decltype(&udev_monitor_get_fd);
using pUdevMonitorReceiveDevice = decltype(&udev_monitor_receive_device);
using pUdevDeviceGetDevnum = decltype(&udev_device_get_devnum);
using pUdevDeviceGetAction = decltype(&udev_device_get_action);
using pUdevDeviceGetPropertyValue = decltype(&udev_device_get_property_value);
using pUdevDeviceUnref = decltype(&udev_device_unref);

class UdevLibImp : public UdevLib {
  public:
    UdevLibImp() = default;
    ~UdevLibImp() override;
    std::unique_ptr<NEO::OsLibrary> libraryHandle;
    bool loadEntryPoints();
    template <class T>
    bool getSymbolAddr(const std::string name, T &proc) {
        void *addr = libraryHandle->getProcAddress(name);
        proc = reinterpret_cast<T>(addr);
        return nullptr != proc;
    }
    bool init();
    int registerEventsFromSubsystemAndGetFd(std::vector<std::string> &subsystemList) override;
    dev_t getEventGenerationSourceDevice(void *dev) override;
    const char *getEventType(void *dev) override;
    const char *getEventPropertyValue(void *dev, const char *key) override;
    void *allocateDeviceToReceiveData() override;
    void dropDeviceReference(void *dev) override;

  protected:
    pUdevNew pUdevNewEntry = nullptr;
    pUdevMonitorNewFromNetlink pUdevMonitorNewFromNetlinkEntry = nullptr;
    pUdevMonitorFilterAddMatchSubsystemDevtype pUdevMonitorFilterAddMatchSubsystemDevtypeEntry = nullptr;
    pUdevMonitorEnableReceiving pUdevMonitorEnableReceivingEntry = nullptr;
    pUdevMonitorGetFd pUdevMonitorGetFdEntry = nullptr;
    pUdevMonitorReceiveDevice pUdevMonitorReceiveDeviceEntry = nullptr;
    pUdevDeviceGetDevnum pUdevDeviceGetDevnumEntry = nullptr;
    pUdevDeviceGetAction pUdevDeviceGetActionEntry = nullptr;
    pUdevDeviceGetPropertyValue pUdevDeviceGetPropertyValueEntry = nullptr;
    pUdevDeviceUnref pUdevDeviceUnrefEntry = nullptr;

  private:
    struct udev_monitor *mon = nullptr;
};

} // namespace Sysman
} // namespace L0
