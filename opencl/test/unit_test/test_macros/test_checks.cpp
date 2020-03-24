/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/test_macros/test_checks.h"

#include "opencl/source/cl_device/cl_device.h"

using namespace NEO;

bool TestChecks::supportsSvm(const ClDevice *pClDevice) {
    return supportsSvm(&pClDevice->getDevice());
}
