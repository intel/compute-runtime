/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/os_interface.h"

#include "core/os_interface/linux/drm_neo.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = false;

OSInterface::OSInterfaceImpl::OSInterfaceImpl() = default;
OSInterface::OSInterfaceImpl::~OSInterfaceImpl() = default;
void OSInterface::OSInterfaceImpl::setDrm(Drm *drm) {
    this->drm.reset(drm);
}

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

void OSInterface::setGmmInputArgs(void *args) {}

} // namespace NEO
