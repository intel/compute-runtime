/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "os_interface.h"

namespace OCLRT {

bool OSInterface::osEnabled64kbPages = false;
bool OSInterface::osEnableLocalMemory = false;

OSInterface::OSInterface() {
    osInterfaceImpl = new OSInterfaceImpl();
}

OSInterface::~OSInterface() {
    delete osInterfaceImpl;
}

uint32_t OSInterface::getHwContextId() const {
    return 0;
}

bool OSInterface::are64kbPagesEnabled() {
    return osEnabled64kbPages;
}

uint32_t OSInterface::getDeviceHandle() const {
    return 0;
}

} // namespace OCLRT