/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/va/va_device.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"

namespace NEO {

VADevice::VADevice() {
}

ClDevice *VADevice::getDeviceFromVA(Platform *pPlatform, VADisplay vaDisplay) {
    return getRootDeviceFromVaDisplay(pPlatform, vaDisplay);
}

VADevice::~VADevice() {
}

} // namespace NEO
