/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_preemption_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"

#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/dispatch_info.h"

namespace NEO {

PreemptionMode ClPreemptionHelper::taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo) {
    PreemptionMode devMode = device.getPreemptionMode();

    for (const auto &di : multiDispatchInfo) {
        auto kernel = di.getKernel();

        const KernelDescriptor *kernelDescriptor = nullptr;
        if (kernel != nullptr) {
            kernelDescriptor = &kernel->getDescriptor();
        }

        PreemptionFlags flags = PreemptionHelper::createPreemptionLevelFlags(device, kernelDescriptor);
        PreemptionMode taskMode = PreemptionHelper::taskPreemptionMode(devMode, flags);
        if (devMode > taskMode) {
            devMode = taskMode;
        }
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stdout, "devMode = %d, taskMode = %d.\n",
                           static_cast<int>(device.getPreemptionMode()), static_cast<int>(taskMode));
    }
    return devMode;
}

void ClPreemptionHelper::overrideMidThreadPreemptionSupport(Context &context, bool value) {
    for (auto device : context.getDevices()) {
        auto &clGfxCoreHelper = device->getDevice().getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
        auto isPreemptionDisabled = debugManager.flags.ForcePreemptionMode.get() == static_cast<int32_t>(PreemptionMode::Disabled);
        if (clGfxCoreHelper.isLimitationForPreemptionNeeded() && !isPreemptionDisabled) {
            device->getDevice().overridePreemptionMode(value ? PreemptionMode::Disabled : PreemptionMode::MidThread);
        }
    }
}
} // namespace NEO
