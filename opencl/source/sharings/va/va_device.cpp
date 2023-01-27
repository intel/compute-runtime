/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/va_device.h"

namespace NEO {

VADevice::VADevice() {
}

ClDevice *VADevice::getDeviceFromVA(Platform *pPlatform, VADisplay vaDisplay) {
    return getRootDeviceFromVaDisplay(pPlatform, vaDisplay);
}

VADevice::~VADevice() {
}

} // namespace NEO
