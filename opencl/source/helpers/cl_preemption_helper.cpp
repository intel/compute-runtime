/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_preemption_helper.h"

#include "opencl/source/helpers/dispatch_info.h"

namespace NEO {

void ClPreemptionHelper::setPreemptionLevelFlags(PreemptionFlags &flags, Device &device, Kernel *kernel) {
    if (kernel) {
        const auto &kernelDescriptor = kernel->getKernelInfo().kernelDescriptor;
        flags.flags.disabledMidThreadPreemptionKernel = kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption;
        flags.flags.vmeKernel = kernel->isVmeKernel();
        flags.flags.usesFencesForReadWriteImages = kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages;
        flags.flags.schedulerKernel = kernel->isSchedulerKernel;
    }
    flags.flags.deviceSupportsVmePreemption = device.getDeviceInfo().vmeAvcSupportsPreemption;
    flags.flags.disablePerCtxtPreemptionGranularityControl = device.getHardwareInfo().workaroundTable.waDisablePerCtxtPreemptionGranularityControl;
    flags.flags.disableLSQCROPERFforOCL = device.getHardwareInfo().workaroundTable.waDisableLSQCROPERFforOCL;
}

PreemptionMode ClPreemptionHelper::taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo) {
    PreemptionMode devMode = device.getPreemptionMode();

    for (const auto &di : multiDispatchInfo) {
        auto kernel = di.getKernel();

        PreemptionFlags flags = {};
        setPreemptionLevelFlags(flags, device, kernel);

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
