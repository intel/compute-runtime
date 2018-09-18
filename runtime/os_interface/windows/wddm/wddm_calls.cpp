/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include <dxgi.h>

namespace OCLRT {
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
} // namespace OCLRT
