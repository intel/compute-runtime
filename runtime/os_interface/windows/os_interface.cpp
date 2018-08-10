/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "os_interface.h"

#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/sys_calls.h"

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
