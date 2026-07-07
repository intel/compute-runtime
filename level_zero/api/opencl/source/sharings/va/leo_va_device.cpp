/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/va/leo_va_device.h"

namespace NEO {

VADevice::VADevice() {
}

ClDevice *VADevice::getDeviceFromVA(Platform *pPlatform, VADisplay vaDisplay) {
    return getRootDeviceFromVaDisplay(pPlatform, vaDisplay);
}

VADevice::~VADevice() {
}

} // namespace NEO
