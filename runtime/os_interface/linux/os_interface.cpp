/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "os_interface.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;

OSInterface::OSInterface() {
    osInterfaceImpl = new OSInterfaceImpl();
}

OSInterface::~OSInterface() {
    delete osInterfaceImpl;
}

bool OSInterface::are64kbPagesEnabled() {
    return osEnabled64kbPages;
}

uint32_t OSInterface::getDeviceHandle() const {
    return 0;
}

} // namespace NEO
