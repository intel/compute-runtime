/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/mocks/linux/mock_dxcore.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"

#include <unistd.h>

namespace NEO {

namespace SysCalls {
bool isShutdownInProgress() {
    return false;
}
} // namespace SysCalls

unsigned int readEnablePreemptionRegKey() {
    return 1;
}

Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return nullptr;
}

Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory() {
    return ultDxCoreCreateAdapterFactory;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return nullptr;
}

} // namespace NEO
