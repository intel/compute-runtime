/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include <level_zero/zer_api.h>

namespace L0 {
ze_context_handle_t ZE_APICALL zerGetDefaultContext() {
    return L0::DriverHandle::fromHandle(L0::globalDriverHandles->front())->getDefaultContext();
}

ze_result_t ZE_APICALL zerGetLastErrorDescription(const char **ppString) {
    return L0::DriverHandle::fromHandle(L0::globalDriverHandles->front())->getErrorDescription(ppString);
}

uint32_t ZE_APICALL zerTranslateDeviceHandleToIdentifier(ze_device_handle_t device) {
    if (!device) {
        auto driverHandle = static_cast<L0::DriverHandleImp *>(L0::globalDriverHandles->front());
        driverHandle->setErrorDescription("Invalid device handle");
        return std::numeric_limits<uint32_t>::max();
    }
    return L0::Device::fromHandle(device)->getIdentifier();
}

ze_device_handle_t ZE_APICALL zerTranslateIdentifierToDeviceHandle(uint32_t identifier) {
    auto driverHandle = static_cast<L0::DriverHandleImp *>(L0::globalDriverHandles->front());
    if (identifier >= driverHandle->devicesToExpose.size()) {
        driverHandle->setErrorDescription("Invalid device identifier");
        return nullptr;
    }
    return driverHandle->devicesToExpose[identifier];
}
} // namespace L0

extern "C" {
ZE_APIEXPORT ze_context_handle_t ZE_APICALL zerGetDefaultContext() {
    return L0::zerGetDefaultContext();
}

ZE_APIEXPORT ze_result_t ZE_APICALL zerGetLastErrorDescription(const char **ppString) {
    return L0::zerGetLastErrorDescription(ppString);
}

ZE_APIEXPORT uint32_t ZE_APICALL zerTranslateDeviceHandleToIdentifier(ze_device_handle_t device) {
    return L0::zerTranslateDeviceHandleToIdentifier(device);
}

ZE_APIEXPORT ze_device_handle_t ZE_APICALL zerTranslateIdentifierToDeviceHandle(uint32_t identifier) {
    return L0::zerTranslateIdentifierToDeviceHandle(identifier);
}
}