/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_interface.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = true;

OSInterface::OSInterface() {
    osInterfaceImpl = new OSInterfaceImpl();
}

OSInterface::~OSInterface() {
    delete osInterfaceImpl;
}

uint32_t OSInterface::getDeviceHandle() const {
    return static_cast<uint32_t>(osInterfaceImpl->getDeviceHandle());
}

void OSInterface::setGmmInputArgs(void *args) {
    this->get()->getWddm()->setGmmInputArg(args);
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
bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId) {
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::move(hwDeviceId), *this));
    if (!wddm->init()) {
        return false;
    }
    memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm.get());
    osInterface = std::make_unique<OSInterface>();
    osInterface->get()->setWddm(wddm.release());
    return true;
}
} // namespace NEO
