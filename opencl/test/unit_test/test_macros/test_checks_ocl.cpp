/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "shared/source/device/device_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"

using namespace NEO;

bool TestChecks::supportsSvm(const ClDevice *pClDevice) {
    return supportsSvm(&pClDevice->getDevice());
}

bool TestChecks::supportsImages(const Context *pContext) {
    return pContext->getDevice(0)->getSharedDeviceInfo().imageSupport;
}

bool TestChecks::supportsOcl21(const Context *pContext) {
    return pContext->getDevice(0)->getEnabledClVersion() >= 21;
}
