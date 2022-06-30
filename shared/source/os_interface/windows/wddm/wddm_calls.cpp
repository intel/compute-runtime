/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <dxcore.h>
#include <dxgi.h>

namespace NEO {
Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return CreateDXGIFactory1;
}

Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory() {
    return nullptr;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return GetSystemInfo;
}

} // namespace NEO
