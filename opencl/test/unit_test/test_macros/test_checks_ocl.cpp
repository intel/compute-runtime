/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "shared/source/device/device_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/kernel/kernel.h"

using namespace NEO;

bool TestChecks::supportsSvm(const ClDevice *pClDevice) {
    return supportsSvm(&pClDevice->getDevice());
}

bool TestChecks::supportsImages(const Context *pContext) {
    return pContext->getDevice(0)->getSharedDeviceInfo().imageSupport;
}

bool TestChecks::supportsOcl21(const Context *pContext) {
    return pContext->getDevice(0)->areOcl21FeaturesEnabled();
}

bool TestChecks::supportsOcl21(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return pHardwareInfo->capabilityTable.supportsOcl21Features;
}

bool TestChecks::supportsDeviceEnqueue(const ClDevice *pClDevice) {
    return pClDevice->getHardwareInfo().capabilityTable.supportsDeviceEnqueue;
}
bool TestChecks::supportsDeviceEnqueue(const Context *pContext) {
    return supportsDeviceEnqueue(pContext->getDevice(0));
}
bool TestChecks::supportsDeviceEnqueue(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return pHardwareInfo->capabilityTable.supportsDeviceEnqueue;
}

bool TestChecks::supportsAuxResolves() {
    KernelInfo kernelInfo{};
    KernelArgInfo argInfo{};
    argInfo.isBuffer = true;
    argInfo.pureStatefulBufferAccess = false;
    kernelInfo.kernelArgInfo.push_back(std::move(argInfo));

    auto &clHwHelper = ClHwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    return clHwHelper.requiresAuxResolves(kernelInfo);
}
