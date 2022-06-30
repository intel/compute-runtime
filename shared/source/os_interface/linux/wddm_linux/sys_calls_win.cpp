/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <unistd.h>

namespace NEO {

bool isShutdownInProgress() {
    return false;
}

unsigned int getPid() {
    return static_cast<uint32_t>(getpid());
}

unsigned int readEnablePreemptionRegKey() {
    return 1;
}

Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return nullptr;
}

Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory() {
    return nullptr;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return nullptr;
}

} // namespace NEO
