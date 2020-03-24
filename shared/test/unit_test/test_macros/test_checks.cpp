/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/test_macros/test_checks.h"

#include "shared/source/device/device.h"

using namespace NEO;

bool TestChecks::supportsSvm(const HardwareInfo *pHardwareInfo) {
    return pHardwareInfo->capabilityTable.ftrSvm;
}
bool TestChecks::supportsSvm(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsSvm(pHardwareInfo.get());
}
bool TestChecks::supportsSvm(const Device *pDevice) {
    return supportsSvm(&pDevice->getHardwareInfo());
}
