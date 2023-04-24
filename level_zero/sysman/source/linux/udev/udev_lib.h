/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
class UdevLib {
  public:
    UdevLib() = default;
    static UdevLib *create();
    virtual int registerEventsFromSubsystemAndGetFd(std::vector<std::string> &subsystemList) = 0;
    virtual dev_t getEventGenerationSourceDevice(void *dev) = 0;
    virtual const char *getEventType(void *dev) = 0;
    virtual const char *getEventPropertyValue(void *dev, const char *key) = 0;
    virtual void *allocateDeviceToReceiveData() = 0;
    virtual void dropDeviceReference(void *dev) = 0;
    virtual ~UdevLib() {}
};
} // namespace Sysman
} // namespace L0
