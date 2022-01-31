/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "shared/source/device/device_info.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"

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

bool TestChecks::supportsOcl21(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return (pHardwareInfo->capabilityTable.supportsOcl21Features && pHardwareInfo->capabilityTable.supportsPipes &&
            pHardwareInfo->capabilityTable.supportsIndependentForwardProgress);
}

bool TestChecks::supportsAuxResolves() {
    KernelInfo kernelInfo{};
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = true;

    auto &clHwHelper = ClHwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    return clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo);
}
