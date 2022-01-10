/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_preemption_helper.h"

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
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stdout, "devMode = %d, taskMode = %d.\n",
                           static_cast<int>(device.getPreemptionMode()), static_cast<int>(taskMode));
    }
    return devMode;
}
} // namespace NEO
