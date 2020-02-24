/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"

#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/kernel/kernel.h"

namespace NEO {

bool PreemptionHelper::allowThreadGroupPreemption(const PreemptionFlags &flags) {
    if (flags.flags.disablePerCtxtPreemptionGranularityControl) {
        return false;
    }

    if (flags.flags.usesFencesForReadWriteImages &&
        flags.flags.disableLSQCROPERFforOCL) {
        return false;
    }
    if (flags.flags.schedulerKernel || flags.flags.vmeKernel) {
        return false;
    }

    return true;
}

bool PreemptionHelper::allowMidThreadPreemption(const PreemptionFlags &flags) {
    return (flags.flags.disabledMidThreadPreemptionKernel == 0) &&
           !(flags.flags.vmeKernel && !flags.flags.deviceSupportsVmePreemption);
}

PreemptionMode PreemptionHelper::taskPreemptionMode(PreemptionMode devicePreemptionMode, const PreemptionFlags &flags) {
    if (devicePreemptionMode == PreemptionMode::Disabled) {
        return PreemptionMode::Disabled;
    }

    if (devicePreemptionMode >= PreemptionMode::MidThread &&
        allowMidThreadPreemption(flags)) {
        return PreemptionMode::MidThread;
    }

    if (devicePreemptionMode >= PreemptionMode::ThreadGroup &&
        allowThreadGroupPreemption(flags)) {
        return PreemptionMode::ThreadGroup;
    }

    return PreemptionMode::MidBatch;
};

void PreemptionHelper::setPreemptionLevelFlags(PreemptionFlags &flags, Device &device, Kernel *kernel) {
    if (kernel) {
        flags.flags.disabledMidThreadPreemptionKernel =
            kernel->getKernelInfo().patchInfo.executionEnvironment &&
            kernel->getKernelInfo().patchInfo.executionEnvironment->DisableMidThreadPreemption;
        flags.flags.vmeKernel = kernel->isVmeKernel();
        flags.flags.usesFencesForReadWriteImages =
            kernel->getKernelInfo().patchInfo.executionEnvironment &&
            kernel->getKernelInfo().patchInfo.executionEnvironment->UsesFencesForReadWriteImages;
        flags.flags.schedulerKernel = kernel->isSchedulerKernel;
    }
    flags.flags.deviceSupportsVmePreemption = device.getDeviceInfo().vmeAvcSupportsPreemption;
    flags.flags.disablePerCtxtPreemptionGranularityControl = device.getHardwareInfo().workaroundTable.waDisablePerCtxtPreemptionGranularityControl;
    flags.flags.disableLSQCROPERFforOCL = device.getHardwareInfo().workaroundTable.waDisableLSQCROPERFforOCL;
}

PreemptionMode PreemptionHelper::taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo) {
    PreemptionMode devMode = device.getPreemptionMode();

    for (const auto &di : multiDispatchInfo) {
        auto kernel = di.getKernel();

        PreemptionFlags flags = {};
        setPreemptionLevelFlags(flags, device, kernel);

        PreemptionMode taskMode = taskPreemptionMode(devMode, flags);
        if (devMode > taskMode) {
            devMode = taskMode;
        }
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stdout, "devMode = %d, taskMode = %d.\n",
                         static_cast<int>(device.getPreemptionMode()), static_cast<int>(taskMode));
    }
    return devMode;
}

void PreemptionHelper::adjustDefaultPreemptionMode(RuntimeCapabilityTable &deviceCapabilities, bool allowMidThread, bool allowThreadGroup, bool allowMidBatch) {
    if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::MidThread &&
        allowMidThread) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::MidThread;
    } else if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::ThreadGroup &&
               allowThreadGroup) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    } else if (deviceCapabilities.defaultPreemptionMode >= PreemptionMode::MidBatch &&
               allowMidBatch) {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::MidBatch;
    } else {
        deviceCapabilities.defaultPreemptionMode = PreemptionMode::Disabled;
    }
}

PreemptionMode PreemptionHelper::getDefaultPreemptionMode(const HardwareInfo &hwInfo) {
    return DebugManager.flags.ForcePreemptionMode.get() == -1
               ? hwInfo.capabilityTable.defaultPreemptionMode
               : static_cast<PreemptionMode>(DebugManager.flags.ForcePreemptionMode.get());
}

} // namespace NEO
