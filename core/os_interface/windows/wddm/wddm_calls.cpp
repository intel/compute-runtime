/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/wddm/wddm.h"

#include <dxgi.h>

namespace NEO {
Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return CreateDXGIFactory;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return GetSystemInfo;
}

Wddm::VirtualFreeFcn getVirtualFree() {
    return VirtualFree;
}

Wddm::VirtualAllocFcn getVirtualAlloc() {
    return VirtualAlloc;
}
} // namespace NEO
