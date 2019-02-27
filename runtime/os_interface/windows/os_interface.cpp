/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "os_interface.h"

#include "runtime/os_interface/windows/sys_calls.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {

bool OSInterface::osEnabled64kbPages = true;

OSInterface::OSInterface() {
    osInterfaceImpl = new OSInterfaceImpl();
}

OSInterface::~OSInterface() {
    delete osInterfaceImpl;
}

uint32_t OSInterface::getHwContextId() const {
    return osInterfaceImpl->getHwContextId();
}

uint32_t OSInterface::getDeviceHandle() const {
    return static_cast<uint32_t>(osInterfaceImpl->getDeviceHandle());
}

OSInterface::OSInterfaceImpl::OSInterfaceImpl() = default;

D3DKMT_HANDLE OSInterface::OSInterfaceImpl::getAdapterHandle() const {
    return wddm->getAdapter();
}

D3DKMT_HANDLE OSInterface::OSInterfaceImpl::getDeviceHandle() const {
    return wddm->getDevice();
}

PFND3DKMT_ESCAPE OSInterface::OSInterfaceImpl::getEscapeHandle() const {
    return wddm->getEscapeHandle();
}

uint32_t OSInterface::OSInterfaceImpl::getHwContextId() const {
    if (wddm == nullptr) {
        return 0;
    }
    return wddm->getHwContextId();
}

bool OSInterface::are64kbPagesEnabled() {
    return osEnabled64kbPages;
}

Wddm *OSInterface::OSInterfaceImpl::getWddm() const {
    return wddm.get();
}

void OSInterface::OSInterfaceImpl::setWddm(Wddm *wddm) {
    this->wddm.reset(wddm);
}

HANDLE OSInterface::OSInterfaceImpl::createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                                                 LPCSTR lpName) {
    return SysCalls::createEvent(lpEventAttributes, bManualReset, bInitialState, lpName);
}

BOOL OSInterface::OSInterfaceImpl::closeHandle(HANDLE hObject) {
    return SysCalls::closeHandle(hObject);
}
} // namespace OCLRT
