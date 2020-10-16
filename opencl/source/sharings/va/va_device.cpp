/*
 * Copyright (C) 2020 Intel Corporation
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
    return pPlatform->getClDevice(0);
}

VADevice::~VADevice() {
}

} // namespace NEO
