/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/sysman/source/shared/linux/udev/udev_lib.h"

#include <cinttypes>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

typedef struct udev *(*pUdevNew)(void);
typedef struct udev_monitor *(*pUdevMonitorNewFromNetlink)(struct udev *, const char *);
typedef int (*pUdevMonitorFilterAddMatchSubsystemDevtype)(struct udev_monitor *udev_monitor, const char *subsystem, const char *devtype);
typedef int (*pUdevMonitorEnableReceiving)(struct udev_monitor *udev_monitor);
typedef int (*pUdevMonitorGetFd)(struct udev_monitor *udev_monitor);
typedef struct udev_device *(*pUdevMonitorReceiveDevice)(struct udev_monitor *udev_monitor);
typedef dev_t (*pUdevDeviceGetDevnum)(struct udev_device *udev_device);
typedef const char *(*pUdevDeviceGetAction)(struct udev_device *udev_device);
typedef const char *(*pUdevDeviceGetPropertyValue)(struct udev_device *udev_device, const char *key);
typedef struct udev_device *(*pUdevDeviceUnref)(struct udev_device *udev_device);

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
